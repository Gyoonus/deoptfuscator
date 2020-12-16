/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "method_handles-inl.h"

#include "android-base/stringprintf.h"

#include "common_dex_operations.h"
#include "jvalue-inl.h"
#include "jvalue.h"
#include "mirror/emulated_stack_frame.h"
#include "mirror/method_handle_impl-inl.h"
#include "mirror/method_type.h"
#include "mirror/var_handle.h"
#include "reflection-inl.h"
#include "reflection.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

namespace {

#define PRIMITIVES_LIST(V) \
  V(Primitive::kPrimBoolean, Boolean, Boolean, Z) \
  V(Primitive::kPrimByte, Byte, Byte, B)          \
  V(Primitive::kPrimChar, Char, Character, C)     \
  V(Primitive::kPrimShort, Short, Short, S)       \
  V(Primitive::kPrimInt, Int, Integer, I)         \
  V(Primitive::kPrimLong, Long, Long, J)          \
  V(Primitive::kPrimFloat, Float, Float, F)       \
  V(Primitive::kPrimDouble, Double, Double, D)

// Assigns |type| to the primitive type associated with |klass|. Returns
// true iff. |klass| was a boxed type (Integer, Long etc.), false otherwise.
bool GetUnboxedPrimitiveType(ObjPtr<mirror::Class> klass, Primitive::Type* type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  std::string storage;
  const char* descriptor = klass->GetDescriptor(&storage);
  static const char kJavaLangPrefix[] = "Ljava/lang/";
  static const size_t kJavaLangPrefixSize = sizeof(kJavaLangPrefix) - 1;
  if (strncmp(descriptor, kJavaLangPrefix, kJavaLangPrefixSize) != 0) {
    return false;
  }

  descriptor += kJavaLangPrefixSize;
#define LOOKUP_PRIMITIVE(primitive, _, java_name, ___) \
  if (strcmp(descriptor, #java_name ";") == 0) {       \
    *type = primitive;                                 \
    return true;                                       \
  }

  PRIMITIVES_LIST(LOOKUP_PRIMITIVE);
#undef LOOKUP_PRIMITIVE
  return false;
}

ObjPtr<mirror::Class> GetBoxedPrimitiveClass(Primitive::Type type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  jmethodID m = nullptr;
  switch (type) {
#define CASE_PRIMITIVE(primitive, _, java_name, __)              \
    case primitive:                                              \
      m = WellKnownClasses::java_lang_ ## java_name ## _valueOf; \
      break;
    PRIMITIVES_LIST(CASE_PRIMITIVE);
#undef CASE_PRIMITIVE
    case Primitive::Type::kPrimNot:
    case Primitive::Type::kPrimVoid:
      return nullptr;
  }
  return jni::DecodeArtMethod(m)->GetDeclaringClass();
}

bool GetUnboxedTypeAndValue(ObjPtr<mirror::Object> o, Primitive::Type* type, JValue* value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ObjPtr<mirror::Class> klass = o->GetClass();
  ArtField* primitive_field = &klass->GetIFieldsPtr()->At(0);
#define CASE_PRIMITIVE(primitive, abbrev, _, shorthand)         \
  if (klass == GetBoxedPrimitiveClass(primitive)) {             \
    *type = primitive;                                          \
    value->Set ## shorthand(primitive_field->Get ## abbrev(o)); \
    return true;                                                \
  }
  PRIMITIVES_LIST(CASE_PRIMITIVE)
#undef CASE_PRIMITIVE
  return false;
}

inline bool IsReferenceType(Primitive::Type type) {
  return type == Primitive::kPrimNot;
}

inline bool IsPrimitiveType(Primitive::Type type) {
  return !IsReferenceType(type);
}

}  // namespace

bool IsParameterTypeConvertible(ObjPtr<mirror::Class> from, ObjPtr<mirror::Class> to)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This function returns true if there's any conceivable conversion
  // between |from| and |to|. It's expected this method will be used
  // to determine if a WrongMethodTypeException should be raised. The
  // decision logic follows the documentation for MethodType.asType().
  if (from == to) {
    return true;
  }

  Primitive::Type from_primitive = from->GetPrimitiveType();
  Primitive::Type to_primitive = to->GetPrimitiveType();
  DCHECK(from_primitive != Primitive::Type::kPrimVoid);
  DCHECK(to_primitive != Primitive::Type::kPrimVoid);

  // If |to| and |from| are references.
  if (IsReferenceType(from_primitive) && IsReferenceType(to_primitive)) {
    // Assignability is determined during parameter conversion when
    // invoking the associated method handle.
    return true;
  }

  // If |to| and |from| are primitives and a widening conversion exists.
  if (Primitive::IsWidenable(from_primitive, to_primitive)) {
    return true;
  }

  // If |to| is a reference and |from| is a primitive, then boxing conversion.
  if (IsReferenceType(to_primitive) && IsPrimitiveType(from_primitive)) {
    return to->IsAssignableFrom(GetBoxedPrimitiveClass(from_primitive));
  }

  // If |from| is a reference and |to| is a primitive, then unboxing conversion.
  if (IsPrimitiveType(to_primitive) && IsReferenceType(from_primitive)) {
    if (from->DescriptorEquals("Ljava/lang/Object;")) {
      // Object might be converted into a primitive during unboxing.
      return true;
    }

    if (Primitive::IsNumericType(to_primitive) && from->DescriptorEquals("Ljava/lang/Number;")) {
      // Number might be unboxed into any of the number primitive types.
      return true;
    }

    Primitive::Type unboxed_type;
    if (GetUnboxedPrimitiveType(from, &unboxed_type)) {
      if (unboxed_type == to_primitive) {
        // Straightforward unboxing conversion such as Boolean => boolean.
        return true;
      }

      // Check if widening operations for numeric primitives would work,
      // such as Byte => byte => long.
      return Primitive::IsWidenable(unboxed_type, to_primitive);
    }
  }

  return false;
}

