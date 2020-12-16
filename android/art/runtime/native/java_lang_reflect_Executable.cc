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

#include "java_lang_reflect_Executable.h"

#include "android-base/stringprintf.h"
#include "nativehelper/jni_macros.h"

#include "art_method-inl.h"
#include "dex/dex_file_annotations.h"
#include "handle.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "native_util.h"
#include "reflection.h"
#include "scoped_fast_native_object_access-inl.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

static jobjectArray Executable_getDeclaredAnnotationsNative(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    // Return an empty array instead of a null pointer.
    ObjPtr<mirror::Class> annotation_array_class =
        soa.Decode<mirror::Class>(WellKnownClasses::java_lang_annotation_Annotation__array);
    ObjPtr<mirror::ObjectArray<mirror::Object>> empty_array =
        mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(), annotation_array_class, 0);
    return soa.AddLocalReference<jobjectArray>(empty_array);
  }
  return soa.AddLocalReference<jobjectArray>(annotations::GetAnnotationsForMethod(method));
}

static jobject Executable_getAnnotationNative(JNIEnv* env,
                                              jobject javaMethod,
                                              jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->IsProxyMethod()) {
    return nullptr;
  } else {
    Handle<mirror::Class> klass(hs.NewHandle(soa.Decode<mirror::Class>(annotationType)));
    return soa.AddLocalReference<jobject>(annotations::GetAnnotationForMethod(method, klass));
  }
}

static jobjectArray Executable_getSignatureAnnotation(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    return nullptr;
  }
  return soa.AddLocalReference<jobjectArray>(annotations::GetSignatureAnnotationForMethod(method));
}


static jobjectArray Executable_getParameterAnnotationsNative(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->IsProxyMethod()) {
    return nullptr;
  }

  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::Object>> annotations =
      hs.NewHandle(annotations::GetParameterAnnotations(method));
  if (annotations.IsNull()) {
    return nullptr;
  }

  // If the method is not a constructor, or has parameter annotations
  // for each parameter, then we can return those annotations
  // unmodified. Otherwise, we need to look at whether the
  // constructor has implicit parameters as these may need padding
  // with empty parameter annotations.
  if (!method->IsConstructor() ||
      annotations->GetLength() == static_cast<int>(method->GetNumberOfParameters())) {
    return soa.AddLocalReference<jobjectArray>(annotations.Get());
  }

  // If declaring class is a local or an enum, do not pad parameter
  // annotations, as the implicit constructor parameters are an implementation
  // detail rather than required by JLS.
  Handle<mirror::Class> declaring_class = hs.NewHandle(method->GetDeclaringClass());
  if (annotations::GetEnclosingMethod(declaring_class) != nullptr ||
      declaring_class->IsEnum()) {
    return soa.AddLocalReference<jobjectArray>(annotations.Get());
  }

  // Prepare to resize the annotations so there is 1:1 correspondence
  // with the constructor parameters.
  Handle<mirror::ObjectArray<mirror::Object>> resized_annotations = hs.NewHandle(
      mirror::ObjectArray<mirror::Object>::Alloc(
          soa.Self(),
          annotations->GetClass(),
          static_cast<int>(method->GetNumberOfParameters())));
  if (resized_annotations.IsNull()) {
    DCHECK(soa.Self()->IsExceptionPending());
    return nullptr;
  }

  static constexpr bool kTransactionActive = false;
  const int32_t offset = resized_annotations->GetLength() - annotations->GetLength();
  if (offset > 0) {
    // Workaround for dexers (d8/dx) that do not insert annotations
    // for implicit parameters (b/68033708).
    ObjPtr<mirror::Class> annotation_array_class =
        soa.Decode<mirror::Class>(WellKnownClasses::java_lang_annotation_Annotation__array);
    Handle<mirror::ObjectArray<mirror::Object>> empty_annotations = hs.NewHandle(
        mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(), annotation_array_class, 0));
    if (empty_annotations.IsNull()) {
      DCHECK(soa.Self()->IsExceptionPending());
      return nullptr;
    }
    for (int i = 0; i < offset; ++i) {
      resized_annotations->SetWithoutChecks<kTransactionActive>(i, empty_annotations.Get());
    }
    for (int i = 0; i < annotations->GetLength(); ++i) {
      ObjPtr<mirror::Object> annotation = annotations->GetWithoutChecks(i);
      resized_annotations->SetWithoutChecks<kTransactionActive>(i + offset, annotation);
    }
  } else {
    // Workaround for Jack (defunct) erroneously inserting annotations
    // for local classes (b/68033708).
    DCHECK_LT(offset, 0);
    for (int i = 0; i < resized_annotations->GetLength(); ++i) {
      ObjPtr<mirror::Object> annotation = annotations->GetWithoutChecks(i - offset);
      resized_annotations->SetWithoutChecks<kTransactionActive>(i, annotation);
    }
  }
  return soa.AddLocalReference<jobjectArray>(resized_annotations.Get());
}

