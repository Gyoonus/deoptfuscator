/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "well_known_classes.h"

#include <stdlib.h>

#include <sstream>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "hidden_api.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "mirror/throwable.h"
#include "nativehelper/scoped_local_ref.h"
#include "obj_ptr-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

jclass WellKnownClasses::dalvik_annotation_optimization_CriticalNative;
jclass WellKnownClasses::dalvik_annotation_optimization_FastNative;
jclass WellKnownClasses::dalvik_system_BaseDexClassLoader;
jclass WellKnownClasses::dalvik_system_DelegateLastClassLoader;
jclass WellKnownClasses::dalvik_system_DexClassLoader;
jclass WellKnownClasses::dalvik_system_DexFile;
jclass WellKnownClasses::dalvik_system_DexPathList;
jclass WellKnownClasses::dalvik_system_DexPathList__Element;
jclass WellKnownClasses::dalvik_system_EmulatedStackFrame;
jclass WellKnownClasses::dalvik_system_PathClassLoader;
jclass WellKnownClasses::dalvik_system_VMRuntime;
jclass WellKnownClasses::java_lang_annotation_Annotation__array;
jclass WellKnownClasses::java_lang_BootClassLoader;
jclass WellKnownClasses::java_lang_ClassLoader;
jclass WellKnownClasses::java_lang_ClassNotFoundException;
jclass WellKnownClasses::java_lang_Daemons;
jclass WellKnownClasses::java_lang_Error;
jclass WellKnownClasses::java_lang_invoke_MethodHandle;
jclass WellKnownClasses::java_lang_invoke_VarHandle;
jclass WellKnownClasses::java_lang_IllegalAccessError;
jclass WellKnownClasses::java_lang_NoClassDefFoundError;
jclass WellKnownClasses::java_lang_Object;
jclass WellKnownClasses::java_lang_OutOfMemoryError;
jclass WellKnownClasses::java_lang_reflect_Constructor;
jclass WellKnownClasses::java_lang_reflect_Executable;
jclass WellKnownClasses::java_lang_reflect_Field;
jclass WellKnownClasses::java_lang_reflect_Method;
jclass WellKnownClasses::java_lang_reflect_Parameter;
jclass WellKnownClasses::java_lang_reflect_Parameter__array;
jclass WellKnownClasses::java_lang_reflect_Proxy;
jclass WellKnownClasses::java_lang_RuntimeException;
jclass WellKnownClasses::java_lang_StackOverflowError;
jclass WellKnownClasses::java_lang_String;
jclass WellKnownClasses::java_lang_StringFactory;
jclass WellKnownClasses::java_lang_System;
jclass WellKnownClasses::java_lang_Thread;
jclass WellKnownClasses::java_lang_ThreadGroup;
jclass WellKnownClasses::java_lang_Throwable;
jclass WellKnownClasses::java_nio_ByteBuffer;
jclass WellKnownClasses::java_nio_DirectByteBuffer;
jclass WellKnownClasses::java_util_ArrayList;
jclass WellKnownClasses::java_util_Collections;
jclass WellKnownClasses::java_util_function_Consumer;
jclass WellKnownClasses::libcore_reflect_AnnotationFactory;
jclass WellKnownClasses::libcore_reflect_AnnotationMember;
jclass WellKnownClasses::libcore_util_EmptyArray;
jclass WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk;
jclass WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer;

jmethodID WellKnownClasses::dalvik_system_BaseDexClassLoader_getLdLibraryPath;
jmethodID WellKnownClasses::dalvik_system_VMRuntime_runFinalization;
jmethodID WellKnownClasses::java_lang_Boolean_valueOf;
jmethodID WellKnownClasses::java_lang_Byte_valueOf;
jmethodID WellKnownClasses::java_lang_Character_valueOf;
jmethodID WellKnownClasses::java_lang_ClassLoader_loadClass;
jmethodID WellKnownClasses::java_lang_ClassNotFoundException_init;
jmethodID WellKnownClasses::java_lang_Daemons_requestHeapTrim;
jmethodID WellKnownClasses::java_lang_Daemons_start;
jmethodID WellKnownClasses::java_lang_Daemons_stop;
jmethodID WellKnownClasses::java_lang_Double_valueOf;
jmethodID WellKnownClasses::java_lang_Float_valueOf;
jmethodID WellKnownClasses::java_lang_Integer_valueOf;
jmethodID WellKnownClasses::java_lang_invoke_MethodHandle_invoke;
jmethodID WellKnownClasses::java_lang_invoke_MethodHandle_invokeExact;
jmethodID WellKnownClasses::java_lang_invoke_MethodHandles_lookup;
jmethodID WellKnownClasses::java_lang_invoke_MethodHandles_Lookup_findConstructor;
jmethodID WellKnownClasses::java_lang_Long_valueOf;
jmethodID WellKnownClasses::java_lang_ref_FinalizerReference_add;
jmethodID WellKnownClasses::java_lang_ref_ReferenceQueue_add;
jmethodID WellKnownClasses::java_lang_reflect_Parameter_init;
jmethodID WellKnownClasses::java_lang_reflect_Proxy_invoke;
jmethodID WellKnownClasses::java_lang_Runtime_nativeLoad;
jmethodID WellKnownClasses::java_lang_Short_valueOf;
jmethodID WellKnownClasses::java_lang_String_charAt;
jmethodID WellKnownClasses::java_lang_System_runFinalization = nullptr;
jmethodID WellKnownClasses::java_lang_Thread_dispatchUncaughtException;
jmethodID WellKnownClasses::java_lang_Thread_init;
jmethodID WellKnownClasses::java_lang_Thread_run;
jmethodID WellKnownClasses::java_lang_ThreadGroup_add;
jmethodID WellKnownClasses::java_lang_ThreadGroup_removeThread;
jmethodID WellKnownClasses::java_nio_DirectByteBuffer_init;
jmethodID WellKnownClasses::java_util_function_Consumer_accept;
jmethodID WellKnownClasses::libcore_reflect_AnnotationFactory_createAnnotation;
jmethodID WellKnownClasses::libcore_reflect_AnnotationMember_init;
jmethodID WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_broadcast;
jmethodID WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_dispatch;

