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

#include <inttypes.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_utf.h"

namespace art {
namespace Test913Heaps {

using android::base::StringPrintf;

#define FINAL final
#define OVERRIDE override
#define UNREACHABLE  __builtin_unreachable

extern "C" JNIEXPORT void JNICALL Java_art_Test913_forceGarbageCollection(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiError ret = jvmti_env->ForceGarbageCollection();
  JvmtiErrorToException(env, jvmti_env, ret);
}

// Collect sizes of objects (classes) ahead of time, to be able to normalize.
struct ClassData {
  jlong size;    // Size as reported by GetObjectSize.
  jlong serial;  // Computed serial that should be printed instead of the size.
};

// Stores a map from tags to ClassData.
static std::map<jlong, ClassData> sClassData;
static size_t sClassDataSerial = 0;
// Large enough number that a collision with a test object is unlikely.
static constexpr jlong kClassDataSerialBase = 123456780000;

// Register a class (or general object) in the class-data map. The serial number is determined by
// the order of calls to this function (so stable Java code leads to stable numbering).
extern "C" JNIEXPORT void JNICALL Java_art_Test913_registerClass(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag, jobject obj) {
  ClassData data;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetObjectSize(obj, &data.size))) {
    return;
  }
  data.serial = kClassDataSerialBase + sClassDataSerial++;
  // Remove old element, if it exists.
  auto old = sClassData.find(tag);
  if (old != sClassData.end()) {
    sClassData.erase(old);
  }
  // Now insert the new mapping.
  sClassData.insert(std::pair<jlong, ClassData>(tag, data));
}

class IterationConfig {
 public:
  IterationConfig() {}
  virtual ~IterationConfig() {}

  virtual jint Handle(jvmtiHeapReferenceKind reference_kind,
                      const jvmtiHeapReferenceInfo* reference_info,
                      jlong class_tag,
                      jlong referrer_class_tag,
                      jlong size,
                      jlong* tag_ptr,
                      jlong* referrer_tag_ptr,
                      jint length,
                      void* user_data) = 0;
};

static jint JNICALL HeapReferenceCallback(jvmtiHeapReferenceKind reference_kind,
                                          const jvmtiHeapReferenceInfo* reference_info,
                                          jlong class_tag,
                                          jlong referrer_class_tag,
                                          jlong size,
                                          jlong* tag_ptr,
                                          jlong* referrer_tag_ptr,
                                          jint length,
                                          void* user_data) {
  IterationConfig* config = reinterpret_cast<IterationConfig*>(user_data);
  return config->Handle(reference_kind,
                        reference_info,
                        class_tag,
                        referrer_class_tag,
                        size,
                        tag_ptr,
                        referrer_tag_ptr,
                        length,
                        user_data);
}