static jobjectArray Executable_getParameters0(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  Thread* self = soa.Self();
  StackHandleScope<8> hs(self);

  Handle<mirror::Method> executable = hs.NewHandle(soa.Decode<mirror::Method>(javaMethod));
  ArtMethod* art_method = executable.Get()->GetArtMethod();
  if (art_method->GetDeclaringClass()->IsProxyClass()) {
    return nullptr;
  }

  // Find the MethodParameters system annotation.
  MutableHandle<mirror::ObjectArray<mirror::String>> names =
      hs.NewHandle<mirror::ObjectArray<mirror::String>>(nullptr);
  MutableHandle<mirror::IntArray> access_flags = hs.NewHandle<mirror::IntArray>(nullptr);
  if (!annotations::GetParametersMetadataForMethod(art_method, &names, &access_flags)) {
    return nullptr;
  }

  // Validate the MethodParameters system annotation data.
  if (UNLIKELY(names == nullptr || access_flags == nullptr)) {
    ThrowIllegalArgumentException(
        StringPrintf("Missing parameter metadata for names or access flags for %s",
                     art_method->PrettyMethod().c_str()).c_str());
    return nullptr;
  }

  // Check array sizes match each other
  int32_t names_count = names.Get()->GetLength();
  int32_t access_flags_count = access_flags.Get()->GetLength();
  if (names_count != access_flags_count) {
    ThrowIllegalArgumentException(
        StringPrintf(
            "Inconsistent parameter metadata for %s. names length: %d, access flags length: %d",
            art_method->PrettyMethod().c_str(),
            names_count,
            access_flags_count).c_str());
    return nullptr;
  }

  // Instantiate a Parameter[] to hold the result.
  Handle<mirror::Class> parameter_array_class =
      hs.NewHandle(
          soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Parameter__array));
  Handle<mirror::ObjectArray<mirror::Object>> parameter_array =
      hs.NewHandle(
          mirror::ObjectArray<mirror::Object>::Alloc(self,
                                                     parameter_array_class.Get(),
                                                     names_count));
  if (UNLIKELY(parameter_array == nullptr)) {
    self->AssertPendingException();
    return nullptr;
  }

  Handle<mirror::Class> parameter_class =
      hs.NewHandle(soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Parameter));
  ArtMethod* parameter_init =
      jni::DecodeArtMethod(WellKnownClasses::java_lang_reflect_Parameter_init);

  // Mutable handles used in the loop below to ensure cleanup without scaling the number of
  // handles by the number of parameters.
  MutableHandle<mirror::String> name = hs.NewHandle<mirror::String>(nullptr);
  MutableHandle<mirror::Object> parameter = hs.NewHandle<mirror::Object>(nullptr);

  // Populate the Parameter[] to return.
  for (int32_t parameter_index = 0; parameter_index < names_count; parameter_index++) {
    name.Assign(names.Get()->Get(parameter_index));
    int32_t modifiers = access_flags.Get()->Get(parameter_index);

    // Allocate / initialize the Parameter to add to parameter_array.
    parameter.Assign(parameter_class->AllocObject(self));
    if (UNLIKELY(parameter == nullptr)) {
      self->AssertPendingOOMException();
      return nullptr;
    }

    uint32_t args[5] = { PointerToLowMemUInt32(parameter.Get()),
                         PointerToLowMemUInt32(name.Get()),
                         static_cast<uint32_t>(modifiers),
                         PointerToLowMemUInt32(executable.Get()),
                         static_cast<uint32_t>(parameter_index)
    };
    JValue result;
    static const char* method_signature = "VLILI";  // return + parameter types
    parameter_init->Invoke(self, args, sizeof(args), &result, method_signature);
    if (UNLIKELY(self->IsExceptionPending())) {
      return nullptr;
    }

    // Store the Parameter in the Parameter[].
    parameter_array.Get()->Set(parameter_index, parameter.Get());
    if (UNLIKELY(self->IsExceptionPending())) {
      return nullptr;
    }
  }
  return soa.AddLocalReference<jobjectArray>(parameter_array.Get());
}

static jboolean Executable_isAnnotationPresentNative(JNIEnv* env,
                                                     jobject javaMethod,
                                                     jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    return false;
  }
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(soa.Decode<mirror::Class>(annotationType)));
  return annotations::IsMethodAnnotationPresent(method, klass);
}