jfieldID WellKnownClasses::dalvik_system_DexFile_cookie;
jfieldID WellKnownClasses::dalvik_system_DexFile_fileName;
jfieldID WellKnownClasses::dalvik_system_BaseDexClassLoader_pathList;
jfieldID WellKnownClasses::dalvik_system_DexPathList_dexElements;
jfieldID WellKnownClasses::dalvik_system_DexPathList__Element_dexFile;
jfieldID WellKnownClasses::dalvik_system_VMRuntime_nonSdkApiUsageConsumer;
jfieldID WellKnownClasses::java_lang_Thread_daemon;
jfieldID WellKnownClasses::java_lang_Thread_group;
jfieldID WellKnownClasses::java_lang_Thread_lock;
jfieldID WellKnownClasses::java_lang_Thread_name;
jfieldID WellKnownClasses::java_lang_Thread_priority;
jfieldID WellKnownClasses::java_lang_Thread_nativePeer;
jfieldID WellKnownClasses::java_lang_ThreadGroup_groups;
jfieldID WellKnownClasses::java_lang_ThreadGroup_ngroups;
jfieldID WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup;
jfieldID WellKnownClasses::java_lang_ThreadGroup_name;
jfieldID WellKnownClasses::java_lang_ThreadGroup_parent;
jfieldID WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup;
jfieldID WellKnownClasses::java_lang_Throwable_cause;
jfieldID WellKnownClasses::java_lang_Throwable_detailMessage;
jfieldID WellKnownClasses::java_lang_Throwable_stackTrace;
jfieldID WellKnownClasses::java_lang_Throwable_stackState;
jfieldID WellKnownClasses::java_lang_Throwable_suppressedExceptions;
jfieldID WellKnownClasses::java_lang_reflect_Executable_artMethod;
jfieldID WellKnownClasses::java_lang_reflect_Proxy_h;
jfieldID WellKnownClasses::java_nio_ByteBuffer_address;
jfieldID WellKnownClasses::java_nio_ByteBuffer_hb;
jfieldID WellKnownClasses::java_nio_ByteBuffer_isReadOnly;
jfieldID WellKnownClasses::java_nio_ByteBuffer_limit;
jfieldID WellKnownClasses::java_nio_ByteBuffer_offset;
jfieldID WellKnownClasses::java_nio_DirectByteBuffer_capacity;
jfieldID WellKnownClasses::java_nio_DirectByteBuffer_effectiveDirectAddress;
jfieldID WellKnownClasses::java_util_ArrayList_array;
jfieldID WellKnownClasses::java_util_ArrayList_size;
jfieldID WellKnownClasses::java_util_Collections_EMPTY_LIST;
jfieldID WellKnownClasses::libcore_util_EmptyArray_STACK_TRACE_ELEMENT;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_data;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_length;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_offset;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_type;

static jclass CacheClass(JNIEnv* env, const char* jni_class_name) {
  ScopedLocalRef<jclass> c(env, env->FindClass(jni_class_name));
  if (c.get() == nullptr) {
    LOG(FATAL) << "Couldn't find class: " << jni_class_name;
  }
  return reinterpret_cast<jclass>(env->NewGlobalRef(c.get()));
}

static jfieldID CacheField(JNIEnv* env, jclass c, bool is_static,
                           const char* name, const char* signature) {
  jfieldID fid = is_static ? env->GetStaticFieldID(c, name, signature) :
      env->GetFieldID(c, name, signature);
  if (fid == nullptr) {
    ScopedObjectAccess soa(env);
    if (soa.Self()->IsExceptionPending()) {
      LOG(FATAL_WITHOUT_ABORT) << soa.Self()->GetException()->Dump();
    }
    std::ostringstream os;
    WellKnownClasses::ToClass(c)->DumpClass(os, mirror::Class::kDumpClassFullDetail);
    LOG(FATAL) << "Couldn't find field \"" << name << "\" with signature \"" << signature << "\": "
               << os.str();
  }
  return fid;
}

