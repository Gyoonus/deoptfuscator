/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "intrinsics.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/utils.h"
#include "class_linker.h"
#include "dex/invoke_type.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "mirror/dex_cache-inl.h"
#include "nodes.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

// Check that intrinsic enum values fit within space set aside in ArtMethod modifier flags.
#define CHECK_INTRINSICS_ENUM_VALUES(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  static_assert( \
      static_cast<uint32_t>(Intrinsics::k ## Name) <= (kAccIntrinsicBits >> CTZ(kAccIntrinsicBits)), \
      "Instrinsics enumeration space overflow.");
#include "intrinsics_list.h"
  INTRINSICS_LIST(CHECK_INTRINSICS_ENUM_VALUES)
#undef INTRINSICS_LIST
#undef CHECK_INTRINSICS_ENUM_VALUES

// Function that returns whether an intrinsic is static/direct or virtual.
static inline InvokeType GetIntrinsicInvokeType(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kInterface;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return IsStatic;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kInterface;
}

// Function that returns whether an intrinsic needs an environment or not.
static inline IntrinsicNeedsEnvironmentOrCache NeedsEnvironmentOrCache(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kNeedsEnvironmentOrCache;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return NeedsEnvironmentOrCache;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kNeedsEnvironmentOrCache;
}

// Function that returns whether an intrinsic has side effects.
static inline IntrinsicSideEffects GetSideEffects(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kAllSideEffects;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return SideEffects;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kAllSideEffects;
}

// Function that returns whether an intrinsic can throw exceptions.
static inline IntrinsicExceptions GetExceptions(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kCanThrow;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return Exceptions;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kCanThrow;
}

static bool CheckInvokeType(Intrinsics intrinsic, HInvoke* invoke)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Whenever the intrinsic is marked as static, report an error if we find an InvokeVirtual.
  //
  // Whenever the intrinsic is marked as direct and we find an InvokeVirtual, a devirtualization
  // failure occured. We might be in a situation where we have inlined a method that calls an
  // intrinsic, but that method is in a different dex file on which we do not have a
  // verified_method that would have helped the compiler driver sharpen the call. In that case,
  // make sure that the intrinsic is actually for some final method (or in a final class), as
  // otherwise the intrinsics setup is broken.
  //
  // For the last direction, we have intrinsics for virtual functions that will perform a check
  // inline. If the precise type is known, however, the instruction will be sharpened to an
  // InvokeStaticOrDirect.
  InvokeType intrinsic_type = GetIntrinsicInvokeType(intrinsic);
  InvokeType invoke_type = invoke->GetInvokeType();

  switch (intrinsic_type) {
    case kStatic:
      return (invoke_type == kStatic);

    case kDirect:
      if (invoke_type == kDirect) {
        return true;
      }
      if (invoke_type == kVirtual) {
        ArtMethod* art_method = invoke->GetResolvedMethod();
        return (art_method->IsFinal() || art_method->GetDeclaringClass()->IsFinal());
      }
      return false;

    case kVirtual:
      // Call might be devirtualized.
      return (invoke_type == kVirtual || invoke_type == kDirect || invoke_type == kInterface);

    case kSuper:
    case kInterface:
    case kPolymorphic:
      return false;
  }
  LOG(FATAL) << "Unknown intrinsic invoke type: " << intrinsic_type;
  UNREACHABLE();
}

bool IntrinsicsRecognizer::Recognize(HInvoke* invoke,
                                     ArtMethod* art_method,
                                     /*out*/ bool* wrong_invoke_type) {
  if (art_method == nullptr) {
    art_method = invoke->GetResolvedMethod();
  }
  *wrong_invoke_type = false;
  if (art_method == nullptr || !art_method->IsIntrinsic()) {
    return false;
  }

  // TODO: b/65872996 The intent is that polymorphic signature methods should
  // be compiler intrinsics. At present, they are only interpreter intrinsics.
  if (art_method->IsPolymorphicSignature()) {
    return false;
  }

  Intrinsics intrinsic = static_cast<Intrinsics>(art_method->GetIntrinsic());
  if (CheckInvokeType(intrinsic, invoke) == false) {
    *wrong_invoke_type = true;
    return false;
  }

  invoke->SetIntrinsic(intrinsic,
                       NeedsEnvironmentOrCache(intrinsic),
                       GetSideEffects(intrinsic),
                       GetExceptions(intrinsic));
  return true;
}