static bool Run(JNIEnv* env,
                jint heap_filter,
                jclass klass_filter,
                jobject initial_object,
                IterationConfig* config) {
  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_reference_callback = HeapReferenceCallback;

  jvmtiError ret = jvmti_env->FollowReferences(heap_filter,
                                               klass_filter,
                                               initial_object,
                                               &callbacks,
                                               config);
  return !JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test913_followReferences(
    JNIEnv* env,
    jclass klass ATTRIBUTE_UNUSED,
    jint heap_filter,
    jclass klass_filter,
    jobject initial_object,
    jint stop_after,
    jint follow_set,
    jobject jniRef) {
  class PrintIterationConfig FINAL : public IterationConfig {
   public:
    PrintIterationConfig(jint _stop_after, jint _follow_set)
        : counter_(0),
          stop_after_(_stop_after),
          follow_set_(_follow_set) {
    }

    jint Handle(jvmtiHeapReferenceKind reference_kind,
                const jvmtiHeapReferenceInfo* reference_info,
                jlong class_tag,
                jlong referrer_class_tag,
                jlong size,
                jlong* tag_ptr,
                jlong* referrer_tag_ptr,
                jint length,
                void* user_data ATTRIBUTE_UNUSED) OVERRIDE {
      jlong tag = *tag_ptr;

      // Ignore any jni-global roots with untagged classes. These can be from the environment,
      // or the JIT.
      if (reference_kind == JVMTI_HEAP_REFERENCE_JNI_GLOBAL && class_tag == 0) {
        return 0;
      }
      // Ignore classes (1000 <= tag < 3000) for thread objects. These can be held by the JIT.
      if (reference_kind == JVMTI_HEAP_REFERENCE_THREAD && class_tag == 0 &&
              (1000 <= *tag_ptr &&  *tag_ptr < 3000)) {
        return 0;
      }
      // Ignore stack-locals of untagged threads. That is the environment.
      if (reference_kind == JVMTI_HEAP_REFERENCE_STACK_LOCAL &&
          reference_info->stack_local.thread_tag != 3000) {
        return 0;
      }
      // Ignore array elements with an untagged source. These are from the environment.
      if (reference_kind == JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT && *referrer_tag_ptr == 0) {
        return 0;
      }

      // Only check tagged objects.
      if (tag == 0) {
        return JVMTI_VISIT_OBJECTS;
      }

      Print(reference_kind,
            reference_info,
            class_tag,
            referrer_class_tag,
            size,
            tag_ptr,
            referrer_tag_ptr,
            length);

      counter_++;
      if (counter_ == stop_after_) {
        return JVMTI_VISIT_ABORT;
      }

      if (tag > 0 && tag < 32) {
        bool should_visit_references = (follow_set_ & (1 << static_cast<int32_t>(tag))) != 0;
        return should_visit_references ? JVMTI_VISIT_OBJECTS : 0;
      }

      return JVMTI_VISIT_OBJECTS;
    }

    void Print(jvmtiHeapReferenceKind reference_kind,
               const jvmtiHeapReferenceInfo* reference_info,
               jlong class_tag,
               jlong referrer_class_tag,
               jlong size,
               jlong* tag_ptr,
               jlong* referrer_tag_ptr,
               jint length) {
      std::string referrer_str;
      if (referrer_tag_ptr == nullptr) {
        referrer_str = "root@root";
      } else {
        referrer_str = StringPrintf("%" PRId64 "@%" PRId64, *referrer_tag_ptr, referrer_class_tag);
      }

      jlong adapted_size = size;
      if (*tag_ptr != 0) {
        // This is a class or interface, the size of which will be dependent on the architecture.
        // Do not print the size, but detect known values and "normalize" for the golden file.
        auto it = sClassData.find(*tag_ptr);
        if (it != sClassData.end()) {
          const ClassData& class_data = it->second;
          if (class_data.size == size) {
            adapted_size = class_data.serial;
          } else {
            adapted_size = 0xDEADDEAD;
          }
        }
      }

      std::string referree_str = StringPrintf("%" PRId64 "@%" PRId64, *tag_ptr, class_tag);

      lines_.push_back(CreateElem(referrer_str,
                                  referree_str,
                                  reference_kind,
                                  reference_info,
                                  adapted_size,
                                  length));
    }

    std::vector<std::string> GetLines() const {
      std::vector<std::string> ret;
      for (const std::unique_ptr<Elem>& e : lines_) {
        ret.push_back(e->Print());
      }
      return ret;
    }

   private:
    // We need to postpone some printing, as required functions are not callback-safe.
    class Elem {
     public:
      Elem(const std::string& referrer, const std::string& referree, jlong size, jint length)
          : referrer_(referrer), referree_(referree), size_(size), length_(length) {}
      virtual ~Elem() {}

      std::string Print() const {
        return StringPrintf("%s --(%s)--> %s [size=%" PRId64 ", length=%d]",
                            referrer_.c_str(),
                            PrintArrowType().c_str(),
                            referree_.c_str(),
                            size_,
                            length_);
      }

     protected:
      virtual std::string PrintArrowType() const = 0;

     private:
      std::string referrer_;
      std::string referree_;
      jlong size_;
      jint length_;
    };

    class JNILocalElement : public Elem {
     public:
      JNILocalElement(const std::string& referrer,
                      const std::string& referree,
                      jlong size,
                      jint length,
                      const jvmtiHeapReferenceInfo* reference_info)
          : Elem(referrer, referree, size, length) {
        memcpy(&info_, reference_info, sizeof(jvmtiHeapReferenceInfo));
      }

     protected:
      std::string PrintArrowType() const OVERRIDE {
        char* name = nullptr;
        if (info_.jni_local.method != nullptr) {
          jvmti_env->GetMethodName(info_.jni_local.method, &name, nullptr, nullptr);
        }
        // Normalize the thread id, as this depends on the number of other threads
        // and which thread is running the test. Should be:
        //   jlong thread_id = info_.jni_local.thread_id;
        // TODO: A pre-pass before the test should be able fetch this number, so it can
        //       be compared explicitly.
        jlong thread_id = 1;
        std::string ret = StringPrintf("jni-local[id=%" PRId64 ",tag=%" PRId64 ",depth=%d,"
                                       "method=%s]",
                                       thread_id,
                                       info_.jni_local.thread_tag,
                                       info_.jni_local.depth,
                                       name == nullptr ? "<null>" : name);
        if (name != nullptr) {
          jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(name));
        }

        return ret;
      }

     private:
      const std::string string_;
      jvmtiHeapReferenceInfo info_;
    };

    class StackLocalElement : public Elem {
     public:
      StackLocalElement(const std::string& referrer,
                        const std::string& referree,
                        jlong size,
                        jint length,
                        const jvmtiHeapReferenceInfo* reference_info)
          : Elem(referrer, referree, size, length) {
        memcpy(&info_, reference_info, sizeof(jvmtiHeapReferenceInfo));

        // Debug code. Try to figure out where bad depth is coming from.
        if (reference_info->stack_local.depth == 6) {
          LOG(FATAL) << "Unexpected depth of 6";
        }
      }

     protected:
      std::string PrintArrowType() const OVERRIDE {
        char* name = nullptr;
        if (info_.stack_local.method != nullptr) {
          jvmti_env->GetMethodName(info_.stack_local.method, &name, nullptr, nullptr);
        }
        // Normalize the thread id, as this depends on the number of other threads
        // and which thread is running the test. Should be:
        //   jlong thread_id = info_.stack_local.thread_id;
        // TODO: A pre-pass before the test should be able fetch this number, so it can
        //       be compared explicitly.
        jlong thread_id = 1;
        std::string ret = StringPrintf("stack-local[id=%" PRId64 ",tag=%" PRId64 ",depth=%d,"
                                       "method=%s,vreg=%d,location=% " PRId64 "]",
                                       thread_id,
                                       info_.stack_local.thread_tag,
                                       info_.stack_local.depth,
                                       name == nullptr ? "<null>" : name,
                                       info_.stack_local.slot,
                                       info_.stack_local.location);
        if (name != nullptr) {
          jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(name));
        }

        return ret;
      }

     private:
      const std::string string_;
      jvmtiHeapReferenceInfo info_;
    };

    // For simple or unimplemented cases.
    class StringElement : public Elem {
     public:
      StringElement(const std::string& referrer,
                   const std::string& referree,
                   jlong size,
                   jint length,
                   const std::string& string)
          : Elem(referrer, referree, size, length), string_(string) {}

     protected:
      std::string PrintArrowType() const OVERRIDE {
        return string_;
      }

     private:
      const std::string string_;
    };

    static std::unique_ptr<Elem> CreateElem(const std::string& referrer,
                                            const std::string& referree,
                                            jvmtiHeapReferenceKind reference_kind,
                                            const jvmtiHeapReferenceInfo* reference_info,
                                            jlong size,
                                            jint length) {
      switch (reference_kind) {
        case JVMTI_HEAP_REFERENCE_CLASS:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "class"));
        case JVMTI_HEAP_REFERENCE_FIELD: {
          std::string tmp = StringPrintf("field@%d", reference_info->field.index);
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                        referree,
                                                        size,
                                                        length,
                                                        tmp));
        }
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT: {
          jint index = reference_info->array.index;
          // Normalize if it's "0@0" -> "3000@1".
          // TODO: A pre-pass could probably give us this index to check explicitly.
          if (referrer == "0@0" && referree == "3000@0") {
            index = 0;
          }
          std::string tmp = StringPrintf("array-element@%d", index);
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         tmp));
        }
        case JVMTI_HEAP_REFERENCE_CLASS_LOADER:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "classloader"));
        case JVMTI_HEAP_REFERENCE_SIGNERS:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "signers"));
        case JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "protection-domain"));
        case JVMTI_HEAP_REFERENCE_INTERFACE:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "interface"));
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD: {
          std::string tmp = StringPrintf("array-element@%d", reference_info->array.index);
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         tmp));;
        }
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "constant-pool"));
        case JVMTI_HEAP_REFERENCE_SUPERCLASS:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "superclass"));
        case JVMTI_HEAP_REFERENCE_JNI_GLOBAL:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "jni-global"));
        case JVMTI_HEAP_REFERENCE_SYSTEM_CLASS:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "system-class"));
        case JVMTI_HEAP_REFERENCE_MONITOR:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "monitor"));
        case JVMTI_HEAP_REFERENCE_STACK_LOCAL:
          return std::unique_ptr<Elem>(new StackLocalElement(referrer,
                                                             referree,
                                                             size,
                                                             length,
                                                             reference_info));
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL:
          return std::unique_ptr<Elem>(new JNILocalElement(referrer,
                                                           referree,
                                                           size,
                                                           length,
                                                           reference_info));
        case JVMTI_HEAP_REFERENCE_THREAD:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "thread"));
        case JVMTI_HEAP_REFERENCE_OTHER:
          return std::unique_ptr<Elem>(new StringElement(referrer,
                                                         referree,
                                                         size,
                                                         length,
                                                         "other"));
      }
      LOG(FATAL) << "Unknown kind";
      UNREACHABLE();
    }

    jint counter_;
    const jint stop_after_;
    const jint follow_set_;

    std::vector<std::unique_ptr<Elem>> lines_;
  };

  // If jniRef isn't null, add a local and a global ref.
  ScopedLocalRef<jobject> jni_local_ref(env, nullptr);
  jobject jni_global_ref = nullptr;
  if (jniRef != nullptr) {
    jni_local_ref.reset(env->NewLocalRef(jniRef));
    jni_global_ref = env->NewGlobalRef(jniRef);
  }

  PrintIterationConfig config(stop_after, follow_set);
  if (!Run(env, heap_filter, klass_filter, initial_object, &config)) {
    return nullptr;
  }

  std::vector<std::string> lines = config.GetLines();
  jobjectArray ret = CreateObjectArray(env,
                                       static_cast<jint>(lines.size()),
                                       "java/lang/String",
                                       [&](jint i) {
                                         return env->NewStringUTF(lines[i].c_str());
                                       });

  if (jni_global_ref != nullptr) {
    env->DeleteGlobalRef(jni_global_ref);
  }

  return ret;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test913_followReferencesString(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject initial_object) {
  struct FindStringCallbacks {
    static jint JNICALL FollowReferencesCallback(
        jvmtiHeapReferenceKind reference_kind ATTRIBUTE_UNUSED,
        const jvmtiHeapReferenceInfo* reference_info ATTRIBUTE_UNUSED,
        jlong class_tag ATTRIBUTE_UNUSED,
        jlong referrer_class_tag ATTRIBUTE_UNUSED,
        jlong size ATTRIBUTE_UNUSED,
        jlong* tag_ptr ATTRIBUTE_UNUSED,
        jlong* referrer_tag_ptr ATTRIBUTE_UNUSED,
        jint length ATTRIBUTE_UNUSED,
        void* user_data ATTRIBUTE_UNUSED) {
      return JVMTI_VISIT_OBJECTS;  // Continue visiting.
    }

    static jint JNICALL StringValueCallback(jlong class_tag,
                                            jlong size,
                                            jlong* tag_ptr,
                                            const jchar* value,
                                            jint value_length,
                                            void* user_data) {
      FindStringCallbacks* p = reinterpret_cast<FindStringCallbacks*>(user_data);
      if (*tag_ptr != 0) {
        size_t utf_byte_count = ti::CountUtf8Bytes(value, value_length);
        std::unique_ptr<char[]> mod_utf(new char[utf_byte_count + 1]);
        memset(mod_utf.get(), 0, utf_byte_count + 1);
        ti::ConvertUtf16ToModifiedUtf8(mod_utf.get(), utf_byte_count, value, value_length);
        p->data.push_back(android::base::StringPrintf("%" PRId64 "@%" PRId64 " (%" PRId64 ", '%s')",
                                                      *tag_ptr,
                                                      class_tag,
                                                      size,
                                                      mod_utf.get()));
        // Update the tag to test whether that works.
        *tag_ptr = *tag_ptr + 1;
      }
      return 0;
    }

    std::vector<std::string> data;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_reference_callback = FindStringCallbacks::FollowReferencesCallback;
  callbacks.string_primitive_value_callback = FindStringCallbacks::StringValueCallback;

  FindStringCallbacks fsc;
  jvmtiError ret = jvmti_env->FollowReferences(0, nullptr, initial_object, &callbacks, &fsc);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }

  jobjectArray retArray = CreateObjectArray(env,
                                            static_cast<jint>(fsc.data.size()),
                                            "java/lang/String",
                                            [&](jint i) {
                                              return env->NewStringUTF(fsc.data[i].c_str());
                                            });
  return retArray;
}


