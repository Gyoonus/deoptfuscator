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

#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include <android-base/logging.h>

#include "art_method-inl.h"
#include "base/runtime_debug.h"
#include "jni.h"

namespace art {

static JavaVM* jvm = nullptr;

static jint Java_Main_intFastNativeMethod(JNIEnv*, jclass, jint a, jint b, jint c);
static jint Java_Main_intCriticalNativeMethod(jint a, jint b, jint c);

static JNINativeMethod sMainMethods[] = {
  {"intFastNativeMethod", "(III)I", reinterpret_cast<void*>(Java_Main_intFastNativeMethod) },
  {"intCriticalNativeMethod", "(III)I", reinterpret_cast<void*>(Java_Main_intCriticalNativeMethod) },
};

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void*) {
  CHECK(vm != nullptr);
  CHECK(jvm == nullptr);
  jvm = vm;
  std::cout << "JNI_OnLoad called" << std::endl;

  return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNI_OnUnload(JavaVM*, void*) {
  // std::cout since LOG(INFO) adds extra stuff like pid.
  std::cout << "JNI_OnUnload called" << std::endl;
  // Clear jvm for CHECK in test 004-JniTest.
  jvm = nullptr;
}

static void* AttachHelper(void* arg) {
  CHECK(jvm != nullptr);

  JNIEnv* env = nullptr;
  JavaVMAttachArgs args = { JNI_VERSION_1_6, __FUNCTION__, nullptr };
  int attach_result = jvm->AttachCurrentThread(&env, &args);
  CHECK_EQ(attach_result, 0);

  typedef void (*Fn)(JNIEnv*);
  Fn fn = reinterpret_cast<Fn>(arg);
  fn(env);

  int detach_result = jvm->DetachCurrentThread();
  CHECK_EQ(detach_result, 0);
  return nullptr;
}

static void PthreadHelper(void (*fn)(JNIEnv*)) {
  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread, nullptr, AttachHelper,
                                             reinterpret_cast<void*>(fn));
  CHECK_EQ(pthread_create_result, 0);
  int pthread_join_result = pthread_join(pthread, nullptr);
  CHECK_EQ(pthread_join_result, 0);
}