static jmethodID CacheMethod(JNIEnv* env, jclass c, bool is_static,
                             const char* name, const char* signature) {
  jmethodID mid = is_static ? env->GetStaticMethodID(c, name, signature) :
      env->GetMethodID(c, name, signature);
  if (mid == nullptr) {
    ScopedObjectAccess soa(env);
    if (soa.Self()->IsExceptionPending()) {
      LOG(FATAL_WITHOUT_ABORT) << soa.Self()->GetException()->Dump();
    }
    std::ostringstream os;
    WellKnownClasses::ToClass(c)->DumpClass(os, mirror::Class::kDumpClassFullDetail);
    LOG(FATAL) << "Couldn't find method \"" << name << "\" with signature \"" << signature << "\": "
               << os.str();
  }
  return mid;
}

static jmethodID CacheMethod(JNIEnv* env, const char* klass, bool is_static,
                      const char* name, const char* signature) {
  ScopedLocalRef<jclass> java_class(env, env->FindClass(klass));
  return CacheMethod(env, java_class.get(), is_static, name, signature);
}

static jmethodID CachePrimitiveBoxingMethod(JNIEnv* env, char prim_name, const char* boxed_name) {
  ScopedLocalRef<jclass> boxed_class(env, env->FindClass(boxed_name));
  return CacheMethod(env, boxed_class.get(), true, "valueOf",
                     android::base::StringPrintf("(%c)L%s;", prim_name, boxed_name).c_str());
}

#define STRING_INIT_LIST(V) \
  V(java_lang_String_init, "()V", newEmptyString, "newEmptyString", "()Ljava/lang/String;", NewEmptyString) \
  V(java_lang_String_init_B, "([B)V", newStringFromBytes_B, "newStringFromBytes", "([B)Ljava/lang/String;", NewStringFromBytes_B) \
  V(java_lang_String_init_BI, "([BI)V", newStringFromBytes_BI, "newStringFromBytes", "([BI)Ljava/lang/String;", NewStringFromBytes_BI) \
  V(java_lang_String_init_BII, "([BII)V", newStringFromBytes_BII, "newStringFromBytes", "([BII)Ljava/lang/String;", NewStringFromBytes_BII) \
  V(java_lang_String_init_BIII, "([BIII)V", newStringFromBytes_BIII, "newStringFromBytes", "([BIII)Ljava/lang/String;", NewStringFromBytes_BIII) \
  V(java_lang_String_init_BIIString, "([BIILjava/lang/String;)V", newStringFromBytes_BIIString, "newStringFromBytes", "([BIILjava/lang/String;)Ljava/lang/String;", NewStringFromBytes_BIIString) \
  V(java_lang_String_init_BString, "([BLjava/lang/String;)V", newStringFromBytes_BString, "newStringFromBytes", "([BLjava/lang/String;)Ljava/lang/String;", NewStringFromBytes_BString) \
  V(java_lang_String_init_BIICharset, "([BIILjava/nio/charset/Charset;)V", newStringFromBytes_BIICharset, "newStringFromBytes", "([BIILjava/nio/charset/Charset;)Ljava/lang/String;", NewStringFromBytes_BIICharset) \
  V(java_lang_String_init_BCharset, "([BLjava/nio/charset/Charset;)V", newStringFromBytes_BCharset, "newStringFromBytes", "([BLjava/nio/charset/Charset;)Ljava/lang/String;", NewStringFromBytes_BCharset) \
  V(java_lang_String_init_C, "([C)V", newStringFromChars_C, "newStringFromChars", "([C)Ljava/lang/String;", NewStringFromChars_C) \
  V(java_lang_String_init_CII, "([CII)V", newStringFromChars_CII, "newStringFromChars", "([CII)Ljava/lang/String;", NewStringFromChars_CII) \
  V(java_lang_String_init_IIC, "(II[C)V", newStringFromChars_IIC, "newStringFromChars", "(II[C)Ljava/lang/String;", NewStringFromChars_IIC) \
  V(java_lang_String_init_String, "(Ljava/lang/String;)V", newStringFromString, "newStringFromString", "(Ljava/lang/String;)Ljava/lang/String;", NewStringFromString) \
  V(java_lang_String_init_StringBuffer, "(Ljava/lang/StringBuffer;)V", newStringFromStringBuffer, "newStringFromStringBuffer", "(Ljava/lang/StringBuffer;)Ljava/lang/String;", NewStringFromStringBuffer) \
  V(java_lang_String_init_III, "([III)V", newStringFromCodePoints, "newStringFromCodePoints", "([III)Ljava/lang/String;", NewStringFromCodePoints) \
  V(java_lang_String_init_StringBuilder, "(Ljava/lang/StringBuilder;)V", newStringFromStringBuilder, "newStringFromStringBuilder", "(Ljava/lang/StringBuilder;)Ljava/lang/String;", NewStringFromStringBuilder) \

#define STATIC_STRING_INIT(init_runtime_name, init_signature, new_runtime_name, ...) \
    static ArtMethod* init_runtime_name; \
    static ArtMethod* new_runtime_name;
    STRING_INIT_LIST(STATIC_STRING_INIT)
#undef STATIC_STRING_INIT

void WellKnownClasses::InitStringInit(JNIEnv* env) {
  ScopedObjectAccess soa(Thread::Current());
  #define LOAD_STRING_INIT(init_runtime_name, init_signature, new_runtime_name,             \
                           new_java_name, new_signature, ...)                               \
      init_runtime_name = jni::DecodeArtMethod(                                             \
          CacheMethod(env, java_lang_String, false, "<init>", init_signature));             \
      new_runtime_name = jni::DecodeArtMethod(                                              \
          CacheMethod(env, java_lang_StringFactory, true, new_java_name, new_signature));
      STRING_INIT_LIST(LOAD_STRING_INIT)
  #undef LOAD_STRING_INIT
}

