/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <stdio.h>

#include <condition_variable>
#include <mutex>
#include <vector>

#include "android-base/macros.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"
#include "test_env.h"

namespace art {
namespace Test912Classes {

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912_isModifiableClass(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jboolean res = JNI_FALSE;
  jvmtiError result = jvmti_env->IsModifiableClass(klass, &res);
  JvmtiErrorToException(env, jvmti_env, result);
  return res;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getClassSignature(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  char* sig;
  char* gen;
  jvmtiError result = jvmti_env->GetClassSignature(klass, &sig, &gen);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) {
    if (i == 0) {
      return sig == nullptr ? nullptr : env->NewStringUTF(sig);
    } else {
      return gen == nullptr ? nullptr : env->NewStringUTF(gen);
    }
  };
  jobjectArray ret = CreateObjectArray(env, 2, "java/lang/String", callback);

  // Need to deallocate the strings.
  if (sig != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(sig));
  }
  if (gen != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(gen));
  }

  return ret;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912_isInterface(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jboolean is_interface = JNI_FALSE;
  jvmtiError result = jvmti_env->IsInterface(klass, &is_interface);
  JvmtiErrorToException(env, jvmti_env, result);
  return is_interface;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912_isArrayClass(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jboolean is_array_class = JNI_FALSE;
  jvmtiError result = jvmti_env->IsArrayClass(klass, &is_array_class);
  JvmtiErrorToException(env, jvmti_env, result);
  return is_array_class;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test912_getClassModifiers(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint mod;
  jvmtiError result = jvmti_env->GetClassModifiers(klass, &mod);
  JvmtiErrorToException(env, jvmti_env, result);
  return mod;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getClassFields(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint count = 0;
  jfieldID* fields = nullptr;
  jvmtiError result = jvmti_env->GetClassFields(klass, &count, &fields);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) {
    jint modifiers;
    // Ignore any errors for simplicity.
    jvmti_env->GetFieldModifiers(klass, fields[i], &modifiers);
    constexpr jint kStatic = 0x8;
    return env->ToReflectedField(klass,
                                 fields[i],
                                 (modifiers & kStatic) != 0 ? JNI_TRUE : JNI_FALSE);
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/Object", callback);
  if (fields != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(fields));
  }
  return ret;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getClassMethods(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint count = 0;
  jmethodID* methods = nullptr;
  jvmtiError result = jvmti_env->GetClassMethods(klass, &count, &methods);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) {
    jint modifiers;
    // Ignore any errors for simplicity.
    jvmti_env->GetMethodModifiers(methods[i], &modifiers);
    constexpr jint kStatic = 0x8;
    return env->ToReflectedMethod(klass,
                                  methods[i],
                                  (modifiers & kStatic) != 0 ? JNI_TRUE : JNI_FALSE);
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/Object", callback);
  if (methods != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(methods));
  }
  return ret;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getImplementedInterfaces(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint count = 0;
  jclass* classes = nullptr;
  jvmtiError result = jvmti_env->GetImplementedInterfaces(klass, &count, &classes);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) {
    return classes[i];
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/Class", callback);
  if (classes != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(classes));
  }
  return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test912_getClassStatus(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint status;
  jvmtiError result = jvmti_env->GetClassStatus(klass, &status);
  JvmtiErrorToException(env, jvmti_env, result);
  return status;
}

extern "C" JNIEXPORT jobject JNICALL Java_art_Test912_getClassLoader(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jobject classloader;
  jvmtiError result = jvmti_env->GetClassLoader(klass, &classloader);
  JvmtiErrorToException(env, jvmti_env, result);
  return classloader;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getClassLoaderClasses(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jobject jclassloader) {
  jint count = 0;
  jclass* classes = nullptr;
  jvmtiError result = jvmti_env->GetClassLoaderClasses(jclassloader, &count, &classes);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) {
    return classes[i];
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/Class", callback);
  if (classes != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(classes));
  }
  return ret;
}

extern "C" JNIEXPORT jintArray JNICALL Java_art_Test912_getClassVersion(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  jint major, minor;
  jvmtiError result = jvmti_env->GetClassVersionNumbers(klass, &minor, &major);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  jintArray int_array = env->NewIntArray(2);
  if (int_array == nullptr) {
    return nullptr;
  }
  jint buf[2] = { major, minor };
  env->SetIntArrayRegion(int_array, 0, 2, buf);

  return int_array;
}

static std::string GetClassName(jvmtiEnv* jenv, JNIEnv* jni_env, jclass klass) {
  char* name;
  jvmtiError result = jenv->GetClassSignature(klass, &name, nullptr);
  if (result != JVMTI_ERROR_NONE) {
    if (jni_env != nullptr) {
      JvmtiErrorToException(jni_env, jenv, result);
    } else {
      printf("Failed to get class signature.\n");
    }
    return "";
  }

  std::string tmp(name);
  jenv->Deallocate(reinterpret_cast<unsigned char*>(name));

  return tmp;
}

static void EnableEvents(JNIEnv* env,
                         jboolean enable,
                         decltype(jvmtiEventCallbacks().ClassLoad) class_load,
                         decltype(jvmtiEventCallbacks().ClassPrepare) class_prepare) {
  if (enable == JNI_FALSE) {
    jvmtiError ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                         JVMTI_EVENT_CLASS_LOAD,
                                                         nullptr);
    if (JvmtiErrorToException(env, jvmti_env, ret)) {
      return;
    }
    ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                              JVMTI_EVENT_CLASS_PREPARE,
                                              nullptr);
    JvmtiErrorToException(env, jvmti_env, ret);
    return;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.ClassLoad = class_load;
  callbacks.ClassPrepare = class_prepare;
  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }

  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_CLASS_LOAD,
                                            nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_CLASS_PREPARE,
                                            nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
}

static std::mutex gEventsMutex;
static std::vector<std::string> gEvents;

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test912_getClassLoadMessages(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  std::lock_guard<std::mutex> guard(gEventsMutex);
  jobjectArray ret = CreateObjectArray(env,
                                       static_cast<jint>(gEvents.size()),
                                       "java/lang/String",
                                       [&](jint i) {
    return env->NewStringUTF(gEvents[i].c_str());
  });
  gEvents.clear();
  return ret;
}

class ClassLoadPreparePrinter {
 public:
  static void JNICALL ClassLoadCallback(jvmtiEnv* jenv,
                                        JNIEnv* jni_env,
                                        jthread thread,
                                        jclass klass) {
    std::string name = GetClassName(jenv, jni_env, klass);
    if (name == "") {
      return;
    }
    std::string thread_name = GetThreadName(jenv, jni_env, thread);
    if (thread_name == "") {
      return;
    }
    if (thread_name_filter_ != "" && thread_name_filter_ != thread_name) {
      return;
    }

    std::lock_guard<std::mutex> guard(gEventsMutex);
    gEvents.push_back(android::base::StringPrintf("Load: %s on %s",
                                                  name.c_str(),
                                                  thread_name.c_str()));
  }

  static void JNICALL ClassPrepareCallback(jvmtiEnv* jenv,
                                           JNIEnv* jni_env,
                                           jthread thread,
                                           jclass klass) {
    std::string name = GetClassName(jenv, jni_env, klass);
    if (name == "") {
      return;
    }
    std::string thread_name = GetThreadName(jenv, jni_env, thread);
    if (thread_name == "") {
      return;
    }
    if (thread_name_filter_ != "" && thread_name_filter_ != thread_name) {
      return;
    }
    std::string cur_thread_name = GetThreadName(jenv, jni_env, nullptr);

    std::lock_guard<std::mutex> guard(gEventsMutex);
    gEvents.push_back(android::base::StringPrintf("Prepare: %s on %s (cur=%s)",
                                                  name.c_str(),
                                                  thread_name.c_str(),
                                                  cur_thread_name.c_str()));
  }

  static std::string GetThreadName(jvmtiEnv* jenv, JNIEnv* jni_env, jthread thread) {
    jvmtiThreadInfo info;
    jvmtiError result = jenv->GetThreadInfo(thread, &info);
    if (result != JVMTI_ERROR_NONE) {
      if (jni_env != nullptr) {
        JvmtiErrorToException(jni_env, jenv, result);
      } else {
        printf("Failed to get thread name.\n");
      }
      return "";
    }

    std::string tmp(info.name);
    jenv->Deallocate(reinterpret_cast<unsigned char*>(info.name));
    jni_env->DeleteLocalRef(info.context_class_loader);
    jni_env->DeleteLocalRef(info.thread_group);

    return tmp;
  }

  static std::string thread_name_filter_;
};
std::string ClassLoadPreparePrinter::thread_name_filter_;  // NOLINT [runtime/string] [4]

extern "C" JNIEXPORT void JNICALL Java_art_Test912_enableClassLoadPreparePrintEvents(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jboolean enable, jthread thread) {
  if (thread != nullptr) {
    ClassLoadPreparePrinter::thread_name_filter_ =
        ClassLoadPreparePrinter::GetThreadName(jvmti_env, env, thread);
  } else {
    ClassLoadPreparePrinter::thread_name_filter_ = "";
  }

  EnableEvents(env,
               enable,
               ClassLoadPreparePrinter::ClassLoadCallback,
               ClassLoadPreparePrinter::ClassPrepareCallback);
}

template<typename T>
static jthread RunEventThread(const std::string& name,
                              jvmtiEnv* jvmti,
                              JNIEnv* env,
                              void (*func)(jvmtiEnv*, JNIEnv*, T*),
                              T* data) {
  // Create a Thread object.
  std::string name_str = name;
  name_str += ": JVMTI_THREAD-Test912";
  ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF(name_str.c_str()));
  CHECK(thread_name.get() != nullptr);

  ScopedLocalRef<jclass> thread_klass(env, env->FindClass("java/lang/Thread"));
  CHECK(thread_klass.get() != nullptr);

  ScopedLocalRef<jobject> thread(env, env->AllocObject(thread_klass.get()));
  CHECK(thread.get() != nullptr);

  jmethodID initID = env->GetMethodID(thread_klass.get(), "<init>", "(Ljava/lang/String;)V");
  CHECK(initID != nullptr);

  env->CallNonvirtualVoidMethod(thread.get(), thread_klass.get(), initID, thread_name.get());
  CHECK(!env->ExceptionCheck());

  // Run agent thread.
  CheckJvmtiError(jvmti, jvmti->RunAgentThread(thread.get(),
                                               reinterpret_cast<jvmtiStartFunction>(func),
                                               reinterpret_cast<void*>(data),
                                               JVMTI_THREAD_NORM_PRIORITY));
  return thread.release();
}

static void JoinTread(JNIEnv* env, jthread thr) {
  ScopedLocalRef<jclass> thread_klass(env, env->FindClass("java/lang/Thread"));
  CHECK(thread_klass.get() != nullptr);

  jmethodID joinID = env->GetMethodID(thread_klass.get(), "join", "()V");
  CHECK(joinID != nullptr);

  env->CallVoidMethod(thr, joinID);
}

class ClassLoadPrepareEquality {
 public:
  static constexpr const char* kClassName = "Lart/Test912$ClassE;";
  static constexpr const char* kStorageFieldName = "STATIC";
  static constexpr const char* kStorageFieldSig = "Ljava/lang/Object;";
  static constexpr const char* kStorageWeakFieldName = "WEAK";
  static constexpr const char* kStorageWeakFieldSig = "Ljava/lang/ref/Reference;";
  static constexpr const char* kWeakClassName = "java/lang/ref/WeakReference";
  static constexpr const char* kWeakInitSig = "(Ljava/lang/Object;)V";
  static constexpr const char* kWeakGetSig = "()Ljava/lang/Object;";