extern "C" JNIEXPORT jstring JNICALL Java_art_Test913_followReferencesPrimitiveArray(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject initial_object) {
  struct FindArrayCallbacks {
    static jint JNICALL FollowReferencesCallback(
        jvmtiHeapReferenceKind reference_kind ATTRIBUTE_UNUSED,
        const jvmtiHeapReferenceInfo* reference_info ATTRIBUTE_UNUSED,
        jlong class_tag ATTRIBUTE_UNUSED,
        jlong referrer_class_tag ATTRIBUTE_UNUSED,
        jlong size ATTRIBUTE_UNUSED,
        jlong* tag_ptr ATTRIBUTE_UNUSED,
        jlong* referrer_tag_ptr ATTRIBUTE_UNUSED,
        jint length ATTRIBUTE_UNUSED,
        void* user_data ATTRIBUTE_UNUSED) {
      return JVMTI_VISIT_OBJECTS;  // Continue visiting.
    }

    static jint JNICALL ArrayValueCallback(jlong class_tag,
                                           jlong size,
                                           jlong* tag_ptr,
                                           jint element_count,
                                           jvmtiPrimitiveType element_type,
                                           const void* elements,
                                           void* user_data) {
      FindArrayCallbacks* p = reinterpret_cast<FindArrayCallbacks*>(user_data);
      if (*tag_ptr != 0) {
        std::ostringstream oss;
        oss << *tag_ptr
            << '@'
            << class_tag
            << " ("
            << size
            << ", "
            << element_count
            << "x"
            << static_cast<char>(element_type)
            << " '";
        size_t element_size;
        switch (element_type) {
          case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
          case JVMTI_PRIMITIVE_TYPE_BYTE:
            element_size = 1;
            break;
          case JVMTI_PRIMITIVE_TYPE_CHAR:
          case JVMTI_PRIMITIVE_TYPE_SHORT:
            element_size = 2;
            break;
          case JVMTI_PRIMITIVE_TYPE_INT:
          case JVMTI_PRIMITIVE_TYPE_FLOAT:
            element_size = 4;
            break;
          case JVMTI_PRIMITIVE_TYPE_LONG:
          case JVMTI_PRIMITIVE_TYPE_DOUBLE:
            element_size = 8;
            break;
          default:
            LOG(FATAL) << "Unknown type " << static_cast<size_t>(element_type);
            UNREACHABLE();
        }
        const uint8_t* data = reinterpret_cast<const uint8_t*>(elements);
        for (size_t i = 0; i != element_size * element_count; ++i) {
          oss << android::base::StringPrintf("%02x", data[i]);
        }
        oss << "')";

        if (!p->data.empty()) {
          p->data += "\n";
        }
        p->data += oss.str();
        // Update the tag to test whether that works.
        *tag_ptr = *tag_ptr + 1;
      }
      return 0;
    }

    std::string data;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_reference_callback = FindArrayCallbacks::FollowReferencesCallback;
  callbacks.array_primitive_value_callback = FindArrayCallbacks::ArrayValueCallback;

  FindArrayCallbacks fac;
  jvmtiError ret = jvmti_env->FollowReferences(0, nullptr, initial_object, &callbacks, &fac);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }
  return env->NewStringUTF(fac.data.c_str());
}