void Thread::InitStringEntryPoints() {
  QuickEntryPoints* qpoints = &tlsPtr_.quick_entrypoints;
  #define SET_ENTRY_POINT(init_runtime_name, init_signature, new_runtime_name,              \
                          new_java_name, new_signature, entry_point_name)                   \
      qpoints->p ## entry_point_name = reinterpret_cast<void(*)()>(new_runtime_name);
      STRING_INIT_LIST(SET_ENTRY_POINT)
  #undef SET_ENTRY_POINT
}

ArtMethod* WellKnownClasses::StringInitToStringFactory(ArtMethod* string_init) {
  #define TO_STRING_FACTORY(init_runtime_name, init_signature, new_runtime_name,            \
                            new_java_name, new_signature, entry_point_name)                 \
      if (string_init == (init_runtime_name)) {                                             \
        return (new_runtime_name);                                                          \
      }
      STRING_INIT_LIST(TO_STRING_FACTORY)
  #undef TO_STRING_FACTORY
  LOG(FATAL) << "Could not find StringFactory method for String.<init>";
  return nullptr;
}

uint32_t WellKnownClasses::StringInitToEntryPoint(ArtMethod* string_init) {
  #define TO_ENTRY_POINT(init_runtime_name, init_signature, new_runtime_name,               \
                         new_java_name, new_signature, entry_point_name)                    \
      if (string_init == (init_runtime_name)) {                                             \
        return kQuick ## entry_point_name;                                                  \
      }
      STRING_INIT_LIST(TO_ENTRY_POINT)
  #undef TO_ENTRY_POINT
  LOG(FATAL) << "Could not find StringFactory method for String.<init>";
  return 0;
}
#undef STRING_INIT_LIST