  static void AgentThreadTest(jvmtiEnv* jvmti ATTRIBUTE_UNUSED,
                              JNIEnv* env,
                              jobject* obj_global) {
    jobject target = *obj_global;
    jobject target_local = env->NewLocalRef(target);
    {
      std::unique_lock<std::mutex> lk(mutex_);
      started_ = true;
      cond_started_.notify_all();
      cond_finished_.wait(lk, [] { return finished_; });
      CHECK(finished_);
    }
    CHECK(env->IsSameObject(target, target_local));
  }

  static void JNICALL ClassLoadCallback(jvmtiEnv* jenv,
                                        JNIEnv* jni_env,
                                        jthread thread ATTRIBUTE_UNUSED,
                                        jclass klass) {
    std::string name = GetClassName(jenv, jni_env, klass);
    if (name == kClassName) {
      found_ = true;
      stored_class_ = jni_env->NewGlobalRef(klass);
      weakly_stored_class_ = jni_env->NewWeakGlobalRef(klass);
      // Check that we update the local refs.
      agent_thread_ = static_cast<jthread>(jni_env->NewGlobalRef(RunEventThread<jobject>(
          "local-ref", jenv, jni_env, &AgentThreadTest, static_cast<jobject*>(&stored_class_))));
      {
        std::unique_lock<std::mutex> lk(mutex_);
        cond_started_.wait(lk, [] { return started_; });
      }
      // Store the value into a field in the heap.
      SetOrCompare(jni_env, klass, true);
    }
  }