void IntrinsicsRecognizer::Run() {
  ScopedObjectAccess soa(Thread::Current());
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      HInstruction* inst = inst_it.Current();
      if (inst->IsInvoke()) {
        bool wrong_invoke_type = false;
        if (Recognize(inst->AsInvoke(), /* art_method */ nullptr, &wrong_invoke_type)) {
          MaybeRecordStat(stats_, MethodCompilationStat::kIntrinsicRecognized);
        } else if (wrong_invoke_type) {
          LOG(WARNING)
              << "Found an intrinsic with unexpected invoke type: "
              << inst->AsInvoke()->GetResolvedMethod()->PrettyMethod() << " "
              << inst->DebugName();
        }
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const Intrinsics& intrinsic) {
  switch (intrinsic) {
    case Intrinsics::kNone:
      os << "None";
      break;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      os << # Name; \
      break;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return os;
}

void IntrinsicVisitor::ComputeIntegerValueOfLocations(HInvoke* invoke,
                                                      CodeGenerator* codegen,
                                                      Location return_location,
                                                      Location first_argument_location) {
  if (Runtime::Current()->IsAotCompiler()) {
    if (codegen->GetCompilerOptions().IsBootImage() ||
        codegen->GetCompilerOptions().GetCompilePic()) {
      // TODO(ngeoffray): Support boot image compilation.
      return;
    }
  }

  IntegerValueOfInfo info = ComputeIntegerValueOfInfo();

  // Most common case is that we have found all we needed (classes are initialized
  // and in the boot image). Bail if not.
  if (info.integer_cache == nullptr ||
      info.integer == nullptr ||
      info.cache == nullptr ||
      info.value_offset == 0 ||
      // low and high cannot be 0, per the spec.
      info.low == 0 ||
      info.high == 0) {
    LOG(INFO) << "Integer.valueOf will not be optimized";
    return;
  }

  // The intrinsic will call if it needs to allocate a j.l.Integer.
  LocationSummary* locations = new (invoke->GetBlock()->GetGraph()->GetAllocator()) LocationSummary(
      invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  if (!invoke->InputAt(0)->IsConstant()) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
  locations->AddTemp(first_argument_location);
  locations->SetOut(return_location);
}

IntrinsicVisitor::IntegerValueOfInfo IntrinsicVisitor::ComputeIntegerValueOfInfo() {
  // Note that we could cache all of the data looked up here. but there's no good
  // location for it. We don't want to add it to WellKnownClasses, to avoid creating global
  // jni values. Adding it as state to the compiler singleton seems like wrong
  // separation of concerns.
  // The need for this data should be pretty rare though.

  // The most common case is that the classes are in the boot image and initialized,
  // which is easy to generate code for. We bail if not.
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  gc::Heap* heap = runtime->GetHeap();
  IntegerValueOfInfo info;
  info.integer_cache = class_linker->FindSystemClass(self, "Ljava/lang/Integer$IntegerCache;");
  if (info.integer_cache == nullptr) {
    self->ClearException();
    return info;
  }
  if (!heap->ObjectIsInBootImageSpace(info.integer_cache) || !info.integer_cache->IsInitialized()) {
    // Optimization only works if the class is initialized and in the boot image.
    return info;
  }
  info.integer = class_linker->FindSystemClass(self, "Ljava/lang/Integer;");
  if (info.integer == nullptr) {
    self->ClearException();
    return info;
  }
  if (!heap->ObjectIsInBootImageSpace(info.integer) || !info.integer->IsInitialized()) {
    // Optimization only works if the class is initialized and in the boot image.
    return info;
  }

  ArtField* field = info.integer_cache->FindDeclaredStaticField("cache", "[Ljava/lang/Integer;");
  if (field == nullptr) {
    return info;
  }
  info.cache = static_cast<mirror::ObjectArray<mirror::Object>*>(
      field->GetObject(info.integer_cache).Ptr());
  if (info.cache == nullptr) {
    return info;
  }

  if (!heap->ObjectIsInBootImageSpace(info.cache)) {
    // Optimization only works if the object is in the boot image.
    return info;
  }

  field = info.integer->FindDeclaredInstanceField("value", "I");
  if (field == nullptr) {
    return info;
  }
  info.value_offset = field->GetOffset().Int32Value();

  field = info.integer_cache->FindDeclaredStaticField("low", "I");
  if (field == nullptr) {
    return info;
  }
  info.low = field->GetInt(info.integer_cache);

  field = info.integer_cache->FindDeclaredStaticField("high", "I");
  if (field == nullptr) {
    return info;
  }
  info.high = field->GetInt(info.integer_cache);

  DCHECK_EQ(info.cache->GetLength(), info.high - info.low + 1);
  return info;
}

}  // namespace art
