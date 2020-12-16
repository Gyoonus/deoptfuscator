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

#include "java_lang_reflect_Parameter.h"

#include "android-base/stringprintf.h"
#include "nativehelper/jni_macros.h"

#include "art_method-inl.h"
#include "base/utils.h"
#include "common_throws.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "jni_internal.h"
#include "native_util.h"
#include "scoped_fast_native_object_access-inl.h"

namespace art {

using android::base::StringPrintf;

static jobject Parameter_getAnnotationNative(JNIEnv* env,
                                             jclass,
                                             jobject javaMethod,
                                             jint parameterIndex,
                                             jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(javaMethod == nullptr)) {
    ThrowNullPointerException("javaMethod == null");
    return nullptr;
  }

  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->IsProxyMethod()) {
    return nullptr;
  }

  uint32_t parameter_count = method->GetParameterTypeList()->Size();
  if (UNLIKELY(parameterIndex < 0 || static_cast<uint32_t>(parameterIndex) >= parameter_count)) {
    ThrowIllegalArgumentException(
        StringPrintf("Illegal parameterIndex %d for %s, parameter_count is %d",
                     parameterIndex,
                     method->PrettyMethod().c_str(),
                     parameter_count).c_str());
    return nullptr;
  }

  uint32_t annotated_parameter_count = annotations::GetNumberOfAnnotatedMethodParameters(method);
  if (annotated_parameter_count == 0u) {
    return nullptr;
  }

  // For constructors with implicit arguments, we may need to adjust
  // annotation positions based on whether the implicit parameters are
  // expected to known and not just a compiler implementation detail.
  if (method->IsConstructor()) {
    StackHandleScope<1> hs(soa.Self());
    // If declaring class is a local or an enum, do not pad parameter
    // annotations, as the implicit constructor parameters are an
    // implementation detail rather than required by JLS.
    Handle<mirror::Class> declaring_class = hs.NewHandle(method->GetDeclaringClass());
    if (annotations::GetEnclosingMethod(declaring_class) == nullptr && !declaring_class->IsEnum()) {
      // Adjust the parameter index if the number of annotations does
      // not match the number of parameters.
      if (annotated_parameter_count <= parameter_count) {
        // Workaround for dexer not inserting annotation state for implicit parameters (b/68033708).
        uint32_t skip_count = parameter_count - annotated_parameter_count;
        DCHECK_GE(2u, skip_count);
        if (parameterIndex < static_cast<jint>(skip_count)) {
          return nullptr;
        }
        parameterIndex -= skip_count;
      } else {
        // Workaround for Jack erroneously inserting implicit parameter for local classes
        // (b/68033708).
        DCHECK_EQ(1u, annotated_parameter_count - parameter_count);
        parameterIndex += static_cast<jint>(annotated_parameter_count - parameter_count);
      }
    }
  }

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(soa.Decode<mirror::Class>(annotationType)));
  return soa.AddLocalReference<jobject>(
      annotations::GetAnnotationForMethodParameter(method, parameterIndex, klass));
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(
      Parameter,
      getAnnotationNative,
      "(Ljava/lang/reflect/Executable;ILjava/lang/Class;)Ljava/lang/annotation/Annotation;"),
};

void register_java_lang_reflect_Parameter(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Parameter");
}

}  // namespace art