static void testFindClassOnAttachedNativeThread(JNIEnv* env) {
  jclass clazz = env->FindClass("Main");
  CHECK(clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  jobjectArray array = env->NewObjectArray(0, clazz, nullptr);
  CHECK(array != nullptr);
  CHECK(!env->ExceptionCheck());
}

extern "C" JNIEXPORT jint JNICALL Java_Main_getFieldSubclass(JNIEnv* env,
                                                             jclass,
                                                             jobject f_obj,
                                                             jclass sub) {
  jfieldID f = env->FromReflectedField(f_obj);
  return env->GetStaticIntField(sub, f);
}

// http://b/10994325
extern "C" JNIEXPORT void JNICALL Java_Main_testFindClassOnAttachedNativeThread(JNIEnv*, jclass) {
  PthreadHelper(&testFindClassOnAttachedNativeThread);
}

static void testFindFieldOnAttachedNativeThread(JNIEnv* env) {
  jclass clazz = env->FindClass("Main");
  CHECK(clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  jfieldID field = env->GetStaticFieldID(clazz, "testFindFieldOnAttachedNativeThreadField", "Z");
  CHECK(field != nullptr);
  CHECK(!env->ExceptionCheck());

  env->SetStaticBooleanField(clazz, field, JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_Main_testFindFieldOnAttachedNativeThreadNative(JNIEnv*,
                                                                                      jclass) {
  PthreadHelper(&testFindFieldOnAttachedNativeThread);
}

static void testReflectFieldGetFromAttachedNativeThread(JNIEnv* env) {
  jclass clazz = env->FindClass("Main");
  CHECK(clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  jclass class_clazz = env->FindClass("java/lang/Class");
  CHECK(class_clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  jmethodID getFieldMetodId = env->GetMethodID(class_clazz, "getField",
                                               "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
  CHECK(getFieldMetodId != nullptr);
  CHECK(!env->ExceptionCheck());

  jstring field_name = env->NewStringUTF("testReflectFieldGetFromAttachedNativeThreadField");
  CHECK(field_name != nullptr);
  CHECK(!env->ExceptionCheck());

  jobject field = env->CallObjectMethod(clazz, getFieldMetodId, field_name);
  CHECK(field != nullptr);
  CHECK(!env->ExceptionCheck());

  jclass field_clazz = env->FindClass("java/lang/reflect/Field");
  CHECK(field_clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  jmethodID getBooleanMetodId = env->GetMethodID(field_clazz, "getBoolean",
                                                 "(Ljava/lang/Object;)Z");
  CHECK(getBooleanMetodId != nullptr);
  CHECK(!env->ExceptionCheck());

  jboolean value = env->CallBooleanMethod(field, getBooleanMetodId, /* ignored */ clazz);
  CHECK(value == false);
  CHECK(!env->ExceptionCheck());
}

// http://b/15539150
extern "C" JNIEXPORT void JNICALL Java_Main_testReflectFieldGetFromAttachedNativeThreadNative(
    JNIEnv*, jclass) {
  PthreadHelper(&testReflectFieldGetFromAttachedNativeThread);
}


// http://b/11243757
extern "C" JNIEXPORT void JNICALL Java_Main_testCallStaticVoidMethodOnSubClassNative(JNIEnv* env,
                                                                                     jclass) {
  jclass super_class = env->FindClass("Main$testCallStaticVoidMethodOnSubClass_SuperClass");
  CHECK(super_class != nullptr);

  jmethodID execute = env->GetStaticMethodID(super_class, "execute", "()V");
  CHECK(execute != nullptr);

  jclass sub_class = env->FindClass("Main$testCallStaticVoidMethodOnSubClass_SubClass");
  CHECK(sub_class != nullptr);

  env->CallStaticVoidMethod(sub_class, execute);
}

extern "C" JNIEXPORT jobject JNICALL Java_Main_testGetMirandaMethodNative(JNIEnv* env, jclass) {
  jclass abstract_class = env->FindClass("Main$testGetMirandaMethod_MirandaAbstract");
  CHECK(abstract_class != nullptr);
  jmethodID miranda_method = env->GetMethodID(abstract_class, "inInterface", "()Z");
  CHECK(miranda_method != nullptr);
  return env->ToReflectedMethod(abstract_class, miranda_method, JNI_FALSE);
}

// https://code.google.com/p/android/issues/detail?id=63055
extern "C" void JNICALL Java_Main_testZeroLengthByteBuffers(JNIEnv* env, jclass) {
  std::vector<uint8_t> buffer(1);
  jobject byte_buffer = env->NewDirectByteBuffer(&buffer[0], 0);
  CHECK(byte_buffer != nullptr);
  CHECK(!env->ExceptionCheck());

  CHECK_EQ(env->GetDirectBufferAddress(byte_buffer), &buffer[0]);
  CHECK_EQ(env->GetDirectBufferCapacity(byte_buffer), 0);
}

constexpr size_t kByteReturnSize = 7;
jbyte byte_returns[kByteReturnSize] = { 0, 1, 2, 127, -1, -2, -128 };

extern "C" jbyte JNICALL Java_Main_byteMethod(JNIEnv*, jclass, jbyte b1, jbyte b2,
                                              jbyte b3, jbyte b4, jbyte b5, jbyte b6,
                                              jbyte b7, jbyte b8, jbyte b9, jbyte b10) {
  // We use b1 to drive the output.
  CHECK_EQ(b2, 2);
  CHECK_EQ(b3, -3);
  CHECK_EQ(b4, 4);
  CHECK_EQ(b5, -5);
  CHECK_EQ(b6, 6);
  CHECK_EQ(b7, -7);
  CHECK_EQ(b8, 8);
  CHECK_EQ(b9, -9);
  CHECK_EQ(b10, 10);

  CHECK_LE(0, b1);
  CHECK_LT(b1, static_cast<jbyte>(kByteReturnSize));

  return byte_returns[b1];
}

constexpr size_t kShortReturnSize = 9;
jshort short_returns[kShortReturnSize] = { 0, 1, 2, 127, 32767, -1, -2, -128,
    static_cast<jshort>(0x8000) };
// The weird static_cast is because short int is only guaranteed down to -32767, not Java's -32768.

extern "C" jshort JNICALL Java_Main_shortMethod(JNIEnv*, jclass, jshort s1, jshort s2,
                                                jshort s3, jshort s4, jshort s5, jshort s6,
                                                jshort s7, jshort s8, jshort s9, jshort s10) {
  // We use s1 to drive the output.
  CHECK_EQ(s2, 2);
  CHECK_EQ(s3, -3);
  CHECK_EQ(s4, 4);
  CHECK_EQ(s5, -5);
  CHECK_EQ(s6, 6);
  CHECK_EQ(s7, -7);
  CHECK_EQ(s8, 8);
  CHECK_EQ(s9, -9);
  CHECK_EQ(s10, 10);

  CHECK_LE(0, s1);
  CHECK_LT(s1, static_cast<jshort>(kShortReturnSize));

  return short_returns[s1];
}

extern "C" jboolean JNICALL Java_Main_booleanMethod(JNIEnv*, jclass, jboolean b1,
                                                    jboolean b2, jboolean b3, jboolean b4,
                                                    jboolean b5, jboolean b6, jboolean b7,
                                                    jboolean b8, jboolean b9, jboolean b10) {
  // We use b1 to drive the output.
  CHECK_EQ(b2, JNI_TRUE);
  CHECK_EQ(b3, JNI_FALSE);
  CHECK_EQ(b4, JNI_TRUE);
  CHECK_EQ(b5, JNI_FALSE);
  CHECK_EQ(b6, JNI_TRUE);
  CHECK_EQ(b7, JNI_FALSE);
  CHECK_EQ(b8, JNI_TRUE);
  CHECK_EQ(b9, JNI_FALSE);
  CHECK_EQ(b10, JNI_TRUE);

  CHECK(b1 == JNI_TRUE || b1 == JNI_FALSE);
  return b1;
}

constexpr size_t kCharReturnSize = 8;
jchar char_returns[kCharReturnSize] = { 0, 1, 2, 127, 255, 256, 15000, 34000 };

extern "C" jchar JNICALL Java_Main_charMethod(JNIEnv*, jclass, jchar c1, jchar c2,
                                              jchar c3, jchar c4, jchar c5, jchar c6, jchar c7,
                                              jchar c8, jchar c9, jchar c10) {
  // We use c1 to drive the output.
  CHECK_EQ(c2, 'a');
  CHECK_EQ(c3, 'b');
  CHECK_EQ(c4, 'c');
  CHECK_EQ(c5, '0');
  CHECK_EQ(c6, '1');
  CHECK_EQ(c7, '2');
  CHECK_EQ(c8, 1234);
  CHECK_EQ(c9, 2345);
  CHECK_EQ(c10, 3456);

  CHECK_LT(c1, static_cast<jchar>(kCharReturnSize));

  return char_returns[c1];
}

extern "C" JNIEXPORT void JNICALL Java_Main_removeLocalObject(JNIEnv* env, jclass, jclass o) {
  // Delete the arg to see if it crashes.
  env->DeleteLocalRef(o);
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_nativeIsAssignableFrom(JNIEnv* env, jclass,
                                                                       jclass from, jclass to) {
  return env->IsAssignableFrom(from, to);
}

static void testShallowGetCallingClassLoader(JNIEnv* env) {
  // Test direct call.
  {
    jclass vmstack_clazz = env->FindClass("dalvik/system/VMStack");
    CHECK(vmstack_clazz != nullptr);
    CHECK(!env->ExceptionCheck());

    jmethodID getCallingClassLoaderMethodId = env->GetStaticMethodID(vmstack_clazz,
                                                                     "getCallingClassLoader",
                                                                     "()Ljava/lang/ClassLoader;");
    CHECK(getCallingClassLoaderMethodId != nullptr);
    CHECK(!env->ExceptionCheck());

    jobject class_loader = env->CallStaticObjectMethod(vmstack_clazz,
                                                       getCallingClassLoaderMethodId);
    CHECK(class_loader == nullptr);
    CHECK(!env->ExceptionCheck());
  }

  // Test one-level call. Use System.loadLibrary().
  {
    jclass system_clazz = env->FindClass("java/lang/System");
    CHECK(system_clazz != nullptr);
    CHECK(!env->ExceptionCheck());

    jmethodID loadLibraryMethodId = env->GetStaticMethodID(system_clazz, "loadLibrary",
                                                           "(Ljava/lang/String;)V");
    CHECK(loadLibraryMethodId != nullptr);
    CHECK(!env->ExceptionCheck());

    // Create a string object.
    jobject library_string = env->NewStringUTF("non_existing_library");
    CHECK(library_string != nullptr);
    CHECK(!env->ExceptionCheck());

    env->CallStaticVoidMethod(system_clazz, loadLibraryMethodId, library_string);
    CHECK(env->ExceptionCheck());

    // We expect UnsatisfiedLinkError.
    jthrowable thrown = env->ExceptionOccurred();
    env->ExceptionClear();

    jclass unsatisfied_link_error_clazz = env->FindClass("java/lang/UnsatisfiedLinkError");
    jclass thrown_class = env->GetObjectClass(thrown);
    CHECK(env->IsSameObject(unsatisfied_link_error_clazz, thrown_class));
  }
}

// http://b/16867274
extern "C" JNIEXPORT void JNICALL Java_Main_nativeTestShallowGetCallingClassLoader(JNIEnv*,
                                                                                   jclass) {
  PthreadHelper(&testShallowGetCallingClassLoader);
}

static void testShallowGetStackClass2(JNIEnv* env) {
  jclass vmstack_clazz = env->FindClass("dalvik/system/VMStack");
  CHECK(vmstack_clazz != nullptr);
  CHECK(!env->ExceptionCheck());

  // Test direct call.
  {
    jmethodID getStackClass2MethodId = env->GetStaticMethodID(vmstack_clazz, "getStackClass2",
                                                              "()Ljava/lang/Class;");
    CHECK(getStackClass2MethodId != nullptr);
    CHECK(!env->ExceptionCheck());

    jobject caller_class = env->CallStaticObjectMethod(vmstack_clazz, getStackClass2MethodId);
    CHECK(caller_class == nullptr);
    CHECK(!env->ExceptionCheck());
  }

  // Test one-level call. Use VMStack.getStackClass1().
  {
    jmethodID getStackClass1MethodId = env->GetStaticMethodID(vmstack_clazz, "getStackClass1",
                                                              "()Ljava/lang/Class;");
    CHECK(getStackClass1MethodId != nullptr);
    CHECK(!env->ExceptionCheck());

    jobject caller_class = env->CallStaticObjectMethod(vmstack_clazz, getStackClass1MethodId);
    CHECK(caller_class == nullptr);
    CHECK(!env->ExceptionCheck());
  }

  // For better testing we would need to compile against libcore and have a two-deep stack
  // ourselves.
}

extern "C" JNIEXPORT void JNICALL Java_Main_nativeTestShallowGetStackClass2(JNIEnv*, jclass) {
  PthreadHelper(&testShallowGetStackClass2);
}

class JniCallNonvirtualVoidMethodTest {
 public:
  explicit JniCallNonvirtualVoidMethodTest(JNIEnv* env)
      : env_(env),
        check_jni_ri_(true),
        check_jni_android_(true),
        super_(GetClass("JniCallNonvirtualTest")),
        sub_(GetClass("JniCallNonvirtualTestSubclass")),
        super_constructor_(GetMethodID(super_, true, "<init>")),
        super_static_(GetMethodID(super_, false, "staticMethod")),
        super_nonstatic_(GetMethodID(super_, true, "nonstaticMethod")),
        sub_constructor_(GetMethodID(sub_, true, "<init>")),
        sub_static_(GetMethodID(sub_, false, "staticMethod")),
        sub_nonstatic_(GetMethodID(sub_, true, "nonstaticMethod")),
        super_field_(GetFieldID(super_, "nonstaticMethodSuperCalled")),
        sub_field_(GetFieldID(super_, "nonstaticMethodSubCalled")) {}

  void Test() {
    TestStaticCallNonvirtualMethod();
    TestNewObject();
    TestnonstaticCallNonvirtualMethod();
  }

  JNIEnv* const env_;

  bool const check_jni_ri_;
  bool const check_jni_android_;

  jclass const super_;
  jclass const sub_;

  jmethodID const super_constructor_;
  jmethodID const super_static_;
  jmethodID const super_nonstatic_;
  jmethodID const sub_constructor_;
  jmethodID const sub_static_;
  jmethodID const sub_nonstatic_;

  jfieldID const super_field_;
  jfieldID const sub_field_;

 private:
  jclass GetClass(const char* class_name) {
    jclass c = env_->FindClass(class_name);
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->FatalError(__FUNCTION__);
    }
    CHECK(!env_->ExceptionCheck());
    CHECK(c != nullptr);
    return c;
  }

  jmethodID GetMethodID(jclass c, bool nonstatic, const char* method_name) {
    jmethodID m = ((nonstatic) ?
                   env_->GetMethodID(c, method_name, "()V") :
                   env_->GetStaticMethodID(c, method_name, "()V"));
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->FatalError(__FUNCTION__);
    }
    CHECK(m != nullptr);
    return m;
  }

  jobject CallConstructor(jclass c, jmethodID m) {
    jobject o = env_->NewObject(c, m);
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->FatalError(__FUNCTION__);
    }
    CHECK(o != nullptr);
    return o;
  }

  void CallMethod(jobject o, jclass c, jmethodID m, bool nonstatic, const char* test_case) {
    printf("RUNNING %s\n", test_case);
    env_->CallNonvirtualVoidMethod(o, c, m);
    bool exception_check = env_->ExceptionCheck();
    if (c == nullptr || !nonstatic) {
      if (!exception_check) {
        printf("FAILED %s due to missing exception\n", test_case);
        env_->FatalError("Expected NullPointerException with null jclass");
      }
      env_->ExceptionClear();
    } else if (exception_check) {
      printf("FAILED %s due to pending exception\n", test_case);
      env_->ExceptionDescribe();
      env_->FatalError(test_case);
    }
    printf("PASSED %s\n", test_case);
  }

  jfieldID GetFieldID(jclass c, const char* field_name) {
    jfieldID m = env_->GetFieldID(c, field_name, "Z");
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->FatalError(__FUNCTION__);
    }
    CHECK(m != nullptr);
    return m;
  }

  jboolean GetBooleanField(jobject o, jfieldID f) {
    jboolean b = env_->GetBooleanField(o, f);
    if (env_->ExceptionCheck()) {
      env_->ExceptionDescribe();
      env_->FatalError(__FUNCTION__);
    }
    return b;
  }

  void TestStaticCallNonvirtualMethod() {
    if (!check_jni_ri_&& !check_jni_android_) {
      CallMethod(nullptr, nullptr, super_static_, false, "null object, null class, super static");
    }
    if (!check_jni_android_) {
      CallMethod(nullptr, super_, super_static_, false, "null object, super class, super static");
    }
    if (!check_jni_android_) {
      CallMethod(nullptr, sub_, super_static_, false, "null object, sub class, super static");
    }

    if (!check_jni_ri_ && !check_jni_android_) {
      CallMethod(nullptr, nullptr, sub_static_, false, "null object, null class, sub static");
    }
    if (!check_jni_android_) {
      CallMethod(nullptr, sub_, sub_static_, false, "null object, super class, sub static");
    }
    if (!check_jni_android_) {
      CallMethod(nullptr, super_, sub_static_, false, "null object, super class, sub static");
    }
  }

  void TestNewObject() {
    jobject super_super = CallConstructor(super_, super_constructor_);
    jobject super_sub = CallConstructor(super_, sub_constructor_);
    jobject sub_super = CallConstructor(sub_, super_constructor_);
    jobject sub_sub = CallConstructor(sub_, sub_constructor_);

    CHECK(env_->IsInstanceOf(super_super, super_));
    CHECK(!env_->IsInstanceOf(super_super, sub_));

    // Note that even though we called (and ran) the subclass
    // constructor, we are not the subclass.
    CHECK(env_->IsInstanceOf(super_sub, super_));
    CHECK(!env_->IsInstanceOf(super_sub, sub_));

    // Note that even though we called the superclass constructor, we
    // are still the subclass.
    CHECK(env_->IsInstanceOf(sub_super, super_));
    CHECK(env_->IsInstanceOf(sub_super, sub_));

    CHECK(env_->IsInstanceOf(sub_sub, super_));
    CHECK(env_->IsInstanceOf(sub_sub, sub_));
  }

  void TestnonstaticCallNonvirtualMethod(bool super_object, bool super_class, bool super_method, const char* test_case) {
    if (check_jni_android_) {
      if (super_object && !super_method) {
        return;  // We don't allow a call with sub class method on the super class instance.
      }
      if (super_class && !super_method) {
        return;  // We don't allow a call with the sub class method with the super class argument.
      }
    }
    jobject o = ((super_object) ?
                 CallConstructor(super_, super_constructor_) :
                 CallConstructor(sub_, sub_constructor_));
    jclass c = (super_class) ? super_ : sub_;
    jmethodID m = (super_method) ? super_nonstatic_ : sub_nonstatic_;
    CallMethod(o, c, m, true, test_case);
    jboolean super_field = GetBooleanField(o, super_field_);
    jboolean sub_field = GetBooleanField(o, sub_field_);
    CHECK_EQ(super_field, super_method);
    CHECK_NE(sub_field, super_method);
  }

  void TestnonstaticCallNonvirtualMethod() {
    TestnonstaticCallNonvirtualMethod(true, true, true, "super object, super class, super nonstatic");
    TestnonstaticCallNonvirtualMethod(true, false, true, "super object, sub class, super nonstatic");
    TestnonstaticCallNonvirtualMethod(true, false, false, "super object, sub class, sub nonstatic");
    TestnonstaticCallNonvirtualMethod(true, true, false, "super object, super class, sub nonstatic");

    TestnonstaticCallNonvirtualMethod(false, true, true, "sub object, super class, super nonstatic");
    TestnonstaticCallNonvirtualMethod(false, false, true, "sub object, sub class, super nonstatic");
    TestnonstaticCallNonvirtualMethod(false, false, false, "sub object, sub class, sub nonstatic");
    TestnonstaticCallNonvirtualMethod(false, true, false, "sub object, super class, sub nonstatic");
  }
};

extern "C" void JNICALL Java_Main_testCallNonvirtual(JNIEnv* env, jclass) {
  JniCallNonvirtualVoidMethodTest(env).Test();
}

extern "C" JNIEXPORT void JNICALL Java_Main_testNewStringObject(JNIEnv* env, jclass) {
  jclass c = env->FindClass("java/lang/String");
  CHECK(c != nullptr);

  jmethodID mid1 = env->GetMethodID(c, "<init>", "()V");
  CHECK(mid1 != nullptr);
  CHECK(!env->ExceptionCheck());
  jmethodID mid2 = env->GetMethodID(c, "<init>", "([B)V");
  CHECK(mid2 != nullptr);
  CHECK(!env->ExceptionCheck());
  jmethodID mid3 = env->GetMethodID(c, "<init>", "([C)V");
  CHECK(mid3 != nullptr);
  CHECK(!env->ExceptionCheck());
  jmethodID mid4 = env->GetMethodID(c, "<init>", "(Ljava/lang/String;)V");
  CHECK(mid4 != nullptr);
  CHECK(!env->ExceptionCheck());

  const char* test_array = "Test";
  int byte_array_length = strlen(test_array);
  jbyteArray byte_array = env->NewByteArray(byte_array_length);
  env->SetByteArrayRegion(byte_array, 0, byte_array_length, reinterpret_cast<const jbyte*>(test_array));

  // Test NewObject
  jstring s = reinterpret_cast<jstring>(env->NewObject(c, mid2, byte_array));
  CHECK(s != nullptr);
  CHECK_EQ(env->GetStringLength(s), byte_array_length);
  CHECK_EQ(env->GetStringUTFLength(s), byte_array_length);
  const char* chars = env->GetStringUTFChars(s, nullptr);
  CHECK_EQ(strcmp(test_array, chars), 0);
  env->ReleaseStringUTFChars(s, chars);

  // Test AllocObject and Call(Nonvirtual)VoidMethod
  jstring s1 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s1 != nullptr);
  jstring s2 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s2 != nullptr);
  jstring s3 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s3 != nullptr);
  jstring s4 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s4 != nullptr);

  jcharArray char_array = env->NewCharArray(5);
  jstring string_arg = env->NewStringUTF("helloworld");

  // With Var Args
  env->CallVoidMethod(s1, mid1);
  env->CallNonvirtualVoidMethod(s2, c, mid2, byte_array);

  // With JValues
  jvalue args3[1];
  args3[0].l = char_array;
  jvalue args4[1];
  args4[0].l = string_arg;
  env->CallVoidMethodA(s3, mid3, args3);
  env->CallNonvirtualVoidMethodA(s4, c, mid4, args4);

  // Test with global and weak global references
  jstring s5 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s5 != nullptr);
  s5 = reinterpret_cast<jstring>(env->NewGlobalRef(s5));
  jstring s6 = reinterpret_cast<jstring>(env->AllocObject(c));
  CHECK(s6 != nullptr);
  s6 = reinterpret_cast<jstring>(env->NewWeakGlobalRef(s6));

  env->CallVoidMethod(s5, mid1);
  env->CallNonvirtualVoidMethod(s6, c, mid2, byte_array);
  CHECK_EQ(env->GetStringLength(s5), 0);
  CHECK_EQ(env->GetStringLength(s6), byte_array_length);
  const char* chars6 = env->GetStringUTFChars(s6, nullptr);
  CHECK_EQ(strcmp(test_array, chars6), 0);
  env->ReleaseStringUTFChars(s6, chars6);
}

