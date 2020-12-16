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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_H_

#include "code_generator.h"
#include "nodes.h"
#include "optimization.h"
#include "parallel_move_resolver.h"

namespace art {

class CompilerDriver;
class DexFile;

// Positive floating-point infinities.
static constexpr uint32_t kPositiveInfinityFloat = 0x7f800000U;
static constexpr uint64_t kPositiveInfinityDouble = UINT64_C(0x7ff0000000000000);

static constexpr uint32_t kNanFloat = 0x7fc00000U;
static constexpr uint64_t kNanDouble = 0x7ff8000000000000;

// Recognize intrinsics from HInvoke nodes.
class IntrinsicsRecognizer : public HOptimization {
 public:
  IntrinsicsRecognizer(HGraph* graph,
                       OptimizingCompilerStats* stats,
                       const char* name = kIntrinsicsRecognizerPassName)
      : HOptimization(graph, name, stats) {}

  void Run() OVERRIDE;

  // Static helper that recognizes intrinsic call. Returns true on success.
  // If it fails due to invoke type mismatch, wrong_invoke_type is set.
  // Useful to recognize intrinsics on individual calls outside this full pass.
  static bool Recognize(HInvoke* invoke, ArtMethod* method, /*out*/ bool* wrong_invoke_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr const char* kIntrinsicsRecognizerPassName = "intrinsics_recognition";

 private:
  DISALLOW_COPY_AND_ASSIGN(IntrinsicsRecognizer);
};

class IntrinsicVisitor : public ValueObject {
 public:
  virtual ~IntrinsicVisitor() {}

  // Dispatch logic.

  void Dispatch(HInvoke* invoke) {
    switch (invoke->GetIntrinsic()) {
      case Intrinsics::kNone:
        return;
#define OPTIMIZING_INTRINSICS(Name, ...) \
      case Intrinsics::k ## Name: \
        Visit ## Name(invoke);    \
        return;
#include "intrinsics_list.h"
        INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

      // Do not put a default case. That way the compiler will complain if we missed a case.
    }
  }

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, ...) \
  virtual void Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) { \
  }
#include "intrinsics_list.h"
  INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

  static void MoveArguments(HInvoke* invoke,
                            CodeGenerator* codegen,
                            InvokeDexCallingConventionVisitor* calling_convention_visitor) {
    if (kIsDebugBuild && invoke->IsInvokeStaticOrDirect()) {
      HInvokeStaticOrDirect* invoke_static_or_direct = invoke->AsInvokeStaticOrDirect();
      // Explicit clinit checks triggered by static invokes must have been
      // pruned by art::PrepareForRegisterAllocation.
      DCHECK(!invoke_static_or_direct->IsStaticWithExplicitClinitCheck());
    }

    if (invoke->GetNumberOfArguments() == 0) {
      // No argument to move.
      return;
    }

    LocationSummary* locations = invoke->GetLocations();

    // We're moving potentially two or more locations to locations that could overlap, so we need
    // a parallel move resolver.
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());

    for (size_t i = 0; i < invoke->GetNumberOfArguments(); i++) {
      HInstruction* input = invoke->InputAt(i);
      Location cc_loc = calling_convention_visitor->GetNextLocation(input->GetType());
      Location actual_loc = locations->InAt(i);

      parallel_move.AddMove(actual_loc, cc_loc, input->GetType(), nullptr);
    }

    codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
  }

  static void ComputeIntegerValueOfLocations(HInvoke* invoke,
                                             CodeGenerator* codegen,
                                             Location return_location,
                                             Location first_argument_location);

  // Temporary data structure for holding Integer.valueOf useful data. We only
  // use it if the mirror::Class* are in the boot image, so it is fine to keep raw
  // mirror::Class pointers in this structure.
  struct IntegerValueOfInfo {
    IntegerValueOfInfo()
        : integer_cache(nullptr),
          integer(nullptr),
          cache(nullptr),
          low(0),
          high(0),
          value_offset(0) {}

    // The java.lang.IntegerCache class.
    mirror::Class* integer_cache;
    // The java.lang.Integer class.
    mirror::Class* integer;
    // Value of java.lang.IntegerCache#cache.
    mirror::ObjectArray<mirror::Object>* cache;
    // Value of java.lang.IntegerCache#low.
    int32_t low;
    // Value of java.lang.IntegerCache#high.
    int32_t high;
    // The offset of java.lang.Integer.value.
    int32_t value_offset;
  };

  static IntegerValueOfInfo ComputeIntegerValueOfInfo();

 protected:
  IntrinsicVisitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IntrinsicVisitor);
};