static jint Executable_compareMethodParametersInternal(JNIEnv* env,
                                                       jobject thisMethod,
                                                       jobject otherMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* this_method = ArtMethod::FromReflectedMethod(soa, thisMethod);
  ArtMethod* other_method = ArtMethod::FromReflectedMethod(soa, otherMethod);

  this_method = this_method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  other_method = other_method->GetInterfaceMethodIfProxy(kRuntimePointerSize);

  const DexFile::TypeList* this_list = this_method->GetParameterTypeList();
  const DexFile::TypeList* other_list = other_method->GetParameterTypeList();

  if (this_list == other_list) {
    return 0;
  }

  if (this_list == nullptr && other_list != nullptr) {
    return -1;
  }

  if (other_list == nullptr && this_list != nullptr) {
    return 1;
  }

  const int32_t this_size = this_list->Size();
  const int32_t other_size = other_list->Size();

  if (this_size != other_size) {
    return (this_size - other_size);
  }

  for (int32_t i = 0; i < this_size; ++i) {
    const DexFile::TypeId& lhs = this_method->GetDexFile()->GetTypeId(
        this_list->GetTypeItem(i).type_idx_);
    const DexFile::TypeId& rhs = other_method->GetDexFile()->GetTypeId(
        other_list->GetTypeItem(i).type_idx_);

    uint32_t lhs_len, rhs_len;
    const char* lhs_data = this_method->GetDexFile()->StringDataAndUtf16LengthByIdx(
        lhs.descriptor_idx_, &lhs_len);
    const char* rhs_data = other_method->GetDexFile()->StringDataAndUtf16LengthByIdx(
        rhs.descriptor_idx_, &rhs_len);

    int cmp = strcmp(lhs_data, rhs_data);
    if (cmp != 0) {
      return (cmp < 0) ? -1 : 1;
    }
  }

  return 0;
}

static jstring Executable_getMethodNameInternal(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  return soa.AddLocalReference<jstring>(method->GetNameAsString(soa.Self()));
}

static jclass Executable_getMethodReturnTypeInternal(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  ObjPtr<mirror::Class> return_type(method->ResolveReturnType());
  if (return_type.IsNull()) {
    CHECK(soa.Self()->IsExceptionPending());
    return nullptr;
  }

  return soa.AddLocalReference<jclass>(return_type);
}

// TODO: Move this to mirror::Class ? Other mirror types that commonly appear
// as arrays have a GetArrayClass() method. This is duplicated in
// java_lang_Class.cc as well.
static ObjPtr<mirror::Class> GetClassArrayClass(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> class_class = mirror::Class::GetJavaLangClass();
  return Runtime::Current()->GetClassLinker()->FindArrayClass(self, &class_class);
}

static jobjectArray Executable_getParameterTypesInternal(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);

  const DexFile::TypeList* params = method->GetParameterTypeList();
  if (params == nullptr) {
    return nullptr;
  }

  const uint32_t num_params = params->Size();

  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::Class> class_array_class = hs.NewHandle(GetClassArrayClass(soa.Self()));
  Handle<mirror::ObjectArray<mirror::Class>> ptypes = hs.NewHandle(
      mirror::ObjectArray<mirror::Class>::Alloc(soa.Self(), class_array_class.Get(), num_params));
  if (ptypes.IsNull()) {
    DCHECK(soa.Self()->IsExceptionPending());
    return nullptr;
  }

  MutableHandle<mirror::Class> param(hs.NewHandle<mirror::Class>(nullptr));
  for (uint32_t i = 0; i < num_params; ++i) {
    const dex::TypeIndex type_idx = params->GetTypeItem(i).type_idx_;
    param.Assign(Runtime::Current()->GetClassLinker()->ResolveType(type_idx, method));
    if (param.Get() == nullptr) {
      DCHECK(soa.Self()->IsExceptionPending());
      return nullptr;
    }
    ptypes->SetWithoutChecks<false>(i, param.Get());
  }

  return soa.AddLocalReference<jobjectArray>(ptypes.Get());
}

static jint Executable_getParameterCountInternal(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);

  const DexFile::TypeList* params = method->GetParameterTypeList();
  return (params == nullptr) ? 0 : params->Size();
}


static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(Executable, compareMethodParametersInternal,
                     "(Ljava/lang/reflect/Method;)I"),
  FAST_NATIVE_METHOD(Executable, getAnnotationNative,
                     "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;"),
  FAST_NATIVE_METHOD(Executable, getDeclaredAnnotationsNative,
                     "()[Ljava/lang/annotation/Annotation;"),
  FAST_NATIVE_METHOD(Executable, getParameterAnnotationsNative,
                     "()[[Ljava/lang/annotation/Annotation;"),
  FAST_NATIVE_METHOD(Executable, getMethodNameInternal, "()Ljava/lang/String;"),
  FAST_NATIVE_METHOD(Executable, getMethodReturnTypeInternal, "()Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Executable, getParameterTypesInternal, "()[Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Executable, getParameterCountInternal, "()I"),
  FAST_NATIVE_METHOD(Executable, getParameters0, "()[Ljava/lang/reflect/Parameter;"),
  FAST_NATIVE_METHOD(Executable, getSignatureAnnotation, "()[Ljava/lang/String;"),
  FAST_NATIVE_METHOD(Executable, isAnnotationPresentNative, "(Ljava/lang/Class;)Z"),
};

void register_java_lang_reflect_Executable(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Executable");
}

}  // namespace art