void WellKnownClasses::Init(JNIEnv* env) {
  hiddenapi::ScopedHiddenApiEnforcementPolicySetting hiddenapi_exemption(
      hiddenapi::EnforcementPolicy::kNoChecks);

  dalvik_annotation_optimization_CriticalNative =
      CacheClass(env, "dalvik/annotation/optimization/CriticalNative");
  dalvik_annotation_optimization_FastNative = CacheClass(env, "dalvik/annotation/optimization/FastNative");
  dalvik_system_BaseDexClassLoader = CacheClass(env, "dalvik/system/BaseDexClassLoader");
  dalvik_system_DelegateLastClassLoader = CacheClass(env, "dalvik/system/DelegateLastClassLoader");
  dalvik_system_DexClassLoader = CacheClass(env, "dalvik/system/DexClassLoader");
  dalvik_system_DexFile = CacheClass(env, "dalvik/system/DexFile");
  dalvik_system_DexPathList = CacheClass(env, "dalvik/system/DexPathList");
  dalvik_system_DexPathList__Element = CacheClass(env, "dalvik/system/DexPathList$Element");
  dalvik_system_EmulatedStackFrame = CacheClass(env, "dalvik/system/EmulatedStackFrame");
  dalvik_system_PathClassLoader = CacheClass(env, "dalvik/system/PathClassLoader");
  dalvik_system_VMRuntime = CacheClass(env, "dalvik/system/VMRuntime");

  java_lang_annotation_Annotation__array = CacheClass(env, "[Ljava/lang/annotation/Annotation;");
  java_lang_BootClassLoader = CacheClass(env, "java/lang/BootClassLoader");
  java_lang_ClassLoader = CacheClass(env, "java/lang/ClassLoader");
  java_lang_ClassNotFoundException = CacheClass(env, "java/lang/ClassNotFoundException");
  java_lang_Daemons = CacheClass(env, "java/lang/Daemons");
  java_lang_Object = CacheClass(env, "java/lang/Object");
  java_lang_OutOfMemoryError = CacheClass(env, "java/lang/OutOfMemoryError");
  java_lang_Error = CacheClass(env, "java/lang/Error");
  java_lang_IllegalAccessError = CacheClass(env, "java/lang/IllegalAccessError");
  java_lang_invoke_MethodHandle = CacheClass(env, "java/lang/invoke/MethodHandle");
  java_lang_invoke_VarHandle = CacheClass(env, "java/lang/invoke/VarHandle");
  java_lang_NoClassDefFoundError = CacheClass(env, "java/lang/NoClassDefFoundError");
  java_lang_reflect_Constructor = CacheClass(env, "java/lang/reflect/Constructor");
  java_lang_reflect_Executable = CacheClass(env, "java/lang/reflect/Executable");
  java_lang_reflect_Field = CacheClass(env, "java/lang/reflect/Field");
  java_lang_reflect_Method = CacheClass(env, "java/lang/reflect/Method");
  java_lang_reflect_Parameter = CacheClass(env, "java/lang/reflect/Parameter");
  java_lang_reflect_Parameter__array = CacheClass(env, "[Ljava/lang/reflect/Parameter;");
  java_lang_reflect_Proxy = CacheClass(env, "java/lang/reflect/Proxy");
  java_lang_RuntimeException = CacheClass(env, "java/lang/RuntimeException");
  java_lang_StackOverflowError = CacheClass(env, "java/lang/StackOverflowError");
  java_lang_String = CacheClass(env, "java/lang/String");
  java_lang_StringFactory = CacheClass(env, "java/lang/StringFactory");
  java_lang_System = CacheClass(env, "java/lang/System");
  java_lang_Thread = CacheClass(env, "java/lang/Thread");
  java_lang_ThreadGroup = CacheClass(env, "java/lang/ThreadGroup");
  java_lang_Throwable = CacheClass(env, "java/lang/Throwable");
  java_nio_ByteBuffer = CacheClass(env, "java/nio/ByteBuffer");
  java_nio_DirectByteBuffer = CacheClass(env, "java/nio/DirectByteBuffer");
  java_util_ArrayList = CacheClass(env, "java/util/ArrayList");
  java_util_Collections = CacheClass(env, "java/util/Collections");
  java_util_function_Consumer = CacheClass(env, "java/util/function/Consumer");
  libcore_reflect_AnnotationFactory = CacheClass(env, "libcore/reflect/AnnotationFactory");
  libcore_reflect_AnnotationMember = CacheClass(env, "libcore/reflect/AnnotationMember");
  libcore_util_EmptyArray = CacheClass(env, "libcore/util/EmptyArray");
  org_apache_harmony_dalvik_ddmc_Chunk = CacheClass(env, "org/apache/harmony/dalvik/ddmc/Chunk");
  org_apache_harmony_dalvik_ddmc_DdmServer = CacheClass(env, "org/apache/harmony/dalvik/ddmc/DdmServer");

  dalvik_system_BaseDexClassLoader_getLdLibraryPath = CacheMethod(env, dalvik_system_BaseDexClassLoader, false, "getLdLibraryPath", "()Ljava/lang/String;");
  dalvik_system_VMRuntime_runFinalization = CacheMethod(env, dalvik_system_VMRuntime, true, "runFinalization", "(J)V");
  java_lang_ClassNotFoundException_init = CacheMethod(env, java_lang_ClassNotFoundException, false, "<init>", "(Ljava/lang/String;Ljava/lang/Throwable;)V");
  java_lang_ClassLoader_loadClass = CacheMethod(env, java_lang_ClassLoader, false, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

  java_lang_Daemons_requestHeapTrim = CacheMethod(env, java_lang_Daemons, true, "requestHeapTrim", "()V");
  java_lang_Daemons_start = CacheMethod(env, java_lang_Daemons, true, "start", "()V");
  java_lang_Daemons_stop = CacheMethod(env, java_lang_Daemons, true, "stop", "()V");
  java_lang_invoke_MethodHandle_invoke = CacheMethod(env, java_lang_invoke_MethodHandle, false, "invoke", "([Ljava/lang/Object;)Ljava/lang/Object;");
  java_lang_invoke_MethodHandle_invokeExact = CacheMethod(env, java_lang_invoke_MethodHandle, false, "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;");
  java_lang_invoke_MethodHandles_lookup = CacheMethod(env, "java/lang/invoke/MethodHandles", true, "lookup", "()Ljava/lang/invoke/MethodHandles$Lookup;");
  java_lang_invoke_MethodHandles_Lookup_findConstructor = CacheMethod(env, "java/lang/invoke/MethodHandles$Lookup", false, "findConstructor", "(Ljava/lang/Class;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;");

  java_lang_ref_FinalizerReference_add = CacheMethod(env, "java/lang/ref/FinalizerReference", true, "add", "(Ljava/lang/Object;)V");
  java_lang_ref_ReferenceQueue_add = CacheMethod(env, "java/lang/ref/ReferenceQueue", true, "add", "(Ljava/lang/ref/Reference;)V");

  java_lang_reflect_Parameter_init = CacheMethod(env, java_lang_reflect_Parameter, false, "<init>", "(Ljava/lang/String;ILjava/lang/reflect/Executable;I)V");
  java_lang_String_charAt = CacheMethod(env, java_lang_String, false, "charAt", "(I)C");
  java_lang_Thread_dispatchUncaughtException = CacheMethod(env, java_lang_Thread, false, "dispatchUncaughtException", "(Ljava/lang/Throwable;)V");
  java_lang_Thread_init = CacheMethod(env, java_lang_Thread, false, "<init>", "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V");
  java_lang_Thread_run = CacheMethod(env, java_lang_Thread, false, "run", "()V");
  java_lang_ThreadGroup_add = CacheMethod(env, java_lang_ThreadGroup, false, "add", "(Ljava/lang/Thread;)V");
  java_lang_ThreadGroup_removeThread = CacheMethod(env, java_lang_ThreadGroup, false, "threadTerminated", "(Ljava/lang/Thread;)V");
  java_nio_DirectByteBuffer_init = CacheMethod(env, java_nio_DirectByteBuffer, false, "<init>", "(JI)V");
  java_util_function_Consumer_accept = CacheMethod(env, java_util_function_Consumer, false, "accept", "(Ljava/lang/Object;)V");
  libcore_reflect_AnnotationFactory_createAnnotation = CacheMethod(env, libcore_reflect_AnnotationFactory, true, "createAnnotation", "(Ljava/lang/Class;[Llibcore/reflect/AnnotationMember;)Ljava/lang/annotation/Annotation;");
  libcore_reflect_AnnotationMember_init = CacheMethod(env, libcore_reflect_AnnotationMember, false, "<init>", "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/reflect/Method;)V");
  org_apache_harmony_dalvik_ddmc_DdmServer_broadcast = CacheMethod(env, org_apache_harmony_dalvik_ddmc_DdmServer, true, "broadcast", "(I)V");
  org_apache_harmony_dalvik_ddmc_DdmServer_dispatch = CacheMethod(env, org_apache_harmony_dalvik_ddmc_DdmServer, true, "dispatch", "(I[BII)Lorg/apache/harmony/dalvik/ddmc/Chunk;");

  dalvik_system_BaseDexClassLoader_pathList = CacheField(env, dalvik_system_BaseDexClassLoader, false, "pathList", "Ldalvik/system/DexPathList;");
  dalvik_system_DexFile_cookie = CacheField(env, dalvik_system_DexFile, false, "mCookie", "Ljava/lang/Object;");
  dalvik_system_DexFile_fileName = CacheField(env, dalvik_system_DexFile, false, "mFileName", "Ljava/lang/String;");
  dalvik_system_DexPathList_dexElements = CacheField(env, dalvik_system_DexPathList, false, "dexElements", "[Ldalvik/system/DexPathList$Element;");
  dalvik_system_DexPathList__Element_dexFile = CacheField(env, dalvik_system_DexPathList__Element, false, "dexFile", "Ldalvik/system/DexFile;");
  dalvik_system_VMRuntime_nonSdkApiUsageConsumer = CacheField(env, dalvik_system_VMRuntime, true, "nonSdkApiUsageConsumer", "Ljava/util/function/Consumer;");
  java_lang_Thread_daemon = CacheField(env, java_lang_Thread, false, "daemon", "Z");
  java_lang_Thread_group = CacheField(env, java_lang_Thread, false, "group", "Ljava/lang/ThreadGroup;");
  java_lang_Thread_lock = CacheField(env, java_lang_Thread, false, "lock", "Ljava/lang/Object;");
  java_lang_Thread_name = CacheField(env, java_lang_Thread, false, "name", "Ljava/lang/String;");
  java_lang_Thread_priority = CacheField(env, java_lang_Thread, false, "priority", "I");
  java_lang_Thread_nativePeer = CacheField(env, java_lang_Thread, false, "nativePeer", "J");
  java_lang_ThreadGroup_groups = CacheField(env, java_lang_ThreadGroup, false, "groups", "[Ljava/lang/ThreadGroup;");
  java_lang_ThreadGroup_ngroups = CacheField(env, java_lang_ThreadGroup, false, "ngroups", "I");
  java_lang_ThreadGroup_mainThreadGroup = CacheField(env, java_lang_ThreadGroup, true, "mainThreadGroup", "Ljava/lang/ThreadGroup;");
  java_lang_ThreadGroup_name = CacheField(env, java_lang_ThreadGroup, false, "name", "Ljava/lang/String;");
  java_lang_ThreadGroup_parent = CacheField(env, java_lang_ThreadGroup, false, "parent", "Ljava/lang/ThreadGroup;");
  java_lang_ThreadGroup_systemThreadGroup = CacheField(env, java_lang_ThreadGroup, true, "systemThreadGroup", "Ljava/lang/ThreadGroup;");
  java_lang_Throwable_cause = CacheField(env, java_lang_Throwable, false, "cause", "Ljava/lang/Throwable;");
  java_lang_Throwable_detailMessage = CacheField(env, java_lang_Throwable, false, "detailMessage", "Ljava/lang/String;");
  java_lang_Throwable_stackTrace = CacheField(env, java_lang_Throwable, false, "stackTrace", "[Ljava/lang/StackTraceElement;");
  java_lang_Throwable_stackState = CacheField(env, java_lang_Throwable, false, "backtrace", "Ljava/lang/Object;");
  java_lang_Throwable_suppressedExceptions = CacheField(env, java_lang_Throwable, false, "suppressedExceptions", "Ljava/util/List;");
  java_lang_reflect_Executable_artMethod = CacheField(env, java_lang_reflect_Executable, false, "artMethod", "J");
  java_nio_ByteBuffer_address = CacheField(env, java_nio_ByteBuffer, false, "address", "J");
  java_nio_ByteBuffer_hb = CacheField(env, java_nio_ByteBuffer, false, "hb", "[B");
  java_nio_ByteBuffer_isReadOnly = CacheField(env, java_nio_ByteBuffer, false, "isReadOnly", "Z");
  java_nio_ByteBuffer_limit = CacheField(env, java_nio_ByteBuffer, false, "limit", "I");
  java_nio_ByteBuffer_offset = CacheField(env, java_nio_ByteBuffer, false, "offset", "I");
  java_nio_DirectByteBuffer_capacity = CacheField(env, java_nio_DirectByteBuffer, false, "capacity", "I");
  java_nio_DirectByteBuffer_effectiveDirectAddress = CacheField(env, java_nio_DirectByteBuffer, false, "address", "J");
  java_util_ArrayList_array = CacheField(env, java_util_ArrayList, false, "elementData", "[Ljava/lang/Object;");
  java_util_ArrayList_size = CacheField(env, java_util_ArrayList, false, "size", "I");
  java_util_Collections_EMPTY_LIST = CacheField(env, java_util_Collections, true, "EMPTY_LIST", "Ljava/util/List;");
  libcore_util_EmptyArray_STACK_TRACE_ELEMENT = CacheField(env, libcore_util_EmptyArray, true, "STACK_TRACE_ELEMENT", "[Ljava/lang/StackTraceElement;");
  org_apache_harmony_dalvik_ddmc_Chunk_data = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "data", "[B");
  org_apache_harmony_dalvik_ddmc_Chunk_length = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "length", "I");
  org_apache_harmony_dalvik_ddmc_Chunk_offset = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "offset", "I");
  org_apache_harmony_dalvik_ddmc_Chunk_type = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "type", "I");

  java_lang_Boolean_valueOf = CachePrimitiveBoxingMethod(env, 'Z', "java/lang/Boolean");
  java_lang_Byte_valueOf = CachePrimitiveBoxingMethod(env, 'B', "java/lang/Byte");
  java_lang_Character_valueOf = CachePrimitiveBoxingMethod(env, 'C', "java/lang/Character");
  java_lang_Double_valueOf = CachePrimitiveBoxingMethod(env, 'D', "java/lang/Double");
  java_lang_Float_valueOf = CachePrimitiveBoxingMethod(env, 'F', "java/lang/Float");
  java_lang_Integer_valueOf = CachePrimitiveBoxingMethod(env, 'I', "java/lang/Integer");
  java_lang_Long_valueOf = CachePrimitiveBoxingMethod(env, 'J', "java/lang/Long");
  java_lang_Short_valueOf = CachePrimitiveBoxingMethod(env, 'S', "java/lang/Short");

  InitStringInit(env);
  Thread::Current()->InitStringEntryPoints();
}

