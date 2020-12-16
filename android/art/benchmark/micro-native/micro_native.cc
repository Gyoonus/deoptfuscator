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

#include <jni.h>
#include <stdio.h>

#ifndef NATIVE_METHOD
#define NATIVE_METHOD(className, functionName, signature) \
    { #functionName, signature, reinterpret_cast<void*>(className ## _ ## functionName) }
#endif
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define GLUE4(a, b, c, d) a ## b ## c ## d
#define GLUE4_(a, b, c, d) GLUE4(a, b, c, d)

#define CLASS_NAME "benchmarks/MicroNative/java/NativeMethods"
#define CLASS_INFIX benchmarks_MicroNative_java_NativeMethods

#define NAME_NORMAL_JNI_METHOD(name) GLUE4_(Java_, CLASS_INFIX, _, name)
#define NAME_CRITICAL_JNI_METHOD(name) GLUE4_(JavaCritical_, CLASS_INFIX, _, name)

#define DEFINE_NORMAL_JNI_METHOD(ret, name) extern "C" JNIEXPORT ret JNICALL GLUE4_(Java_, CLASS_INFIX, _, name)
#define DEFINE_CRITICAL_JNI_METHOD(ret, name) extern "C" JNIEXPORT ret JNICALL GLUE4_(JavaCritical_, CLASS_INFIX, _, name)

static void NativeMethods_emptyJniStaticSynchronizedMethod0(JNIEnv*, jclass) { }
static void NativeMethods_emptyJniSynchronizedMethod0(JNIEnv*, jclass) { }

static JNINativeMethod gMethods_NormalOnly[] = {
  NATIVE_METHOD(NativeMethods, emptyJniStaticSynchronizedMethod0, "()V"),
  NATIVE_METHOD(NativeMethods, emptyJniSynchronizedMethod0, "()V"),
};

static void NativeMethods_emptyJniMethod0(JNIEnv*, jobject) { }
static void NativeMethods_emptyJniMethod6(JNIEnv*, jobject, int, int, int, int, int, int) { }
static void NativeMethods_emptyJniMethod6L(JNIEnv*, jobject, jobject, jarray, jarray, jobject,
                                           jarray, jarray) { }
static void NativeMethods_emptyJniStaticMethod6L(JNIEnv*, jclass, jobject, jarray, jarray, jobject,
                                                 jarray, jarray) { }

static void NativeMethods_emptyJniStaticMethod0(JNIEnv*, jclass) { }
static void NativeMethods_emptyJniStaticMethod6(JNIEnv*, jclass, int, int, int, int, int, int) { }

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(NativeMethods, emptyJniMethod0, "()V"),
  NATIVE_METHOD(NativeMethods, emptyJniMethod6, "(IIIIII)V"),
  NATIVE_METHOD(NativeMethods, emptyJniMethod6L, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod6L, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod0, "()V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod6, "(IIIIII)V"),
};

static void NativeMethods_emptyJniMethod0_Fast(JNIEnv*, jobject) { }
static void NativeMethods_emptyJniMethod6_Fast(JNIEnv*, jobject, int, int, int, int, int, int) { }
static void NativeMethods_emptyJniMethod6L_Fast(JNIEnv*, jobject, jobject, jarray, jarray, jobject,
                                                jarray, jarray) { }
static void NativeMethods_emptyJniStaticMethod6L_Fast(JNIEnv*, jclass, jobject, jarray, jarray,
                                                      jobject, jarray, jarray) { }

static void NativeMethods_emptyJniStaticMethod0_Fast(JNIEnv*, jclass) { }
static void NativeMethods_emptyJniStaticMethod6_Fast(JNIEnv*, jclass, int, int, int, int, int, int) { }

static JNINativeMethod gMethods_Fast[] = {
  NATIVE_METHOD(NativeMethods, emptyJniMethod0_Fast, "()V"),
  NATIVE_METHOD(NativeMethods, emptyJniMethod6_Fast, "(IIIIII)V"),
  NATIVE_METHOD(NativeMethods, emptyJniMethod6L_Fast, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod6L_Fast, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod0_Fast, "()V"),
  NATIVE_METHOD(NativeMethods, emptyJniStaticMethod6_Fast, "(IIIIII)V"),
};

// Have both a Java_ and a JavaCritical_ version of the same empty method.
// The runtime automatically selects the right one when doing a dlsym-based native lookup.
DEFINE_NORMAL_JNI_METHOD(void,   emptyJniStaticMethod0_1Critical)(JNIEnv*, jclass) { }
DEFINE_CRITICAL_JNI_METHOD(void, emptyJniStaticMethod0_1Critical)() { }
DEFINE_NORMAL_JNI_METHOD(void,   emptyJniStaticMethod6_1Critical)(JNIEnv*, jclass, int, int, int, int, int, int) { }
DEFINE_CRITICAL_JNI_METHOD(void, emptyJniStaticMethod6_1Critical)(int, int, int, int, int, int) { }

static JNINativeMethod gMethods_Critical[] = {
  // Don't use NATIVE_METHOD because the name is mangled differently.
  { "emptyJniStaticMethod0_Critical", "()V",
        reinterpret_cast<void*>(NAME_CRITICAL_JNI_METHOD(emptyJniStaticMethod0_1Critical)) },
  { "emptyJniStaticMethod6_Critical", "(IIIIII)V",
        reinterpret_cast<void*>(NAME_CRITICAL_JNI_METHOD(emptyJniStaticMethod6_1Critical)) }
};

void jniRegisterNativeMethods(JNIEnv* env,
                              const char* className,
                              const JNINativeMethod* methods,
                              int numMethods) {
    jclass c = env->FindClass(className);
    if (c == nullptr) {
        char* tmp;
        const char* msg;
        if (asprintf(&tmp,
                     "Native registration unable to find class '%s'; aborting...",
                     className) == -1) {
            // Allocation failed, print default warning.
            msg = "Native registration unable to find class; aborting...";
        } else {
            msg = tmp;
        }
        env->FatalError(msg);
    }

    if (env->RegisterNatives(c, methods, numMethods) < 0) {
        char* tmp;
        const char* msg;
        if (asprintf(&tmp, "RegisterNatives failed for '%s'; aborting...", className) == -1) {
            // Allocation failed, print default warning.
            msg = "RegisterNatives failed; aborting...";
        } else {
            msg = tmp;
        }
        env->FatalError(msg);
    }
}

void register_micro_native_methods(JNIEnv* env) {
  jniRegisterNativeMethods(env, CLASS_NAME, gMethods_NormalOnly, NELEM(gMethods_NormalOnly));
  jniRegisterNativeMethods(env, CLASS_NAME, gMethods, NELEM(gMethods));
  jniRegisterNativeMethods(env, CLASS_NAME, gMethods_Fast, NELEM(gMethods_Fast));

  if (env->FindClass("dalvik/annotation/optimization/CriticalNative") != nullptr) {
    // Only register them explicitly if the annotation is present.
    jniRegisterNativeMethods(env, CLASS_NAME, gMethods_Critical, NELEM(gMethods_Critical));
  } else {
    if (env->ExceptionCheck()) {
      // It will throw NoClassDefFoundError
      env->ExceptionClear();
    }
  }
  // else let them be registered implicitly.
}