static constexpr const char* GetPrimitiveTypeName(jvmtiPrimitiveType type) {
  switch (type) {
    case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
      return "boolean";
    case JVMTI_PRIMITIVE_TYPE_BYTE:
      return "byte";
    case JVMTI_PRIMITIVE_TYPE_CHAR:
      return "char";
    case JVMTI_PRIMITIVE_TYPE_SHORT:
      return "short";
    case JVMTI_PRIMITIVE_TYPE_INT:
      return "int";
    case JVMTI_PRIMITIVE_TYPE_FLOAT:
      return "float";
    case JVMTI_PRIMITIVE_TYPE_LONG:
      return "long";
    case JVMTI_PRIMITIVE_TYPE_DOUBLE:
      return "double";
  }
  LOG(FATAL) << "Unknown type " << static_cast<size_t>(type);
  UNREACHABLE();
}

extern "C" JNIEXPORT jstring JNICALL Java_art_Test913_followReferencesPrimitiveFields(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject initial_object) {
  struct FindFieldCallbacks {
    static jint JNICALL FollowReferencesCallback(
        jvmtiHeapReferenceKind reference_kind ATTRIBUTE_UNUSED,
        const jvmtiHeapReferenceInfo* reference_info ATTRIBUTE_UNUSED,
        jlong class_tag ATTRIBUTE_UNUSED,
        jlong referrer_class_tag ATTRIBUTE_UNUSED,
        jlong size ATTRIBUTE_UNUSED,
        jlong* tag_ptr ATTRIBUTE_UNUSED,
        jlong* referrer_tag_ptr ATTRIBUTE_UNUSED,
        jint length ATTRIBUTE_UNUSED,
        void* user_data ATTRIBUTE_UNUSED) {
      return JVMTI_VISIT_OBJECTS;  // Continue visiting.
    }

    static jint JNICALL PrimitiveFieldValueCallback(jvmtiHeapReferenceKind kind,
                                                    const jvmtiHeapReferenceInfo* info,
                                                    jlong class_tag,
                                                    jlong* tag_ptr,
                                                    jvalue value,
                                                    jvmtiPrimitiveType value_type,
                                                    void* user_data) {
      FindFieldCallbacks* p = reinterpret_cast<FindFieldCallbacks*>(user_data);
      if (*tag_ptr != 0) {
        std::ostringstream oss;
        oss << *tag_ptr
            << '@'
            << class_tag
            << " ("
            << (kind == JVMTI_HEAP_REFERENCE_FIELD ? "instance, " : "static, ")
            << GetPrimitiveTypeName(value_type)
            << ", index="
            << info->field.index
            << ") ";
        // Be lazy, always print eight bytes.
        static_assert(sizeof(jvalue) == sizeof(uint64_t), "Unexpected jvalue size");
        uint64_t val;
        memcpy(&val, &value, sizeof(uint64_t));  // To avoid undefined behavior.
        oss << android::base::StringPrintf("%016" PRIx64, val);

        if (!p->data.empty()) {
          p->data += "\n";
        }
        p->data += oss.str();
        // Update the tag to test whether that works.
        *tag_ptr = *tag_ptr + 1;
      }
      return 0;
    }

    std::string data;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_reference_callback = FindFieldCallbacks::FollowReferencesCallback;
  callbacks.primitive_field_callback = FindFieldCallbacks::PrimitiveFieldValueCallback;

  FindFieldCallbacks ffc;
  jvmtiError ret = jvmti_env->FollowReferences(0, nullptr, initial_object, &callbacks, &ffc);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }
  return env->NewStringUTF(ffc.data.c_str());
}