void WellKnownClasses::LateInit(JNIEnv* env) {
  ScopedLocalRef<jclass> java_lang_Runtime(env, env->FindClass("java/lang/Runtime"));
  // CacheField and CacheMethod will initialize their classes. Classes below
  // have clinit sections that call JNI methods. Late init is required
  // to make sure these JNI methods are available.
  java_lang_Runtime_nativeLoad =
      CacheMethod(env, java_lang_Runtime.get(), true, "nativeLoad",
                  "(Ljava/lang/String;Ljava/lang/ClassLoader;)"
                      "Ljava/lang/String;");
  java_lang_reflect_Proxy_invoke =
    CacheMethod(env, java_lang_reflect_Proxy, true, "invoke",
                "(Ljava/lang/reflect/Proxy;Ljava/lang/reflect/Method;"
                    "[Ljava/lang/Object;)Ljava/lang/Object;");
  java_lang_reflect_Proxy_h =
    CacheField(env, java_lang_reflect_Proxy, false, "h",
               "Ljava/lang/reflect/InvocationHandler;");
}

void WellKnownClasses::Clear() {
  dalvik_annotation_optimization_CriticalNative = nullptr;
  dalvik_annotation_optimization_FastNative = nullptr;
  dalvik_system_BaseDexClassLoader = nullptr;
  dalvik_system_DelegateLastClassLoader = nullptr;
  dalvik_system_DexClassLoader = nullptr;
  dalvik_system_DexFile = nullptr;
  dalvik_system_DexPathList = nullptr;
  dalvik_system_DexPathList__Element = nullptr;
  dalvik_system_EmulatedStackFrame = nullptr;
  dalvik_system_PathClassLoader = nullptr;
  dalvik_system_VMRuntime = nullptr;
  java_lang_annotation_Annotation__array = nullptr;
  java_lang_BootClassLoader = nullptr;
  java_lang_ClassLoader = nullptr;
  java_lang_ClassNotFoundException = nullptr;
  java_lang_Daemons = nullptr;
  java_lang_Error = nullptr;
  java_lang_IllegalAccessError = nullptr;
  java_lang_invoke_MethodHandle = nullptr;
  java_lang_invoke_VarHandle = nullptr;
  java_lang_NoClassDefFoundError = nullptr;
  java_lang_Object = nullptr;
  java_lang_OutOfMemoryError = nullptr;
  java_lang_reflect_Constructor = nullptr;
  java_lang_reflect_Executable = nullptr;
  java_lang_reflect_Field = nullptr;
  java_lang_reflect_Method = nullptr;
  java_lang_reflect_Parameter = nullptr;
  java_lang_reflect_Parameter__array = nullptr;
  java_lang_reflect_Proxy = nullptr;
  java_lang_RuntimeException = nullptr;
  java_lang_StackOverflowError = nullptr;
  java_lang_String = nullptr;
  java_lang_StringFactory = nullptr;
  java_lang_System = nullptr;
  java_lang_Thread = nullptr;
  java_lang_ThreadGroup = nullptr;
  java_lang_Throwable = nullptr;
  java_util_ArrayList = nullptr;
  java_util_Collections = nullptr;
  java_nio_ByteBuffer = nullptr;
  java_nio_DirectByteBuffer = nullptr;
  libcore_reflect_AnnotationFactory = nullptr;
  libcore_reflect_AnnotationMember = nullptr;
  libcore_util_EmptyArray = nullptr;
  org_apache_harmony_dalvik_ddmc_Chunk = nullptr;
  org_apache_harmony_dalvik_ddmc_DdmServer = nullptr;

  dalvik_system_BaseDexClassLoader_getLdLibraryPath = nullptr;
  dalvik_system_VMRuntime_runFinalization = nullptr;
  java_lang_Boolean_valueOf = nullptr;
  java_lang_Byte_valueOf = nullptr;
  java_lang_Character_valueOf = nullptr;
  java_lang_ClassLoader_loadClass = nullptr;
  java_lang_ClassNotFoundException_init = nullptr;
  java_lang_Daemons_requestHeapTrim = nullptr;
  java_lang_Daemons_start = nullptr;
  java_lang_Daemons_stop = nullptr;
  java_lang_Double_valueOf = nullptr;
  java_lang_Float_valueOf = nullptr;
  java_lang_Integer_valueOf = nullptr;
  java_lang_invoke_MethodHandle_invoke = nullptr;
  java_lang_invoke_MethodHandle_invokeExact = nullptr;
  java_lang_invoke_MethodHandles_lookup = nullptr;
  java_lang_invoke_MethodHandles_Lookup_findConstructor = nullptr;
  java_lang_Long_valueOf = nullptr;
  java_lang_ref_FinalizerReference_add = nullptr;
  java_lang_ref_ReferenceQueue_add = nullptr;
  java_lang_reflect_Parameter_init = nullptr;
  java_lang_reflect_Proxy_invoke = nullptr;
  java_lang_Runtime_nativeLoad = nullptr;
  java_lang_Short_valueOf = nullptr;
  java_lang_String_charAt = nullptr;
  java_lang_System_runFinalization = nullptr;
  java_lang_Thread_dispatchUncaughtException = nullptr;
  java_lang_Thread_init = nullptr;
  java_lang_Thread_run = nullptr;
  java_lang_ThreadGroup_add = nullptr;
  java_lang_ThreadGroup_removeThread = nullptr;
  java_nio_DirectByteBuffer_init = nullptr;
  libcore_reflect_AnnotationFactory_createAnnotation = nullptr;
  libcore_reflect_AnnotationMember_init = nullptr;
  org_apache_harmony_dalvik_ddmc_DdmServer_broadcast = nullptr;
  org_apache_harmony_dalvik_ddmc_DdmServer_dispatch = nullptr;

  dalvik_system_BaseDexClassLoader_pathList = nullptr;
  dalvik_system_DexFile_cookie = nullptr;
  dalvik_system_DexFile_fileName = nullptr;
  dalvik_system_DexPathList_dexElements = nullptr;
  dalvik_system_DexPathList__Element_dexFile = nullptr;
  java_lang_reflect_Executable_artMethod = nullptr;
  java_lang_reflect_Proxy_h = nullptr;
  java_lang_Thread_daemon = nullptr;
  java_lang_Thread_group = nullptr;
  java_lang_Thread_lock = nullptr;
  java_lang_Thread_name = nullptr;
  java_lang_Thread_priority = nullptr;
  java_lang_Thread_nativePeer = nullptr;
  java_lang_ThreadGroup_groups = nullptr;
  java_lang_ThreadGroup_ngroups = nullptr;
  java_lang_ThreadGroup_mainThreadGroup = nullptr;
  java_lang_ThreadGroup_name = nullptr;
  java_lang_ThreadGroup_parent = nullptr;
  java_lang_ThreadGroup_systemThreadGroup = nullptr;
  java_lang_Throwable_cause = nullptr;
  java_lang_Throwable_detailMessage = nullptr;
  java_lang_Throwable_stackTrace = nullptr;
  java_lang_Throwable_stackState = nullptr;
  java_lang_Throwable_suppressedExceptions = nullptr;
  java_nio_ByteBuffer_address = nullptr;
  java_nio_ByteBuffer_hb = nullptr;
  java_nio_ByteBuffer_isReadOnly = nullptr;
  java_nio_ByteBuffer_limit = nullptr;
  java_nio_ByteBuffer_offset = nullptr;
  java_nio_DirectByteBuffer_capacity = nullptr;
  java_nio_DirectByteBuffer_effectiveDirectAddress = nullptr;
  java_util_ArrayList_array = nullptr;
  java_util_ArrayList_size = nullptr;
  java_util_Collections_EMPTY_LIST = nullptr;
  libcore_util_EmptyArray_STACK_TRACE_ELEMENT = nullptr;
  org_apache_harmony_dalvik_ddmc_Chunk_data = nullptr;
  org_apache_harmony_dalvik_ddmc_Chunk_length = nullptr;
  org_apache_harmony_dalvik_ddmc_Chunk_offset = nullptr;
  org_apache_harmony_dalvik_ddmc_Chunk_type = nullptr;
}

ObjPtr<mirror::Class> WellKnownClasses::ToClass(jclass global_jclass) {
  auto ret = ObjPtr<mirror::Class>::DownCast(Thread::Current()->DecodeJObject(global_jclass));
  DCHECK(!ret.IsNull());
  return ret;
}

}  // namespace art