  static void JNICALL ClassPrepareCallback(jvmtiEnv* jenv,
                                           JNIEnv* jni_env,
                                           jthread thread ATTRIBUTE_UNUSED,
                                           jclass klass) {
    std::string name = GetClassName(jenv, jni_env, klass);
    if (name == kClassName) {
      CHECK(stored_class_ != nullptr);
      CHECK(jni_env->IsSameObject(stored_class_, klass));
      CHECK(jni_env->IsSameObject(weakly_stored_class_, klass));
      {
        std::unique_lock<std::mutex> lk(mutex_);
        finished_ = true;
        cond_finished_.notify_all();
      }
      // Look up the value in a field in the heap.
      SetOrCompare(jni_env, klass, false);
      JoinTread(jni_env, agent_thread_);
      compared_ = true;
    }
  }

  static void SetOrCompare(JNIEnv* jni_env, jobject value, bool set) {
    CHECK(storage_class_ != nullptr);

    // Simple direct storage.
    jfieldID field = jni_env->GetStaticFieldID(storage_class_, kStorageFieldName, kStorageFieldSig);
    CHECK(field != nullptr);

    if (set) {
      jni_env->SetStaticObjectField(storage_class_, field, value);
      CHECK(!jni_env->ExceptionCheck());
    } else {
      ScopedLocalRef<jobject> stored(jni_env, jni_env->GetStaticObjectField(storage_class_, field));
      CHECK(jni_env->IsSameObject(value, stored.get()));
    }

    // Storage as a reference.
    ScopedLocalRef<jclass> weak_ref_class(jni_env, jni_env->FindClass(kWeakClassName));
    CHECK(weak_ref_class.get() != nullptr);
    jfieldID weak_field = jni_env->GetStaticFieldID(storage_class_,
                                                    kStorageWeakFieldName,
                                                    kStorageWeakFieldSig);
    CHECK(weak_field != nullptr);
    if (set) {
      // Create a WeakReference.
      jmethodID weak_init = jni_env->GetMethodID(weak_ref_class.get(), "<init>", kWeakInitSig);
      CHECK(weak_init != nullptr);
      ScopedLocalRef<jobject> weak_obj(jni_env, jni_env->NewObject(weak_ref_class.get(),
                                                                   weak_init,
                                                                   value));
      CHECK(weak_obj.get() != nullptr);
      jni_env->SetStaticObjectField(storage_class_, weak_field, weak_obj.get());
      CHECK(!jni_env->ExceptionCheck());
    } else {
      // Check the reference value.
      jmethodID get_referent = jni_env->GetMethodID(weak_ref_class.get(), "get", kWeakGetSig);
      CHECK(get_referent != nullptr);
      ScopedLocalRef<jobject> weak_obj(jni_env, jni_env->GetStaticObjectField(storage_class_,
                                                                              weak_field));
      CHECK(weak_obj.get() != nullptr);
      ScopedLocalRef<jobject> weak_referent(jni_env, jni_env->CallObjectMethod(weak_obj.get(),
                                                                               get_referent));
      CHECK(weak_referent.get() != nullptr);
      CHECK(jni_env->IsSameObject(value, weak_referent.get()));
    }
  }