#define GENERIC_OPTIMIZATION(name, bit)                \
public:                                                \
void Set##name() { SetBit(k##name); }                  \
bool Get##name() const { return IsBitSet(k##name); }   \
private:                                               \
static constexpr size_t k##name = bit

class IntrinsicOptimizations : public ValueObject {
 public:
  explicit IntrinsicOptimizations(HInvoke* invoke)
      : value_(invoke->GetIntrinsicOptimizations()) {}
  explicit IntrinsicOptimizations(const HInvoke& invoke)
      : value_(invoke.GetIntrinsicOptimizations()) {}

  static constexpr int kNumberOfGenericOptimizations = 2;
  GENERIC_OPTIMIZATION(DoesNotNeedDexCache, 0);
  GENERIC_OPTIMIZATION(DoesNotNeedEnvironment, 1);

 protected:
  bool IsBitSet(uint32_t bit) const {
    DCHECK_LT(bit, sizeof(uint32_t) * kBitsPerByte);
    return (*value_ & (1 << bit)) != 0u;
  }

  void SetBit(uint32_t bit) {
    DCHECK_LT(bit, sizeof(uint32_t) * kBitsPerByte);
    *(const_cast<uint32_t* const>(value_)) |= (1 << bit);
  }

 private:
  const uint32_t* const value_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicOptimizations);
};

#undef GENERIC_OPTIMIZATION

#define INTRINSIC_OPTIMIZATION(name, bit)                             \
public:                                                               \
void Set##name() { SetBit(k##name); }                                 \
bool Get##name() const { return IsBitSet(k##name); }                  \
private:                                                              \
static constexpr size_t k##name = (bit) + kNumberOfGenericOptimizations

class StringEqualsOptimizations : public IntrinsicOptimizations {
 public:
  explicit StringEqualsOptimizations(HInvoke* invoke) : IntrinsicOptimizations(invoke) {}

  INTRINSIC_OPTIMIZATION(ArgumentNotNull, 0);
  INTRINSIC_OPTIMIZATION(ArgumentIsString, 1);
  INTRINSIC_OPTIMIZATION(NoReadBarrierForStringClass, 2);

 private:
  DISALLOW_COPY_AND_ASSIGN(StringEqualsOptimizations);
};

class SystemArrayCopyOptimizations : public IntrinsicOptimizations {
 public:
  explicit SystemArrayCopyOptimizations(HInvoke* invoke) : IntrinsicOptimizations(invoke) {}

  INTRINSIC_OPTIMIZATION(SourceIsNotNull, 0);
  INTRINSIC_OPTIMIZATION(DestinationIsNotNull, 1);
  INTRINSIC_OPTIMIZATION(DestinationIsSource, 2);
  INTRINSIC_OPTIMIZATION(CountIsSourceLength, 3);
  INTRINSIC_OPTIMIZATION(CountIsDestinationLength, 4);
  INTRINSIC_OPTIMIZATION(DoesNotNeedTypeCheck, 5);
  INTRINSIC_OPTIMIZATION(DestinationIsTypedObjectArray, 6);
  INTRINSIC_OPTIMIZATION(DestinationIsNonPrimitiveArray, 7);
  INTRINSIC_OPTIMIZATION(DestinationIsPrimitiveArray, 8);
  INTRINSIC_OPTIMIZATION(SourceIsNonPrimitiveArray, 9);
  INTRINSIC_OPTIMIZATION(SourceIsPrimitiveArray, 10);

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemArrayCopyOptimizations);
};

#undef INTRISIC_OPTIMIZATION

//
// Macros for use in the intrinsics code generators.
//

// Defines an unimplemented intrinsic: that is, a method call that is recognized as an
// intrinsic to exploit e.g. no side-effects or exceptions, but otherwise not handled
// by this architecture-specific intrinsics code generator. Eventually it is implemented
// as a true method call.
#define UNIMPLEMENTED_INTRINSIC(Arch, Name)                                               \
void IntrinsicLocationsBuilder ## Arch::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) { \
}                                                                                         \
void IntrinsicCodeGenerator ## Arch::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) {    \
}