// This is copied from test 908. Consider moving this to the main shim.

static size_t starts = 0;
static size_t finishes = 0;

static void JNICALL GarbageCollectionFinish(jvmtiEnv* ti_env ATTRIBUTE_UNUSED) {
  finishes++;
}

static void JNICALL GarbageCollectionStart(jvmtiEnv* ti_env ATTRIBUTE_UNUSED) {
  starts++;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test913_setupGcCallback(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.GarbageCollectionFinish = GarbageCollectionFinish;
  callbacks.GarbageCollectionStart = GarbageCollectionStart;

  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test913_enableGcTracking(JNIEnv* env,
                                                                    jclass klass ATTRIBUTE_UNUSED,
                                                                    jboolean enable) {
  jvmtiError ret = jvmti_env->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE,
      JVMTI_EVENT_GARBAGE_COLLECTION_START,
      nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
  ret = jvmti_env->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE,
      JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
      nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test913_getGcStarts(JNIEnv* env ATTRIBUTE_UNUSED,
                                                               jclass klass ATTRIBUTE_UNUSED) {
  jint result = static_cast<jint>(starts);
  starts = 0;
  return result;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test913_getGcFinishes(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                 jclass klass ATTRIBUTE_UNUSED) {
  jint result = static_cast<jint>(finishes);
  finishes = 0;
  return result;
}

using GetObjectHeapId = jvmtiError(*)(jvmtiEnv*, jlong, jint*, ...);
static GetObjectHeapId gGetObjectHeapIdFn = nullptr;

using GetHeapName = jvmtiError(*)(jvmtiEnv*, jint, char**, ...);
static GetHeapName gGetHeapNameFn = nullptr;

using IterateThroughHeapExt = jvmtiError(*)(jvmtiEnv*,
                                            jint,
                                            jclass,
                                            const jvmtiHeapCallbacks*,
                                            const void*);
static IterateThroughHeapExt gIterateThroughHeapExt = nullptr;


static void FreeExtensionFunctionInfo(jvmtiExtensionFunctionInfo* extensions, jint count) {
  for (size_t i = 0; i != static_cast<size_t>(count); ++i) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(extensions[i].id));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(extensions[i].short_description));
    for (size_t j = 0; j != static_cast<size_t>(extensions[i].param_count); ++j) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(extensions[i].params[j].name));
    }
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(extensions[i].params));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(extensions[i].errors));
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test913_checkForExtensionApis(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jint extension_count;
  jvmtiExtensionFunctionInfo* extensions;
  jvmtiError result = jvmti_env->GetExtensionFunctions(&extension_count, &extensions);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return;
  }

  for (size_t i = 0; i != static_cast<size_t>(extension_count); ++i) {
    if (strcmp("com.android.art.heap.get_object_heap_id", extensions[i].id) == 0) {
      CHECK(gGetObjectHeapIdFn == nullptr);
      gGetObjectHeapIdFn = reinterpret_cast<GetObjectHeapId>(extensions[i].func);

      CHECK_EQ(extensions[i].param_count, 2);

      CHECK_EQ(strcmp("tag", extensions[i].params[0].name), 0);
      CHECK_EQ(extensions[i].params[0].base_type, JVMTI_TYPE_JLONG);
      CHECK_EQ(extensions[i].params[0].kind, JVMTI_KIND_IN);

      CHECK_EQ(strcmp("heap_id", extensions[i].params[1].name), 0);
      CHECK_EQ(extensions[i].params[1].base_type, JVMTI_TYPE_JINT);
      CHECK_EQ(extensions[i].params[1].kind, JVMTI_KIND_OUT);
      CHECK_EQ(extensions[i].params[1].null_ok, false);

      CHECK_EQ(extensions[i].error_count, 1);
      CHECK(extensions[i].errors != nullptr);
      CHECK(extensions[i].errors[0] == JVMTI_ERROR_NOT_FOUND);

      continue;
    }

    if (strcmp("com.android.art.heap.get_heap_name", extensions[i].id) == 0) {
      CHECK(gGetHeapNameFn == nullptr);
      gGetHeapNameFn = reinterpret_cast<GetHeapName>(extensions[i].func);

      CHECK_EQ(extensions[i].param_count, 2);

      CHECK_EQ(strcmp("heap_id", extensions[i].params[0].name), 0);
      CHECK_EQ(extensions[i].params[0].base_type, JVMTI_TYPE_JINT);
      CHECK_EQ(extensions[i].params[0].kind, JVMTI_KIND_IN);

      CHECK_EQ(strcmp("heap_name", extensions[i].params[1].name), 0);
      CHECK_EQ(extensions[i].params[1].base_type, JVMTI_TYPE_CCHAR);
      CHECK_EQ(extensions[i].params[1].kind, JVMTI_KIND_ALLOC_BUF);
      CHECK_EQ(extensions[i].params[1].null_ok, false);

      CHECK_EQ(extensions[i].error_count, 1);
      CHECK(extensions[i].errors != nullptr);
      CHECK(extensions[i].errors[0] == JVMTI_ERROR_ILLEGAL_ARGUMENT);
    }

    if (strcmp("com.android.art.heap.iterate_through_heap_ext", extensions[i].id) == 0) {
      CHECK(gIterateThroughHeapExt == nullptr);
      gIterateThroughHeapExt = reinterpret_cast<IterateThroughHeapExt>(extensions[i].func);

      CHECK_EQ(extensions[i].param_count, 4);

      CHECK_EQ(strcmp("heap_filter", extensions[i].params[0].name), 0);
      CHECK_EQ(extensions[i].params[0].base_type, JVMTI_TYPE_JINT);
      CHECK_EQ(extensions[i].params[0].kind, JVMTI_KIND_IN);

      CHECK_EQ(strcmp("klass", extensions[i].params[1].name), 0);
      CHECK_EQ(extensions[i].params[1].base_type, JVMTI_TYPE_JCLASS);
      CHECK_EQ(extensions[i].params[1].kind, JVMTI_KIND_IN);
      CHECK_EQ(extensions[i].params[1].null_ok, true);

      CHECK_EQ(strcmp("callbacks", extensions[i].params[2].name), 0);
      CHECK_EQ(extensions[i].params[2].base_type, JVMTI_TYPE_CVOID);
      CHECK_EQ(extensions[i].params[2].kind, JVMTI_KIND_IN_PTR);
      CHECK_EQ(extensions[i].params[2].null_ok, false);

      CHECK_EQ(strcmp("user_data", extensions[i].params[3].name), 0);
      CHECK_EQ(extensions[i].params[3].base_type, JVMTI_TYPE_CVOID);
      CHECK_EQ(extensions[i].params[3].kind, JVMTI_KIND_IN_PTR);
      CHECK_EQ(extensions[i].params[3].null_ok, true);

      CHECK_EQ(extensions[i].error_count, 3);
      CHECK(extensions[i].errors != nullptr);
      CHECK(extensions[i].errors[0] == JVMTI_ERROR_MUST_POSSESS_CAPABILITY);
      CHECK(extensions[i].errors[1] == JVMTI_ERROR_INVALID_CLASS);
      CHECK(extensions[i].errors[2] == JVMTI_ERROR_NULL_POINTER);
    }
  }

  CHECK(gGetObjectHeapIdFn != nullptr);
  CHECK(gGetHeapNameFn != nullptr);

  FreeExtensionFunctionInfo(extensions, extension_count);
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test913_getObjectHeapId(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag) {
  CHECK(gGetObjectHeapIdFn != nullptr);
  jint heap_id;
  jvmtiError result = gGetObjectHeapIdFn(jvmti_env, tag, &heap_id);
  JvmtiErrorToException(env, jvmti_env, result);
  return heap_id;
}