extern "C" JNIEXPORT jlong JNICALL Java_Main_testGetMethodID(JNIEnv* env, jclass, jclass c) {
  return reinterpret_cast<jlong>(env->GetMethodID(c, "a", "()V"));
}

extern "C" JNIEXPORT void JNICALL Java_Main_enterJniCriticalSection(JNIEnv* env, jclass,
                                                                    jint arraySize,
                                                                    jbyteArray array0,
                                                                    jbyteArray array1) {
  for (int i = 0; i < 50000; ++i) {
    char* data0 = reinterpret_cast<char*>(env->GetPrimitiveArrayCritical(array0, nullptr));
    char* data1 = reinterpret_cast<char*>(env->GetPrimitiveArrayCritical(array1, nullptr));
    bool up = i % 2 == 0;
    for (int j = 0; j < arraySize; ++j) {
      if (up) {
        data1[j] = data0[j] + 1;
      } else {
        data0[j] = data1[j] + 1;
      }
    }
    env->ReleasePrimitiveArrayCritical(array1, data1, 0);
    env->ReleasePrimitiveArrayCritical(array0, data0, 0);
  }
}

class JniCallDefaultMethodsTest {
 public:
  explicit JniCallDefaultMethodsTest(JNIEnv* env)
      : env_(env), concrete_class_(env_->FindClass("ConcreteClass")) {
    CHECK(!env_->ExceptionCheck());
    CHECK(concrete_class_ != nullptr);
  }