// Defines a list of unreached intrinsics: that is, method calls that are recognized as
// an intrinsic, and then always converted into HIR instructions before they reach any
// architecture-specific intrinsics code generator.
#define UNREACHABLE_INTRINSIC(Arch, Name)                                \
void IntrinsicLocationsBuilder ## Arch::Visit ## Name(HInvoke* invoke) { \
  LOG(FATAL) << "Unreachable: intrinsic " << invoke->GetIntrinsic()      \
             << " should have been converted to HIR";                    \
}                                                                        \
void IntrinsicCodeGenerator ## Arch::Visit ## Name(HInvoke* invoke) {    \
  LOG(FATAL) << "Unreachable: intrinsic " << invoke->GetIntrinsic()      \
             << " should have been converted to HIR";                    \
}
#define UNREACHABLE_INTRINSICS(Arch)                            \
UNREACHABLE_INTRINSIC(Arch, FloatFloatToIntBits)                \
UNREACHABLE_INTRINSIC(Arch, DoubleDoubleToLongBits)             \
UNREACHABLE_INTRINSIC(Arch, FloatIsNaN)                         \
UNREACHABLE_INTRINSIC(Arch, DoubleIsNaN)                        \
UNREACHABLE_INTRINSIC(Arch, IntegerRotateLeft)                  \
UNREACHABLE_INTRINSIC(Arch, LongRotateLeft)                     \
UNREACHABLE_INTRINSIC(Arch, IntegerRotateRight)                 \
UNREACHABLE_INTRINSIC(Arch, LongRotateRight)                    \
UNREACHABLE_INTRINSIC(Arch, IntegerCompare)                     \
UNREACHABLE_INTRINSIC(Arch, LongCompare)                        \
UNREACHABLE_INTRINSIC(Arch, IntegerSignum)                      \
UNREACHABLE_INTRINSIC(Arch, LongSignum)                         \
UNREACHABLE_INTRINSIC(Arch, StringCharAt)                       \
UNREACHABLE_INTRINSIC(Arch, StringIsEmpty)                      \
UNREACHABLE_INTRINSIC(Arch, StringLength)                       \
UNREACHABLE_INTRINSIC(Arch, UnsafeLoadFence)                    \
UNREACHABLE_INTRINSIC(Arch, UnsafeStoreFence)                   \
UNREACHABLE_INTRINSIC(Arch, UnsafeFullFence)                    \
UNREACHABLE_INTRINSIC(Arch, VarHandleFullFence)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleAcquireFence)              \
UNREACHABLE_INTRINSIC(Arch, VarHandleReleaseFence)              \
UNREACHABLE_INTRINSIC(Arch, VarHandleLoadLoadFence)             \
UNREACHABLE_INTRINSIC(Arch, VarHandleStoreStoreFence)           \
UNREACHABLE_INTRINSIC(Arch, MethodHandleInvokeExact)            \
UNREACHABLE_INTRINSIC(Arch, MethodHandleInvoke)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleCompareAndExchange)        \
UNREACHABLE_INTRINSIC(Arch, VarHandleCompareAndExchangeAcquire) \
UNREACHABLE_INTRINSIC(Arch, VarHandleCompareAndExchangeRelease) \
UNREACHABLE_INTRINSIC(Arch, VarHandleCompareAndSet)             \
UNREACHABLE_INTRINSIC(Arch, VarHandleGet)                       \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAcquire)                \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndAdd)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndAddAcquire)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndAddRelease)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseAnd)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseAndAcquire)   \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseAndRelease)   \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseOr)           \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseOrAcquire)    \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseOrRelease)    \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseXor)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseXorAcquire)   \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndBitwiseXorRelease)   \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndSet)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndSetAcquire)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetAndSetRelease)          \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetOpaque)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleGetVolatile)               \
UNREACHABLE_INTRINSIC(Arch, VarHandleSet)                       \
UNREACHABLE_INTRINSIC(Arch, VarHandleSetOpaque)                 \
UNREACHABLE_INTRINSIC(Arch, VarHandleSetRelease)                \
UNREACHABLE_INTRINSIC(Arch, VarHandleSetVolatile)               \
UNREACHABLE_INTRINSIC(Arch, VarHandleWeakCompareAndSet)         \
UNREACHABLE_INTRINSIC(Arch, VarHandleWeakCompareAndSetAcquire)  \
UNREACHABLE_INTRINSIC(Arch, VarHandleWeakCompareAndSetPlain)    \
UNREACHABLE_INTRINSIC(Arch, VarHandleWeakCompareAndSetRelease)

template <typename IntrinsicLocationsBuilder, typename Codegenerator>
bool IsCallFreeIntrinsic(HInvoke* invoke, Codegenerator* codegen) {
  if (invoke->GetIntrinsic() != Intrinsics::kNone) {
    // This invoke may have intrinsic code generation defined. However, we must
    // now also determine if this code generation is truly there and call-free
    // (not unimplemented, no bail on instruction features, or call on slow path).
    // This is done by actually calling the locations builder on the instruction
    // and clearing out the locations once result is known. We assume this
    // call only has creating locations as side effects!
    // TODO: Avoid wasting Arena memory.
    IntrinsicLocationsBuilder builder(codegen);
    bool success = builder.TryDispatch(invoke) && !invoke->GetLocations()->CanCall();
    invoke->SetLocations(nullptr);
    return success;
  }
  return false;
}

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_H_
