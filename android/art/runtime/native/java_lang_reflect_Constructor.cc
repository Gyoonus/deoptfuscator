/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "java_lang_reflect_Constructor.h"

#include "nativehelper/jni_macros.h"

#include "art_method-inl.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "class_linker.h"
#include "dex/dex_file_annotations.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "native_util.h"
#include "reflection.h"
#include "scoped_fast_native_object_access-inl.h"
#include "well_known_classes.h"

namespace art {

static jobjectArray Constructor_getExceptionTypes(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod)
      ->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  mirror::ObjectArray<mirror::Class>* result_array =
      annotations::GetExceptionTypesForMethod(method);
  if (result_array == nullptr) {
    // Return an empty array instead of a null pointer.
    ObjPtr<mirror::Class> class_class = mirror::Class::GetJavaLangClass();
    ObjPtr<mirror::Class> class_array_class =
        Runtime::Current()->GetClassLinker()->FindArrayClass(soa.Self(), &class_class);
    if (class_array_class == nullptr) {
      return nullptr;
    }
    ObjPtr<mirror::ObjectArray<mirror::Class>> empty_array =
        mirror::ObjectArray<mirror::Class>::Alloc(soa.Self(), class_array_class, 0);
    return soa.AddLocalReference<jobjectArray>(empty_array);
  } else {
    return soa.AddLocalReference<jobjectArray>(result_array);
  }
}

/*
 * We can also safely assume the constructor isn't associated
 * with an interface, array, or primitive class. If this is coming from
 * native, it is OK to avoid access checks since JNI does not enforce them.
 */
static jobject Constructor_newInstance0(JNIEnv* env, jobject javaMethod, jobjectArray javaArgs) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::Constructor> m = soa.Decode<mirror::Constructor>(javaMethod);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> c(hs.NewHandle(m->GetDeclaringClass()));
  if (UNLIKELY(c->IsAbstract())) {
    soa.Self()->ThrowNewExceptionF("Ljava/lang/InstantiationException;", "Can't instantiate %s %s",
                                   c->IsInterface() ? "interface" : "abstract class",
                                   c->PrettyDescriptor().c_str());
    return nullptr;
  }
  // Verify that we can access the class.
  if (!m->IsAccessible() && !c->IsPublic()) {
    // Go 2 frames back, this method is always called from newInstance0, which is called from
    // Constructor.newInstance(Object... args).
    ObjPtr<mirror::Class> caller = GetCallingClass(soa.Self(), 2);
    // If caller is null, then we called from JNI, just avoid the check since JNI avoids most
    // access checks anyways. TODO: Investigate if this the correct behavior.
    if (caller != nullptr && !caller->CanAccess(c.Get())) {
      if (c->PrettyDescriptor() == "dalvik.system.DexPathList$Element") {
        // b/20699073.
        LOG(WARNING) << "The dalvik.system.DexPathList$Element constructor is not accessible by "
                        "default. This is a temporary workaround for backwards compatibility "
                        "with class-loader hacks. Please update your application.";
      } else {
        soa.Self()->ThrowNewExceptionF(
            "Ljava/lang/IllegalAccessException;", "%s is not accessible from %s",
            c->PrettyClass().c_str(),
            caller->PrettyClass().c_str());
        return nullptr;
      }
    }
  }
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(soa.Self(), c, true, true)) {
    DCHECK(soa.Self()->IsExceptionPending());
    return nullptr;
  }
  bool movable = true;
  if (!kMovingClasses && c->IsClassClass()) {
    movable = false;
  }

  // String constructor is replaced by a StringFactory method in InvokeMethod.
  if (c->IsStringClass()) {
    return InvokeMethod(soa, javaMethod, nullptr, javaArgs, 2);
  }

  ObjPtr<mirror::Object> receiver =
      movable ? c->AllocObject(soa.Self()) : c->AllocNonMovableObject(soa.Self());
  if (receiver == nullptr) {
    return nullptr;
  }
  jobject javaReceiver = soa.AddLocalReference<jobject>(receiver);
  InvokeMethod(soa, javaMethod, javaReceiver, javaArgs, 2);
  // Constructors are ()V methods, so we shouldn't touch the result of InvokeMethod.
  return javaReceiver;
}

static jobject Constructor_newInstanceFromSerialization(JNIEnv* env, jclass unused ATTRIBUTE_UNUSED,
                                                        jclass ctorClass, jclass allocClass) {
    jmethodID ctor = env->GetMethodID(ctorClass, "<init>", "()V");
    DCHECK(ctor != NULL);
    return env->NewObject(allocClass, ctor);
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(Constructor, getExceptionTypes, "()[Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Constructor, newInstance0, "([Ljava/lang/Object;)Ljava/lang/Object;"),
  FAST_NATIVE_METHOD(Constructor, newInstanceFromSerialization, "(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/Object;"),
};

void register_java_lang_reflect_Constructor(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Constructor");
}

}  // namespace art
