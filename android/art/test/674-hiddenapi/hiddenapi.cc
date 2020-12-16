/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "class_linker.h"
#include "dex/art_dex_file_loader.h"
#include "hidden_api.h"
#include "jni.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "ti-agent/scoped_utf_chars.h"

namespace art {
namespace Test674HiddenApi {

extern "C" JNIEXPORT void JNICALL Java_Main_init(JNIEnv*, jclass) {
  Runtime* runtime = Runtime::Current();
  runtime->SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kBlacklistOnly);
  runtime->SetDedupeHiddenApiWarnings(false);
  runtime->AlwaysSetHiddenApiWarningFlag();
}

extern "C" JNIEXPORT void JNICALL Java_Main_appendToBootClassLoader(
    JNIEnv* env, jclass, jstring jpath) {
  ScopedUtfChars utf(env, jpath);
  const char* path = utf.c_str();
  if (path == nullptr) {
    return;
  }

  ArtDexFileLoader dex_loader;
  std::string error_msg;
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  if (!dex_loader.Open(path,
                       path,
                       /* verify */ false,
                       /* verify_checksum */ true,
                       &error_msg,
                       &dex_files)) {
    LOG(FATAL) << "Could not open " << path << " for boot classpath extension: " << error_msg;
    UNREACHABLE();
  }

  ScopedObjectAccess soa(Thread::Current());
  for (std::unique_ptr<const DexFile>& dex_file : dex_files) {
    Runtime::Current()->GetClassLinker()->AppendToBootClassPath(
        Thread::Current(), *dex_file.release());
  }
}

static jobject NewInstance(JNIEnv* env, jclass klass) {
  jmethodID constructor = env->GetMethodID(klass, "<init>", "()V");
  if (constructor == NULL) {
    return NULL;
  }
  return env->NewObject(klass, constructor);
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverField(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jfieldID field = is_static ? env->GetStaticFieldID(klass, utf_name.c_str(), "I")
                             : env->GetFieldID(klass, utf_name.c_str(), "I");
  if (field == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canGetField(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jfieldID field = is_static ? env->GetStaticFieldID(klass, utf_name.c_str(), "I")
                             : env->GetFieldID(klass, utf_name.c_str(), "I");
  if (field == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }
  if (is_static) {
    env->GetStaticIntField(klass, field);
  } else {
    jobject obj = NewInstance(env, klass);
    if (obj == NULL) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return JNI_FALSE;
    }
    env->GetIntField(obj, field);
  }

  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canSetField(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jfieldID field = is_static ? env->GetStaticFieldID(klass, utf_name.c_str(), "I")
                             : env->GetFieldID(klass, utf_name.c_str(), "I");
  if (field == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }
  if (is_static) {
    env->SetStaticIntField(klass, field, 42);
  } else {
    jobject obj = NewInstance(env, klass);
    if (obj == NULL) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return JNI_FALSE;
    }
    env->SetIntField(obj, field, 42);
  }

  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverMethod(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jmethodID method = is_static ? env->GetStaticMethodID(klass, utf_name.c_str(), "()I")
                               : env->GetMethodID(klass, utf_name.c_str(), "()I");
  if (method == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canInvokeMethodA(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jmethodID method = is_static ? env->GetStaticMethodID(klass, utf_name.c_str(), "()I")
                               : env->GetMethodID(klass, utf_name.c_str(), "()I");
  if (method == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  if (is_static) {
    env->CallStaticIntMethodA(klass, method, nullptr);
  } else {
    jobject obj = NewInstance(env, klass);
    if (obj == NULL) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return JNI_FALSE;
    }
    env->CallIntMethodA(obj, method, nullptr);
  }

  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canInvokeMethodV(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jmethodID method = is_static ? env->GetStaticMethodID(klass, utf_name.c_str(), "()I")
                               : env->GetMethodID(klass, utf_name.c_str(), "()I");
  if (method == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  if (is_static) {
    env->CallStaticIntMethod(klass, method);
  } else {
    jobject obj = NewInstance(env, klass);
    if (obj == NULL) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return JNI_FALSE;
    }
    env->CallIntMethod(obj, method);
  }

  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

static constexpr size_t kConstructorSignatureLength = 5;  // e.g. (IZ)V
static constexpr size_t kNumConstructorArgs = kConstructorSignatureLength - 3;

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverConstructor(
    JNIEnv* env, jclass, jclass klass, jstring args) {
  ScopedUtfChars utf_args(env, args);
  jmethodID constructor = env->GetMethodID(klass, "<init>", utf_args.c_str());
  if (constructor == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canInvokeConstructorA(
    JNIEnv* env, jclass, jclass klass, jstring args) {
  ScopedUtfChars utf_args(env, args);
  jmethodID constructor = env->GetMethodID(klass, "<init>", utf_args.c_str());
  if (constructor == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  // CheckJNI won't allow out-of-range values, so just zero everything.
  CHECK_EQ(strlen(utf_args.c_str()), kConstructorSignatureLength);
  size_t initargs_size = sizeof(jvalue) * kNumConstructorArgs;
  jvalue *initargs = reinterpret_cast<jvalue*>(alloca(initargs_size));
  memset(initargs, 0, initargs_size);

  env->NewObjectA(klass, constructor, initargs);
  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canInvokeConstructorV(
    JNIEnv* env, jclass, jclass klass, jstring args) {
  ScopedUtfChars utf_args(env, args);
  jmethodID constructor = env->GetMethodID(klass, "<init>", utf_args.c_str());
  if (constructor == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  // CheckJNI won't allow out-of-range values, so just zero everything.
  CHECK_EQ(strlen(utf_args.c_str()), kConstructorSignatureLength);
  size_t initargs_size = sizeof(jvalue) * kNumConstructorArgs;
  jvalue *initargs = reinterpret_cast<jvalue*>(alloca(initargs_size));
  memset(initargs, 0, initargs_size);

  static_assert(kNumConstructorArgs == 2, "Change the varargs below if you change the constant");
  env->NewObject(klass, constructor, initargs[0], initargs[1]);
  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jint JNICALL Java_Reflection_getHiddenApiAccessFlags(JNIEnv*, jclass) {
  return static_cast<jint>(kAccHiddenApiBits);
}

extern "C" JNIEXPORT jboolean JNICALL Java_ChildClass_hasPendingWarning(JNIEnv*, jclass) {
  return Runtime::Current()->HasPendingHiddenApiWarning();
}

extern "C" JNIEXPORT void JNICALL Java_ChildClass_clearWarning(JNIEnv*, jclass) {
  Runtime::Current()->SetPendingHiddenApiWarning(false);
}

}  // namespace Test674HiddenApi
}  // namespace art