bool IsReturnTypeConvertible(ObjPtr<mirror::Class> from, ObjPtr<mirror::Class> to)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (to->GetPrimitiveType() == Primitive::Type::kPrimVoid) {
    // Result will be ignored.
    return true;
  } else if (from->GetPrimitiveType() == Primitive::Type::kPrimVoid) {
    // Returned value will be 0 / null.
    return true;
  } else {
    // Otherwise apply usual parameter conversion rules.
    return IsParameterTypeConvertible(from, to);
  }
}

bool ConvertJValueCommon(
    Handle<mirror::MethodType> callsite_type,
    Handle<mirror::MethodType> callee_type,
    ObjPtr<mirror::Class> from,
    ObjPtr<mirror::Class> to,
    JValue* value) {
  // The reader maybe concerned about the safety of the heap object
  // that may be in |value|. There is only one case where allocation
  // is obviously needed and that's for boxing. However, in the case
  // of boxing |value| contains a non-reference type.

  const Primitive::Type from_type = from->GetPrimitiveType();
  const Primitive::Type to_type = to->GetPrimitiveType();

  // Put incoming value into |src_value| and set return value to 0.
  // Errors and conversions from void require the return value to be 0.
  const JValue src_value(*value);
  value->SetJ(0);

  // Conversion from void set result to zero.
  if (from_type == Primitive::kPrimVoid) {
    return true;
  }

  // This method must be called only when the types don't match.
  DCHECK(from != to);

  if (IsPrimitiveType(from_type) && IsPrimitiveType(to_type)) {
    // The source and target types are both primitives.
    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(from_type, to_type, src_value, value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }
    return true;
  } else if (IsReferenceType(from_type) && IsReferenceType(to_type)) {
    // They're both reference types. If "from" is null, we can pass it
    // through unchanged. If not, we must generate a cast exception if
    // |to| is not assignable from the dynamic type of |ref|.
    //
    // Playing it safe with StackHandleScope here, not expecting any allocation
    // in mirror::Class::IsAssignable().
    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::Class> h_to(hs.NewHandle(to));
    Handle<mirror::Object> h_obj(hs.NewHandle(src_value.GetL()));
    if (UNLIKELY(!h_obj.IsNull() && !to->IsAssignableFrom(h_obj->GetClass()))) {
      ThrowClassCastException(h_to.Get(), h_obj->GetClass());
      return false;
    }
    value->SetL(h_obj.Get());
    return true;
  } else if (IsReferenceType(to_type)) {
    DCHECK(IsPrimitiveType(from_type));
    // The source type is a primitive and the target type is a reference, so we must box.
    // The target type maybe a super class of the boxed source type, for example,
    // if the source type is int, it's boxed type is java.lang.Integer, and the target
    // type could be java.lang.Number.
    Primitive::Type type;
    if (!GetUnboxedPrimitiveType(to, &type)) {
      ObjPtr<mirror::Class> boxed_from_class = GetBoxedPrimitiveClass(from_type);
      if (LIKELY(boxed_from_class->IsSubClass(to))) {
        type = from_type;
      } else {
        ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
        return false;
      }
    }

    if (UNLIKELY(from_type != type)) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(from_type, type, src_value, value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    // Then perform the actual boxing, and then set the reference.
    ObjPtr<mirror::Object> boxed = BoxPrimitive(type, src_value);
    value->SetL(boxed.Ptr());
    return true;
  } else {
    // The source type is a reference and the target type is a primitive, so we must unbox.
    DCHECK(IsReferenceType(from_type));
    DCHECK(IsPrimitiveType(to_type));

    ObjPtr<mirror::Object> from_obj(src_value.GetL());
    if (UNLIKELY(from_obj.IsNull())) {
      ThrowNullPointerException(
          StringPrintf("Expected to unbox a '%s' primitive type but was returned null",
                       from->PrettyDescriptor().c_str()).c_str());
      return false;
    }

    Primitive::Type unboxed_type;
    JValue unboxed_value;
    if (UNLIKELY(!GetUnboxedTypeAndValue(from_obj, &unboxed_type, &unboxed_value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(unboxed_type, to_type, unboxed_value, value))) {
      if (from->IsAssignableFrom(GetBoxedPrimitiveClass(to_type))) {
        // CallSite may be Number, but the Number object is
        // incompatible, e.g. Number (Integer) for a short.
        ThrowClassCastException(from, to);
      } else {
        // CallSite is incompatible, e.g. Integer for a short.
        ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      }
      return false;
    }

    return true;
  }
}

namespace {

inline void CopyArgumentsFromCallerFrame(const ShadowFrame& caller_frame,
                                         ShadowFrame* callee_frame,
                                         const InstructionOperands* const operands,
                                         const size_t first_dst_reg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  for (size_t i = 0; i < operands->GetNumberOfOperands(); ++i) {
    size_t dst_reg = first_dst_reg + i;
    size_t src_reg = operands->GetOperand(i);
    // Uint required, so that sign extension does not make this wrong on 64-bit systems
    uint32_t src_value = caller_frame.GetVReg(src_reg);
    ObjPtr<mirror::Object> o = caller_frame.GetVRegReference<kVerifyNone>(src_reg);
    // If both register locations contains the same value, the register probably holds a reference.
    // Note: As an optimization, non-moving collectors leave a stale reference value
    // in the references array even after the original vreg was overwritten to a non-reference.
    if (src_value == reinterpret_cast<uintptr_t>(o.Ptr())) {
      callee_frame->SetVRegReference(dst_reg, o.Ptr());
    } else {
      callee_frame->SetVReg(dst_reg, src_value);
    }
  }
}

inline bool ConvertAndCopyArgumentsFromCallerFrame(
    Thread* self,
    Handle<mirror::MethodType> callsite_type,
    Handle<mirror::MethodType> callee_type,
    const ShadowFrame& caller_frame,
    uint32_t first_dest_reg,
    const InstructionOperands* const operands,
    ShadowFrame* callee_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::ObjectArray<mirror::Class>> from_types(callsite_type->GetPTypes());
  ObjPtr<mirror::ObjectArray<mirror::Class>> to_types(callee_type->GetPTypes());

  const int32_t num_method_params = from_types->GetLength();
  if (to_types->GetLength() != num_method_params) {
    ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
    return false;
  }

  ShadowFrameGetter getter(caller_frame, operands);
  ShadowFrameSetter setter(callee_frame, first_dest_reg);
  return PerformConversions<ShadowFrameGetter, ShadowFrameSetter>(self,
                                                                  callsite_type,
                                                                  callee_type,
                                                                  &getter,
                                                                  &setter,
                                                                  num_method_params);
}

inline bool IsInvoke(const mirror::MethodHandle::Kind handle_kind) {
  return handle_kind <= mirror::MethodHandle::Kind::kLastInvokeKind;
}

inline bool IsInvokeTransform(const mirror::MethodHandle::Kind handle_kind) {
  return (handle_kind == mirror::MethodHandle::Kind::kInvokeTransform
          || handle_kind == mirror::MethodHandle::Kind::kInvokeCallSiteTransform);
}

inline bool IsInvokeVarHandle(const mirror::MethodHandle::Kind handle_kind) {
  return (handle_kind == mirror::MethodHandle::Kind::kInvokeVarHandle ||
          handle_kind == mirror::MethodHandle::Kind::kInvokeVarHandleExact);
}

inline bool IsFieldAccess(mirror::MethodHandle::Kind handle_kind) {
  return (handle_kind >= mirror::MethodHandle::Kind::kFirstAccessorKind
          && handle_kind <= mirror::MethodHandle::Kind::kLastAccessorKind);
}

// Calculate the number of ins for a proxy or native method, where we
// can't just look at the code item.
static inline size_t GetInsForProxyOrNativeMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(method->IsNative() || method->IsProxyMethod());
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  uint32_t shorty_length = 0;
  const char* shorty = method->GetShorty(&shorty_length);

  // Static methods do not include the receiver. The receiver isn't included
  // in the shorty_length though the return value is.
  size_t num_ins = method->IsStatic() ? shorty_length - 1 : shorty_length;
  for (const char* c = shorty + 1; *c != '\0'; ++c) {
    if (*c == 'J' || *c == 'D') {
      ++num_ins;
    }
  }
  return num_ins;
}

// Returns true iff. the callsite type for a polymorphic invoke is transformer
// like, i.e that it has a single input argument whose type is
// dalvik.system.EmulatedStackFrame.
static inline bool IsCallerTransformer(Handle<mirror::MethodType> callsite_type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::ObjectArray<mirror::Class>> param_types(callsite_type->GetPTypes());
  if (param_types->GetLength() == 1) {
    ObjPtr<mirror::Class> param(param_types->GetWithoutChecks(0));
    // NB Comparing descriptor here as it appears faster in cycle simulation than using:
    //   param == WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_EmulatedStackFrame)
    // Costs are 98 vs 173 cycles per invocation.
    return param->DescriptorEquals("Ldalvik/system/EmulatedStackFrame;");
  }

  return false;
}

static inline bool MethodHandleInvokeMethod(ArtMethod* called_method,
                                            Handle<mirror::MethodType> callsite_type,
                                            Handle<mirror::MethodType> target_type,
                                            Thread* self,
                                            ShadowFrame& shadow_frame,
                                            const InstructionOperands* const operands,
                                            JValue* result) REQUIRES_SHARED(Locks::mutator_lock_) {
  // Compute method information.
  CodeItemDataAccessor accessor(called_method->DexInstructionData());

  // Number of registers for the callee's call frame. Note that for non-exact
  // invokes, we always derive this information from the callee method. We
  // cannot guarantee during verification that the number of registers encoded
  // in the invoke is equal to the number of ins for the callee. This is because
  // some transformations (such as boxing a long -> Long or wideining an
  // int -> long will change that number.
  uint16_t num_regs;
  size_t num_input_regs;
  size_t first_dest_reg;
  if (LIKELY(accessor.HasCodeItem())) {
    num_regs = accessor.RegistersSize();
    first_dest_reg = num_regs - accessor.InsSize();
    num_input_regs = accessor.InsSize();
    // Parameter registers go at the end of the shadow frame.
    DCHECK_NE(first_dest_reg, (size_t)-1);
  } else {
    // No local regs for proxy and native methods.
    DCHECK(called_method->IsNative() || called_method->IsProxyMethod());
    num_regs = num_input_regs = GetInsForProxyOrNativeMethod(called_method);
    first_dest_reg = 0;
  }

  // Allocate shadow frame on the stack.
  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(num_regs, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();

  // Whether this polymorphic invoke was issued by a transformer method.
  bool is_caller_transformer = false;
  // Thread might be suspended during PerformArgumentConversions due to the
  // allocations performed during boxing.
  {
    ScopedStackedShadowFramePusher pusher(
        self, new_shadow_frame, StackedShadowFrameType::kShadowFrameUnderConstruction);
    if (callsite_type->IsExactMatch(target_type.Get())) {
      // This is an exact invoke, we can take the fast path of just copying all
      // registers without performing any argument conversions.
      CopyArgumentsFromCallerFrame(shadow_frame,
                                   new_shadow_frame,
                                   operands,
                                   first_dest_reg);
    } else {
      // This includes the case where we're entering this invoke-polymorphic
      // from a transformer method. In that case, the callsite_type will contain
      // a single argument of type dalvik.system.EmulatedStackFrame. In that
      // case, we'll have to unmarshal the EmulatedStackFrame into the
      // new_shadow_frame and perform argument conversions on it.
      if (IsCallerTransformer(callsite_type)) {
        is_caller_transformer = true;
        // The emulated stack frame is the first and only argument when we're coming
        // through from a transformer.
        size_t first_arg_register = operands->GetOperand(0);
        ObjPtr<mirror::EmulatedStackFrame> emulated_stack_frame(
            reinterpret_cast<mirror::EmulatedStackFrame*>(
                shadow_frame.GetVRegReference(first_arg_register)));
        if (!emulated_stack_frame->WriteToShadowFrame(self,
                                                      target_type,
                                                      first_dest_reg,
                                                      new_shadow_frame)) {
          DCHECK(self->IsExceptionPending());
          result->SetL(0);
          return false;
        }
      } else {
        if (!callsite_type->IsConvertible(target_type.Get())) {
          ThrowWrongMethodTypeException(target_type.Get(), callsite_type.Get());
          return false;
        }
        if (!ConvertAndCopyArgumentsFromCallerFrame(self,
                                                    callsite_type,
                                                    target_type,
                                                    shadow_frame,
                                                    first_dest_reg,
                                                    operands,
                                                    new_shadow_frame)) {
          DCHECK(self->IsExceptionPending());
          result->SetL(0);
          return false;
        }
      }
    }
  }

  bool use_interpreter_entrypoint = ClassLinker::ShouldUseInterpreterEntrypoint(
      called_method, called_method->GetEntryPointFromQuickCompiledCode());
  PerformCall(self,
              accessor,
              shadow_frame.GetMethod(),
              first_dest_reg,
              new_shadow_frame,
              result,
              use_interpreter_entrypoint);
  if (self->IsExceptionPending()) {
    return false;
  }

  // If the caller of this signature polymorphic method was a transformer,
  // we need to copy the result back out to the emulated stack frame.
  if (is_caller_transformer) {
    StackHandleScope<2> hs(self);
    size_t first_callee_register = operands->GetOperand(0);
    Handle<mirror::EmulatedStackFrame> emulated_stack_frame(
        hs.NewHandle(reinterpret_cast<mirror::EmulatedStackFrame*>(
            shadow_frame.GetVRegReference(first_callee_register))));
    Handle<mirror::MethodType> emulated_stack_type(hs.NewHandle(emulated_stack_frame->GetType()));
    JValue local_result;
    local_result.SetJ(result->GetJ());

    if (ConvertReturnValue(emulated_stack_type, target_type, &local_result)) {
      emulated_stack_frame->SetReturnValue(self, local_result);
      return true;
    }

    DCHECK(self->IsExceptionPending());
    return false;
  }

  return ConvertReturnValue(callsite_type, target_type, result);
}

static inline bool MethodHandleInvokeTransform(ArtMethod* called_method,
                                               Handle<mirror::MethodType> callsite_type,
                                               Handle<mirror::MethodType> callee_type,
                                               Thread* self,
                                               ShadowFrame& shadow_frame,
                                               Handle<mirror::MethodHandle> receiver,
                                               const InstructionOperands* const operands,
                                               JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This can be fixed to two, because the method we're calling here
  // (MethodHandle.transformInternal) doesn't have any locals and the signature
  // is known :
  //
  // private MethodHandle.transformInternal(EmulatedStackFrame sf);
  //
  // This means we need only two vregs :
  // - One for the receiver object.
  // - One for the only method argument (an EmulatedStackFrame).
  static constexpr size_t kNumRegsForTransform = 2;

  CodeItemDataAccessor accessor(called_method->DexInstructionData());
  DCHECK_EQ(kNumRegsForTransform, accessor.RegistersSize());
  DCHECK_EQ(kNumRegsForTransform, accessor.InsSize());

  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(kNumRegsForTransform, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();

  StackHandleScope<1> hs(self);
  MutableHandle<mirror::EmulatedStackFrame> sf(hs.NewHandle<mirror::EmulatedStackFrame>(nullptr));
  if (IsCallerTransformer(callsite_type)) {
    // If we're entering this transformer from another transformer, we can pass
    // through the handle directly to the callee, instead of having to
    // instantiate a new stack frame based on the shadow frame.
    size_t first_callee_register = operands->GetOperand(0);
    sf.Assign(reinterpret_cast<mirror::EmulatedStackFrame*>(
        shadow_frame.GetVRegReference(first_callee_register)));
  } else {
    sf.Assign(mirror::EmulatedStackFrame::CreateFromShadowFrameAndArgs(self,
                                                                       callsite_type,
                                                                       callee_type,
                                                                       shadow_frame,
                                                                       operands));

    // Something went wrong while creating the emulated stack frame, we should
    // throw the pending exception.
    if (sf == nullptr) {
      DCHECK(self->IsExceptionPending());
      return false;
    }
  }

  new_shadow_frame->SetVRegReference(0, receiver.Get());
  new_shadow_frame->SetVRegReference(1, sf.Get());

  bool use_interpreter_entrypoint = ClassLinker::ShouldUseInterpreterEntrypoint(
      called_method, called_method->GetEntryPointFromQuickCompiledCode());
  PerformCall(self,
              accessor,
              shadow_frame.GetMethod(),
              0 /* first destination register */,
              new_shadow_frame,
              result,
              use_interpreter_entrypoint);
  if (self->IsExceptionPending()) {
    return false;
  }

  // If the called transformer method we called has returned a value, then we
  // need to copy it back to |result|.
  sf->GetReturnValue(self, result);
  return ConvertReturnValue(callsite_type, callee_type, result);
}

inline static ObjPtr<mirror::Class> GetAndInitializeDeclaringClass(Thread* self, ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Method handle invocations on static fields should ensure class is
  // initialized. This usually happens when an instance is constructed
  // or class members referenced, but this is not guaranteed when
  // looking up method handles.
  ObjPtr<mirror::Class> klass = field->GetDeclaringClass();
  if (UNLIKELY(!klass->IsInitialized())) {
    StackHandleScope<1> hs(self);
    HandleWrapperObjPtr<mirror::Class> h(hs.NewHandleWrapper(&klass));
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h, true, true)) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
  }
  return klass;
}

ArtMethod* RefineTargetMethod(Thread* self,
                              ShadowFrame& shadow_frame,
                              const mirror::MethodHandle::Kind& handle_kind,
                              Handle<mirror::MethodType> handle_type,
                              Handle<mirror::MethodType> callsite_type,
                              const uint32_t receiver_reg,
                              ArtMethod* target_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (handle_kind == mirror::MethodHandle::Kind::kInvokeVirtual ||
      handle_kind == mirror::MethodHandle::Kind::kInvokeInterface) {
    // For virtual and interface methods ensure target_method points to
    // the actual method to invoke.
    ObjPtr<mirror::Object> receiver(shadow_frame.GetVRegReference(receiver_reg));
    if (IsCallerTransformer(callsite_type)) {
      // The current receiver is an emulated stack frame, the method's
      // receiver needs to be fetched from there as the emulated frame
      // will be unpacked into a new frame.
      receiver = ObjPtr<mirror::EmulatedStackFrame>::DownCast(receiver)->GetReceiver();
    }

    ObjPtr<mirror::Class> declaring_class(target_method->GetDeclaringClass());
    if (receiver == nullptr || receiver->GetClass() != declaring_class) {
      // Verify that _vRegC is an object reference and of the type expected by
      // the receiver.
      if (!VerifyObjectIsClass(receiver, declaring_class)) {
        DCHECK(self->IsExceptionPending());
        return nullptr;
      }
      return receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(
          target_method, kRuntimePointerSize);
    }
  } else if (handle_kind == mirror::MethodHandle::Kind::kInvokeDirect) {
    // String constructors are a special case, they are replaced with
    // StringFactory methods.
    if (target_method->IsConstructor() && target_method->GetDeclaringClass()->IsStringClass()) {
      DCHECK(handle_type->GetRType()->IsStringClass());
      return WellKnownClasses::StringInitToStringFactory(target_method);
    }
  } else if (handle_kind == mirror::MethodHandle::Kind::kInvokeSuper) {
    // Note that we're not dynamically dispatching on the type of the receiver
    // here. We use the static type of the "receiver" object that we've
    // recorded in the method handle's type, which will be the same as the
    // special caller that was specified at the point of lookup.
    ObjPtr<mirror::Class> referrer_class = handle_type->GetPTypes()->Get(0);
    ObjPtr<mirror::Class> declaring_class = target_method->GetDeclaringClass();
    if (referrer_class == declaring_class) {
      return target_method;
    }
    if (!declaring_class->IsInterface()) {
      ObjPtr<mirror::Class> super_class = referrer_class->GetSuperClass();
      uint16_t vtable_index = target_method->GetMethodIndex();
      DCHECK(super_class != nullptr);
      DCHECK(super_class->HasVTable());
      // Note that super_class is a super of referrer_class and target_method
      // will always be declared by super_class (or one of its super classes).
      DCHECK_LT(vtable_index, super_class->GetVTableLength());
      return super_class->GetVTableEntry(vtable_index, kRuntimePointerSize);
    }
  }
  return target_method;
}

bool DoInvokePolymorphicMethod(Thread* self,
                               ShadowFrame& shadow_frame,
                               Handle<mirror::MethodHandle> method_handle,
                               Handle<mirror::MethodType> callsite_type,
                               const InstructionOperands* const operands,
                               JValue* result)
  REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> handle_type(hs.NewHandle(method_handle->GetMethodType()));
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  DCHECK(IsInvoke(handle_kind));

  // Get the method we're actually invoking along with the kind of
  // invoke that is desired. We don't need to perform access checks at this
  // point because they would have been performed on our behalf at the point
  // of creation of the method handle.
  ArtMethod* target_method = method_handle->GetTargetMethod();
  uint32_t receiver_reg = (operands->GetNumberOfOperands() > 0) ? operands->GetOperand(0) : 0u;
  ArtMethod* called_method = RefineTargetMethod(self,
                                                shadow_frame,
                                                handle_kind,
                                                handle_type,
                                                callsite_type,
                                                receiver_reg,
                                                target_method);
  if (called_method == nullptr) {
    DCHECK(self->IsExceptionPending());
    return false;
  }

  if (IsInvokeTransform(handle_kind)) {
    // There are two cases here - method handles representing regular
    // transforms and those representing call site transforms. Method
    // handles for call site transforms adapt their MethodType to match
    // the call site. For these, the |callee_type| is the same as the
    // |callsite_type|. The VarargsCollector is such a tranform, its
    // method type depends on the call site, ie. x(a) or x(a, b), or
    // x(a, b, c). The VarargsCollector invokes a variable arity method
    // with the arity arguments in an array.
    Handle<mirror::MethodType> callee_type =
        (handle_kind == mirror::MethodHandle::Kind::kInvokeCallSiteTransform) ? callsite_type
        : handle_type;
    return MethodHandleInvokeTransform(called_method,
                                       callsite_type,
                                       callee_type,
                                       self,
                                       shadow_frame,
                                       method_handle /* receiver */,
                                       operands,
                                       result);
  } else {
    return MethodHandleInvokeMethod(called_method,
                                    callsite_type,
                                    handle_type,
                                    self,
                                    shadow_frame,
                                    operands,
                                    result);
  }
}

// Helper for getters in invoke-polymorphic.
inline static void MethodHandleFieldGet(Thread* self,
                                        const ShadowFrame& shadow_frame,
                                        ObjPtr<mirror::Object>& obj,
                                        ArtField* field,
                                        Primitive::Type field_type,
                                        JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  switch (field_type) {
    case Primitive::kPrimBoolean:
      DoFieldGetCommon<Primitive::kPrimBoolean>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimByte:
      DoFieldGetCommon<Primitive::kPrimByte>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimChar:
      DoFieldGetCommon<Primitive::kPrimChar>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimShort:
      DoFieldGetCommon<Primitive::kPrimShort>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimInt:
      DoFieldGetCommon<Primitive::kPrimInt>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimLong:
      DoFieldGetCommon<Primitive::kPrimLong>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimFloat:
      DoFieldGetCommon<Primitive::kPrimInt>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimDouble:
      DoFieldGetCommon<Primitive::kPrimLong>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimNot:
      DoFieldGetCommon<Primitive::kPrimNot>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
}

// Helper for setters in invoke-polymorphic.
inline bool MethodHandleFieldPut(Thread* self,
                                 ShadowFrame& shadow_frame,
                                 ObjPtr<mirror::Object>& obj,
                                 ArtField* field,
                                 Primitive::Type field_type,
                                 JValue& value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(!Runtime::Current()->IsActiveTransaction());
  static const bool kTransaction = false;         // Not in a transaction.
  static const bool kAssignabilityCheck = false;  // No access check.
  switch (field_type) {
    case Primitive::kPrimBoolean:
      return
          DoFieldPutCommon<Primitive::kPrimBoolean, kAssignabilityCheck, kTransaction>(
              self, shadow_frame, obj, field, value);
    case Primitive::kPrimByte:
      return DoFieldPutCommon<Primitive::kPrimByte, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimChar:
      return DoFieldPutCommon<Primitive::kPrimChar, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimShort:
      return DoFieldPutCommon<Primitive::kPrimShort, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      return DoFieldPutCommon<Primitive::kPrimInt, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      return DoFieldPutCommon<Primitive::kPrimLong, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimNot:
      return DoFieldPutCommon<Primitive::kPrimNot, kAssignabilityCheck, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
}

static JValue GetValueFromShadowFrame(const ShadowFrame& shadow_frame,
                                      Primitive::Type field_type,
                                      uint32_t vreg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue field_value;
  switch (field_type) {
    case Primitive::kPrimBoolean:
      field_value.SetZ(static_cast<uint8_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimByte:
      field_value.SetB(static_cast<int8_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimChar:
      field_value.SetC(static_cast<uint16_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimShort:
      field_value.SetS(static_cast<int16_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      field_value.SetI(shadow_frame.GetVReg(vreg));
      break;
    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      field_value.SetJ(shadow_frame.GetVRegLong(vreg));
      break;
    case Primitive::kPrimNot:
      field_value.SetL(shadow_frame.GetVRegReference(vreg));
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
  return field_value;
}

template <bool do_conversions>
bool MethodHandleFieldAccess(Thread* self,
                             ShadowFrame& shadow_frame,
                             Handle<mirror::MethodHandle> method_handle,
                             Handle<mirror::MethodType> callsite_type,
                             const InstructionOperands* const operands,
                             JValue* result) REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> handle_type(hs.NewHandle(method_handle->GetMethodType()));
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  ArtField* field = method_handle->GetTargetField();
  Primitive::Type field_type = field->GetTypeAsPrimitiveType();
  switch (handle_kind) {
    case mirror::MethodHandle::kInstanceGet: {
      size_t obj_reg = operands->GetOperand(0);
      ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(obj_reg);
      MethodHandleFieldGet(self, shadow_frame, obj, field, field_type, result);
      if (do_conversions && !ConvertReturnValue(callsite_type, handle_type, result)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return true;
    }
    case mirror::MethodHandle::kStaticGet: {
      ObjPtr<mirror::Object> obj = GetAndInitializeDeclaringClass(self, field);
      if (obj == nullptr) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      MethodHandleFieldGet(self, shadow_frame, obj, field, field_type, result);
      if (do_conversions && !ConvertReturnValue(callsite_type, handle_type, result)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return true;
    }
    case mirror::MethodHandle::kInstancePut: {
      size_t obj_reg = operands->GetOperand(0);
      size_t value_reg = operands->GetOperand(1);
      const size_t kPTypeIndex = 1;
      // Use ptypes instead of field type since we may be unboxing a reference for a primitive
      // field. The field type is incorrect for this case.
      JValue value = GetValueFromShadowFrame(
          shadow_frame,
          callsite_type->GetPTypes()->Get(kPTypeIndex)->GetPrimitiveType(),
          value_reg);
      if (do_conversions && !ConvertArgumentValue(callsite_type,
                                                  handle_type,
                                                  kPTypeIndex,
                                                  &value)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(obj_reg);
      return MethodHandleFieldPut(self, shadow_frame, obj, field, field_type, value);
    }
    case mirror::MethodHandle::kStaticPut: {
      ObjPtr<mirror::Object> obj = GetAndInitializeDeclaringClass(self, field);
      if (obj == nullptr) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      size_t value_reg = operands->GetOperand(0);
      const size_t kPTypeIndex = 0;
      // Use ptypes instead of field type since we may be unboxing a reference for a primitive
      // field. The field type is incorrect for this case.
      JValue value = GetValueFromShadowFrame(
          shadow_frame,
          callsite_type->GetPTypes()->Get(kPTypeIndex)->GetPrimitiveType(),
          value_reg);
      if (do_conversions && !ConvertArgumentValue(callsite_type,
                                                  handle_type,
                                                  kPTypeIndex,
                                                  &value)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return MethodHandleFieldPut(self, shadow_frame, obj, field, field_type, value);
    }
    default:
      LOG(FATAL) << "Unreachable: " << handle_kind;
      UNREACHABLE();
  }
}

bool DoVarHandleInvokeTranslationUnchecked(Thread* self,
                                           ShadowFrame& shadow_frame,
                                           mirror::VarHandle::AccessMode access_mode,
                                           Handle<mirror::VarHandle> vh,
                                           Handle<mirror::MethodType> vh_type,
                                           Handle<mirror::MethodType> callsite_type,
                                           const InstructionOperands* const operands,
                                           JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(operands->GetNumberOfOperands(), static_cast<uint32_t>(vh_type->GetNumberOfPTypes()));
  DCHECK_EQ(operands->GetNumberOfOperands(),
            static_cast<uint32_t>(callsite_type->GetNumberOfPTypes()));
  const size_t vreg_count = vh_type->NumberOfVRegs();
  ShadowFrameAllocaUniquePtr accessor_frame =
      CREATE_SHADOW_FRAME(vreg_count, nullptr, shadow_frame.GetMethod(), shadow_frame.GetDexPC());
  ShadowFrameGetter getter(shadow_frame, operands);
  static const uint32_t kFirstAccessorReg = 0;
  ShadowFrameSetter setter(accessor_frame.get(), kFirstAccessorReg);
  if (!PerformConversions(self, callsite_type, vh_type, &getter, &setter)) {
    return false;
  }
  RangeInstructionOperands accessor_operands(kFirstAccessorReg, kFirstAccessorReg + vreg_count);
  if (!vh->Access(access_mode, accessor_frame.get(), &accessor_operands, result)) {
    return false;
  }
  return ConvertReturnValue(callsite_type, vh_type, result);
}

bool DoVarHandleInvokeTranslation(Thread* self,
                                  ShadowFrame& shadow_frame,
                                  bool invokeExact,
                                  Handle<mirror::MethodHandle> method_handle,
                                  Handle<mirror::MethodType> callsite_type,
                                  const InstructionOperands* const operands,
                                  JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!invokeExact) {
    // Exact invokes are checked for compatability higher up. The
    // non-exact invoke path doesn't have a similar check due to
    // transformers which have EmulatedStack frame arguments with the
    // actual method type associated with the frame.
    if (UNLIKELY(!callsite_type->IsConvertible(method_handle->GetMethodType()))) {
      ThrowWrongMethodTypeException(method_handle->GetMethodType(), callsite_type.Get());
      return false;
    }
  }

  //
  // Basic checks that apply in all cases.
  //
  StackHandleScope<6> hs(self);
  Handle<mirror::ObjectArray<mirror::Class>>
      callsite_ptypes(hs.NewHandle(callsite_type->GetPTypes()));
  Handle<mirror::ObjectArray<mirror::Class>>
         mh_ptypes(hs.NewHandle(method_handle->GetMethodType()->GetPTypes()));

  // Check that the first parameter is a VarHandle
  if (callsite_ptypes->GetLength() < 1 ||
      !mh_ptypes->Get(0)->IsAssignableFrom(callsite_ptypes->Get(0)) ||
      mh_ptypes->Get(0) != mirror::VarHandle::StaticClass()) {
    ThrowWrongMethodTypeException(method_handle->GetMethodType(), callsite_type.Get());
    return false;
  }

  // Get the receiver
  mirror::Object* receiver = shadow_frame.GetVRegReference(operands->GetOperand(0));
  if (receiver == nullptr) {
    ThrowNullPointerException("Expected argument 1 to be a non-null VarHandle");
    return false;
  }

  // Cast to VarHandle instance
  Handle<mirror::VarHandle> vh(hs.NewHandle(down_cast<mirror::VarHandle*>(receiver)));
  DCHECK(mirror::VarHandle::StaticClass()->IsAssignableFrom(vh->GetClass()));

  // Determine the accessor kind to dispatch
  ArtMethod* target_method = method_handle->GetTargetMethod();
  int intrinsic_index = target_method->GetIntrinsic();
  mirror::VarHandle::AccessMode access_mode =
      mirror::VarHandle::GetAccessModeByIntrinsic(static_cast<Intrinsics>(intrinsic_index));
  Handle<mirror::MethodType> vh_type =
      hs.NewHandle(vh->GetMethodTypeForAccessMode(self, access_mode));
  Handle<mirror::MethodType> mh_invoke_type = hs.NewHandle(
      mirror::MethodType::CloneWithoutLeadingParameter(self, method_handle->GetMethodType()));
  if (method_handle->GetHandleKind() == mirror::MethodHandle::Kind::kInvokeVarHandleExact) {
    if (!mh_invoke_type->IsExactMatch(vh_type.Get())) {
      ThrowWrongMethodTypeException(vh_type.Get(), mh_invoke_type.Get());
      return false;
    }
  } else {
    DCHECK_EQ(method_handle->GetHandleKind(), mirror::MethodHandle::Kind::kInvokeVarHandle);
    if (!mh_invoke_type->IsConvertible(vh_type.Get())) {
      ThrowWrongMethodTypeException(vh_type.Get(), mh_invoke_type.Get());
      return false;
    }
  }

  Handle<mirror::MethodType> callsite_type_without_varhandle =
      hs.NewHandle(mirror::MethodType::CloneWithoutLeadingParameter(self, callsite_type.Get()));
  NoReceiverInstructionOperands varhandle_operands(operands);
  DCHECK_EQ(static_cast<int32_t>(varhandle_operands.GetNumberOfOperands()),
            callsite_type_without_varhandle->GetPTypes()->GetLength());
  return DoVarHandleInvokeTranslationUnchecked(self,
                                               shadow_frame,
                                               access_mode,
                                               vh,
                                               vh_type,
                                               callsite_type_without_varhandle,
                                               &varhandle_operands,
                                               result);
}

static inline bool MethodHandleInvokeInternal(Thread* self,
                                              ShadowFrame& shadow_frame,
                                              Handle<mirror::MethodHandle> method_handle,
                                              Handle<mirror::MethodType> callsite_type,
                                              const InstructionOperands* const operands,
                                              JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  if (IsFieldAccess(handle_kind)) {
    ObjPtr<mirror::MethodType> handle_type(method_handle->GetMethodType());
    DCHECK(!callsite_type->IsExactMatch(handle_type.Ptr()));
    if (!callsite_type->IsConvertible(handle_type.Ptr())) {
      ThrowWrongMethodTypeException(handle_type.Ptr(), callsite_type.Get());
      return false;
    }
    const bool do_convert = true;
    return MethodHandleFieldAccess<do_convert>(
        self,
        shadow_frame,
        method_handle,
        callsite_type,
        operands,
        result);
  }
  if (IsInvokeVarHandle(handle_kind)) {
    return DoVarHandleInvokeTranslation(self,
                                        shadow_frame,
                                        /*invokeExact*/ false,
                                        method_handle,
                                        callsite_type,
                                        operands,
                                        result);
  }
  return DoInvokePolymorphicMethod(self,
                                   shadow_frame,
                                   method_handle,
                                   callsite_type,
                                   operands,
                                   result);
}

static inline bool MethodHandleInvokeExactInternal(
    Thread* self,
    ShadowFrame& shadow_frame,
    Handle<mirror::MethodHandle> method_handle,
    Handle<mirror::MethodType> callsite_type,
    const InstructionOperands* const operands,
    JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> method_handle_type(hs.NewHandle(method_handle->GetMethodType()));
  if (!callsite_type->IsExactMatch(method_handle_type.Get())) {
    ThrowWrongMethodTypeException(method_handle_type.Get(), callsite_type.Get());
    return false;
  }

  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  if (IsFieldAccess(handle_kind)) {
    const bool do_convert = false;
    return MethodHandleFieldAccess<do_convert>(self,
                                               shadow_frame,
                                               method_handle,
                                               callsite_type,
                                               operands,
                                               result);
  }

  // Slow-path check.
  if (IsInvokeTransform(handle_kind) ||
      IsCallerTransformer(callsite_type)) {
    return DoInvokePolymorphicMethod(self,
                                     shadow_frame,
                                     method_handle,
                                     callsite_type,
                                     operands,
                                     result);
  } else if (IsInvokeVarHandle(handle_kind)) {
    return DoVarHandleInvokeTranslation(self,
                                        shadow_frame,
                                        /*invokeExact*/ true,
                                        method_handle,
                                        callsite_type,
                                        operands,
                                        result);
  }

  // On the fast-path. This is equivalent to DoCallPolymoprhic without the conversion paths.
  ArtMethod* target_method = method_handle->GetTargetMethod();
  uint32_t receiver_reg = (operands->GetNumberOfOperands() > 0) ? operands->GetOperand(0) : 0u;
  ArtMethod* called_method = RefineTargetMethod(self,
                                                shadow_frame,
                                                handle_kind,
                                                method_handle_type,
                                                callsite_type,
                                                receiver_reg,
                                                target_method);
  if (called_method == nullptr) {
    DCHECK(self->IsExceptionPending());
    return false;
  }

  // Compute method information.
  CodeItemDataAccessor accessor(called_method->DexInstructionData());
  uint16_t num_regs;
  size_t num_input_regs;
  size_t first_dest_reg;
  if (LIKELY(accessor.HasCodeItem())) {
    num_regs = accessor.RegistersSize();
    first_dest_reg = num_regs - accessor.InsSize();
    num_input_regs = accessor.InsSize();
    // Parameter registers go at the end of the shadow frame.
    DCHECK_NE(first_dest_reg, (size_t)-1);
  } else {
    // No local regs for proxy and native methods.
    DCHECK(called_method->IsNative() || called_method->IsProxyMethod());
    num_regs = num_input_regs = GetInsForProxyOrNativeMethod(called_method);
    first_dest_reg = 0;
  }

  // Allocate shadow frame on the stack.
  const char* old_cause = self->StartAssertNoThreadSuspension("DoCallCommon");
  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(num_regs, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();
  CopyArgumentsFromCallerFrame(shadow_frame,
                               new_shadow_frame,
                               operands,
                               first_dest_reg);
  self->EndAssertNoThreadSuspension(old_cause);

  bool use_interpreter_entrypoint = ClassLinker::ShouldUseInterpreterEntrypoint(
      called_method, called_method->GetEntryPointFromQuickCompiledCode());
  PerformCall(self,
              accessor,
              shadow_frame.GetMethod(),
              first_dest_reg,
              new_shadow_frame,
              result,
              use_interpreter_entrypoint);
  if (self->IsExceptionPending()) {
    return false;
  }
  return true;
}

}  // namespace

bool MethodHandleInvoke(Thread* self,
                       ShadowFrame& shadow_frame,
                       Handle<mirror::MethodHandle> method_handle,
                       Handle<mirror::MethodType> callsite_type,
                       const InstructionOperands* const operands,
                       JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(callsite_type->IsExactMatch(method_handle->GetMethodType()))) {
    // A non-exact invoke that can be invoked exactly.
    return MethodHandleInvokeExactInternal(self,
                                           shadow_frame,
                                           method_handle,
                                           callsite_type,
                                           operands,
                                           result);
  } else {
    return MethodHandleInvokeInternal(self,
                                      shadow_frame,
                                      method_handle,
                                      callsite_type,
                                      operands,
                                      result);
  }
}

bool MethodHandleInvokeExact(Thread* self,
                             ShadowFrame& shadow_frame,
                             Handle<mirror::MethodHandle> method_handle,
                             Handle<mirror::MethodType> callsite_type,
                             const InstructionOperands* const operands,
                             JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // We need to check the nominal type of the handle in addition to the
  // real type. The "nominal" type is present when MethodHandle.asType is
  // called any handle, and results in the declared type of the handle
  // changing.
  ObjPtr<mirror::MethodType> nominal_type(method_handle->GetNominalType());
  if (UNLIKELY(nominal_type != nullptr)) {
    if (UNLIKELY(!callsite_type->IsExactMatch(nominal_type.Ptr()))) {
      ThrowWrongMethodTypeException(nominal_type.Ptr(), callsite_type.Get());
      return false;
    }
    if (LIKELY(!nominal_type->IsExactMatch(method_handle->GetMethodType()))) {
      // Different nominal type means we have to treat as non-exact.
      return MethodHandleInvokeInternal(self,
                                        shadow_frame,
                                        method_handle,
                                        callsite_type,
                                        operands,
                                        result);
    }
  }
  return MethodHandleInvokeExactInternal(self,
                                         shadow_frame,
                                         method_handle,
                                         callsite_type,
                                         operands,
                                         result);
}

}  // namespace art