extern "C" JNIEXPORT jstring JNICALL Java_art_Test913_getHeapName(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jint heap_id) {
  CHECK(gGetHeapNameFn != nullptr);
  char* heap_name;
  jvmtiError result = gGetHeapNameFn(jvmti_env, heap_id, &heap_name);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }
  jstring ret = env->NewStringUTF(heap_name);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(heap_name));
  return ret;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test913_checkGetObjectHeapIdInCallback(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag, jint heap_id) {
  CHECK(gGetObjectHeapIdFn != nullptr);

  {
    struct GetObjectHeapIdCallbacks {
      static jint JNICALL FollowReferencesCallback(
          jvmtiHeapReferenceKind reference_kind ATTRIBUTE_UNUSED,
          const jvmtiHeapReferenceInfo* reference_info ATTRIBUTE_UNUSED,
          jlong class_tag ATTRIBUTE_UNUSED,
          jlong referrer_class_tag ATTRIBUTE_UNUSED,
          jlong size ATTRIBUTE_UNUSED,
          jlong* tag_ptr,
          jlong* referrer_tag_ptr ATTRIBUTE_UNUSED,
          jint length ATTRIBUTE_UNUSED,
          void* user_data) {
        if (*tag_ptr != 0) {
          GetObjectHeapIdCallbacks* p = reinterpret_cast<GetObjectHeapIdCallbacks*>(user_data);
          if (*tag_ptr == p->check_callback_tag) {
            jint tag_heap_id;
            jvmtiError result = gGetObjectHeapIdFn(jvmti_env, *tag_ptr, &tag_heap_id);
            CHECK_EQ(result, JVMTI_ERROR_NONE);
            CHECK_EQ(tag_heap_id, p->check_callback_id);
            return JVMTI_VISIT_ABORT;
          }
        }

        return JVMTI_VISIT_OBJECTS;  // Continue visiting.
      }

      jlong check_callback_tag;
      jint check_callback_id;
    };

    jvmtiHeapCallbacks callbacks;
    memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
    callbacks.heap_reference_callback = GetObjectHeapIdCallbacks::FollowReferencesCallback;

    GetObjectHeapIdCallbacks ffc;
    ffc.check_callback_tag = tag;
    ffc.check_callback_id = heap_id;

    jvmtiError ret = jvmti_env->FollowReferences(0, nullptr, nullptr, &callbacks, &ffc);
    if (JvmtiErrorToException(env, jvmti_env, ret)) {
      return;
    }
  }

  {
    struct GetObjectHeapIdCallbacks {
      static jint JNICALL HeapIterationCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                                jlong size ATTRIBUTE_UNUSED,
                                                jlong* tag_ptr,
                                                jint length ATTRIBUTE_UNUSED,
                                                void* user_data) {
        if (*tag_ptr != 0) {
          GetObjectHeapIdCallbacks* p = reinterpret_cast<GetObjectHeapIdCallbacks*>(user_data);
          if (*tag_ptr == p->check_callback_tag) {
            jint tag_heap_id;
            jvmtiError result = gGetObjectHeapIdFn(jvmti_env, *tag_ptr, &tag_heap_id);
            CHECK_EQ(result, JVMTI_ERROR_NONE);
            CHECK_EQ(tag_heap_id, p->check_callback_id);
            return JVMTI_VISIT_ABORT;
          }
        }

        return 0;  // Continue visiting.
      }

      jlong check_callback_tag;
      jint check_callback_id;
    };

    jvmtiHeapCallbacks callbacks;
    memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
    callbacks.heap_iteration_callback = GetObjectHeapIdCallbacks::HeapIterationCallback;

    GetObjectHeapIdCallbacks ffc;
    ffc.check_callback_tag = tag;
    ffc.check_callback_id = heap_id;

    jvmtiError ret = jvmti_env->IterateThroughHeap(0, nullptr, &callbacks, &ffc);
    if (JvmtiErrorToException(env, jvmti_env, ret)) {
      return;
    }
  }
}