  static void CheckFound() {
    CHECK(found_);
    CHECK(compared_);
  }

  static void Free(JNIEnv* env) {
    if (stored_class_ != nullptr) {
      env->DeleteGlobalRef(stored_class_);
      DCHECK(weakly_stored_class_ != nullptr);
      env->DeleteWeakGlobalRef(weakly_stored_class_);
      // Do not attempt to delete the local ref. It will be out of date by now.
    }
  }

  static jclass storage_class_;

 private:
  static jobject stored_class_;
  static jweak weakly_stored_class_;
  static jthread agent_thread_;
  static std::mutex mutex_;
  static bool started_;
  static std::condition_variable cond_finished_;
  static bool finished_;
  static std::condition_variable cond_started_;
  static bool found_;
  static bool compared_;
};

jclass ClassLoadPrepareEquality::storage_class_ = nullptr;
jobject ClassLoadPrepareEquality::stored_class_ = nullptr;
jweak ClassLoadPrepareEquality::weakly_stored_class_ = nullptr;
jthread ClassLoadPrepareEquality::agent_thread_ = nullptr;
std::mutex ClassLoadPrepareEquality::mutex_;
bool ClassLoadPrepareEquality::started_ = false;
std::condition_variable ClassLoadPrepareEquality::cond_started_;
bool ClassLoadPrepareEquality::finished_ = false;
std::condition_variable ClassLoadPrepareEquality::cond_finished_;
bool ClassLoadPrepareEquality::found_ = false;
bool ClassLoadPrepareEquality::compared_ = false;

extern "C" JNIEXPORT void JNICALL Java_art_Test912_setEqualityEventStorageClass(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  ClassLoadPrepareEquality::storage_class_ =
      reinterpret_cast<jclass>(env->NewGlobalRef(klass));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test912_enableClassLoadPrepareEqualityEvents(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jboolean b) {
  EnableEvents(env,
               b,
               ClassLoadPrepareEquality::ClassLoadCallback,
               ClassLoadPrepareEquality::ClassPrepareCallback);
  if (b == JNI_FALSE) {
    ClassLoadPrepareEquality::Free(env);
    ClassLoadPrepareEquality::CheckFound();
    env->DeleteGlobalRef(ClassLoadPrepareEquality::storage_class_);
    ClassLoadPrepareEquality::storage_class_ = nullptr;
  }
}

}  // namespace Test912Classes
}  // namespace art