  void Test() {
    TestCalls("ConcreteClass", { "JniCallNonOverridenDefaultMethod",
                                 "JniCallOverridenDefaultMethod",
                                 "JniCallOverridenDefaultMethodWithSuper",
                                 "JniCallOverridenAbstractMethod",
                                 "JniCallConflictDefaultMethod",
                                 "JniCallSoftConflictMethod" });
    TestCalls("DefaultInterface", { "JniCallNonOverridenDefaultMethod",
                                    "JniCallOverridenDefaultMethod",
                                    "JniCallOverridenAbstractMethod",
                                    "JniCallConflictDefaultMethod",
                                    "JniCallSoftConflictMethod" });
    TestCalls("AbstractInterface", { "JniCallSoftConflictMethod" });
    TestCalls("ConflictInterface", { "JniCallConflictDefaultMethod" });
  }

 private:
  void TestCalls(const char* declaring_class, std::vector<const char*> methods) {
    jmethodID new_method = env_->GetMethodID(concrete_class_, "<init>", "()V");
    jobject obj = env_->NewObject(concrete_class_, new_method);
    CHECK(!env_->ExceptionCheck());
    CHECK(obj != nullptr);
    jclass decl_class = env_->FindClass(declaring_class);
    CHECK(!env_->ExceptionCheck());
    CHECK(decl_class != nullptr);
    for (const char* method : methods) {
      jmethodID method_id = env_->GetMethodID(decl_class, method, "()V");
      CHECK(!env_->ExceptionCheck());
      printf("Calling method %s->%s on object of type ConcreteClass\n", declaring_class, method);
      env_->CallVoidMethod(obj, method_id);
      if (env_->ExceptionCheck()) {
        jthrowable thrown = env_->ExceptionOccurred();
        env_->ExceptionClear();
        jmethodID to_string = env_->GetMethodID(
            env_->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
        jstring exception_string = (jstring) env_->CallObjectMethod(thrown, to_string);
        CHECK(!env_->ExceptionCheck());
        const char* exception_string_utf8 = env_->GetStringUTFChars(exception_string, nullptr);
        CHECK(!env_->ExceptionCheck());
        CHECK(exception_string_utf8 != nullptr);
        printf("EXCEPTION OCCURED: %s\n", exception_string_utf8);
        env_->ReleaseStringUTFChars(exception_string, exception_string_utf8);
      }
    }
  }

  JNIEnv* env_;
  jclass concrete_class_;
};

extern "C" JNIEXPORT void JNICALL Java_Main_testCallDefaultMethods(JNIEnv* env) {
  JniCallDefaultMethodsTest(env).Test();
}

static void InvokeSpecificMethod(JNIEnv* env, jobject obj, const char* method) {
  jclass lambda_class = env->FindClass("LambdaInterface");
  CHECK(!env->ExceptionCheck());
  CHECK(lambda_class != nullptr);
  jmethodID method_id = env->GetMethodID(lambda_class, method, "()V");
  CHECK(!env->ExceptionCheck());
  env->CallVoidMethod(obj, method_id);
  CHECK(!env->ExceptionCheck());
}

extern "C" JNIEXPORT void JNICALL Java_Main_testInvokeLambdaDefaultMethod(
    JNIEnv* e, jclass, jobject l) {
  InvokeSpecificMethod(e, l, "sayHiTwice");
}

extern "C" JNIEXPORT void JNICALL Java_Main_testInvokeLambdaMethod(JNIEnv* e, jclass, jobject l) {
  InvokeSpecificMethod(e, l, "sayHi");
}

// Register on-demand because many tests share this JNI library and
// we can't unconditionally register them.
extern "C" JNIEXPORT jboolean JNICALL Java_Main_registerNativesJniTest(JNIEnv* e, jclass kls) {
  const size_t numMethods = sizeof(sMainMethods)/sizeof(JNINativeMethod);

  if (e->RegisterNatives(kls, sMainMethods, numMethods) < 0) {
      std::cerr << "RegisterNatives failed for 'Main'" << std::endl;
      return JNI_FALSE;
  }

  return JNI_TRUE;
}

// Annotated with @FastNative in Java code. Doesn't need to be explicitly registered with "!".
// NOTE: Has to be registered explicitly to avoid mutator lock check failures.
static jint Java_Main_intFastNativeMethod(JNIEnv*, jclass, jint a, jint b, jint c) {
  return a + b + c;
}

// Annotated with @CriticalNative in Java code. Doesn't need to be explicitly registered with "!".
// NOTE: Has to be registered explicitly to avoid mutator lock check failures.
static jint Java_Main_intCriticalNativeMethod(jint a, jint b, jint c) {
  // Note that unlike a "Fast Native" method this excludes JNIEnv and the jclass parameters.
  return a + b + c;
}

extern "C" JNIEXPORT jobject JNICALL Java_Main_lookupClinit(JNIEnv* env, jclass, jclass kls) {
  jmethodID clinit_id = env->GetStaticMethodID(kls, "<clinit>", "()V");

  if (clinit_id != nullptr) {
    jobject obj = env->ToReflectedMethod(kls, clinit_id, /*isStatic*/ true);
    CHECK(obj != nullptr);
    return obj;
  } else {
    return nullptr;
  }
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isSlowDebug(JNIEnv*, jclass) {
  // Return whether slow-debug is on. Only relevant for debug builds.
  if (kIsDebugBuild) {
    // Register a dummy flag and get the default value it should be initialized with.
    static bool dummy_flag = false;
    dummy_flag = RegisterRuntimeDebugFlag(&dummy_flag);

    return dummy_flag ? JNI_TRUE : JNI_FALSE;
  }
  // To pass the Java-side test, just so "on" for release builds.
  return JNI_TRUE;
}

}  // namespace art