static bool gFoundExt = false;

static jint JNICALL HeapIterationExtCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                             jlong size ATTRIBUTE_UNUSED,
                                             jlong* tag_ptr,
                                             jint length ATTRIBUTE_UNUSED,
                                             void* user_data ATTRIBUTE_UNUSED,
                                             jint heap_id) {
  // We expect some tagged objects at or above the threshold, where the expected heap id is
  // encoded into lowest byte.
  constexpr jlong kThreshold = 30000000;
  jlong tag = *tag_ptr;
  if (tag >= kThreshold) {
    jint expected_heap_id = static_cast<jint>(tag - kThreshold);
    CHECK_EQ(expected_heap_id, heap_id);
    gFoundExt = true;
  }
  return 0;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test913_iterateThroughHeapExt(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  CHECK(gIterateThroughHeapExt != nullptr);

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_iteration_callback =
      reinterpret_cast<decltype(callbacks.heap_iteration_callback)>(HeapIterationExtCallback);

  jvmtiError ret = gIterateThroughHeapExt(jvmti_env, 0, nullptr, &callbacks, nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
  CHECK(gFoundExt);
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test913_checkInitialized(JNIEnv* env, jclass, jclass c) {
  jint status;
  jvmtiError error = jvmti_env->GetClassStatus(c, &status);
  if (JvmtiErrorToException(env, jvmti_env, error)) {
    return false;
  }
  return (status & JVMTI_CLASS_STATUS_INITIALIZED) != 0;
}

}  // namespace Test913Heaps
}  // namespace art
