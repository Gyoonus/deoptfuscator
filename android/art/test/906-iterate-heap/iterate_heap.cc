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
#include <pthread.h>

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_primitive_array.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"
#include "ti_utf.h"

namespace art {
namespace Test906IterateHeap {

class IterationConfig {
 public:
  IterationConfig() {}
  virtual ~IterationConfig() {}

  virtual jint Handle(jlong class_tag, jlong size, jlong* tag_ptr, jint length) = 0;
};

static jint JNICALL HeapIterationCallback(jlong class_tag,
                                          jlong size,
                                          jlong* tag_ptr,
                                          jint length,
                                          void* user_data) {
  IterationConfig* config = reinterpret_cast<IterationConfig*>(user_data);
  return config->Handle(class_tag, size, tag_ptr, length);
}

static bool Run(JNIEnv* env, jint heap_filter, jclass klass_filter, IterationConfig* config) {
  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_iteration_callback = HeapIterationCallback;

  jvmtiError ret = jvmti_env->IterateThroughHeap(heap_filter,
                                                 klass_filter,
                                                 &callbacks,
                                                 config);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return false;
  }
  return true;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test906_iterateThroughHeapCount(
    JNIEnv* env,
    jclass klass ATTRIBUTE_UNUSED,
    jint heap_filter,
    jclass klass_filter,
    jint stop_after) {
  class CountIterationConfig : public IterationConfig {
   public:
    CountIterationConfig(jint _counter, jint _stop_after)
        : counter(_counter),
          stop_after(_stop_after) {
    }

    jint Handle(jlong class_tag ATTRIBUTE_UNUSED,
                jlong size ATTRIBUTE_UNUSED,
                jlong* tag_ptr ATTRIBUTE_UNUSED,
                jint length ATTRIBUTE_UNUSED) OVERRIDE {
      counter++;
      if (counter == stop_after) {
        return JVMTI_VISIT_ABORT;
      }
      return 0;
    }

    jint counter;
    const jint stop_after;
  };

  CountIterationConfig config(0, stop_after);
  Run(env, heap_filter, klass_filter, &config);

  if (config.counter > config.stop_after) {
    printf("Error: more objects visited than signaled.");
  }

  return config.counter;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test906_iterateThroughHeapData(
    JNIEnv* env,
    jclass klass ATTRIBUTE_UNUSED,
    jint heap_filter,
    jclass klass_filter,
    jlongArray class_tags,
    jlongArray sizes,
    jlongArray tags,
    jintArray lengths) {
  class DataIterationConfig : public IterationConfig {
   public:
    jint Handle(jlong class_tag, jlong size, jlong* tag_ptr, jint length) OVERRIDE {
      class_tags_.push_back(class_tag);
      sizes_.push_back(size);
      tags_.push_back(*tag_ptr);
      lengths_.push_back(length);

      return 0;  // Continue.
    }

    std::vector<jlong> class_tags_;
    std::vector<jlong> sizes_;
    std::vector<jlong> tags_;
    std::vector<jint> lengths_;
  };

  DataIterationConfig config;
  if (!Run(env, heap_filter, klass_filter, &config)) {
    return -1;
  }

  ScopedLongArrayRW s_class_tags(env, class_tags);
  ScopedLongArrayRW s_sizes(env, sizes);
  ScopedLongArrayRW s_tags(env, tags);
  ScopedIntArrayRW s_lengths(env, lengths);

  for (size_t i = 0; i != config.class_tags_.size(); ++i) {
    s_class_tags[i] = config.class_tags_[i];
    s_sizes[i] = config.sizes_[i];
    s_tags[i] = config.tags_[i];
    s_lengths[i] = config.lengths_[i];
  }

  return static_cast<jint>(config.class_tags_.size());
}

extern "C" JNIEXPORT void JNICALL Java_art_Test906_iterateThroughHeapAdd(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jint heap_filter, jclass klass_filter) {
  class AddIterationConfig : public IterationConfig {
   public:
    AddIterationConfig() {}

    jint Handle(jlong class_tag ATTRIBUTE_UNUSED,
                jlong size ATTRIBUTE_UNUSED,
                jlong* tag_ptr,
                jint length ATTRIBUTE_UNUSED) OVERRIDE {
      jlong current_tag = *tag_ptr;
      if (current_tag != 0) {
        *tag_ptr = current_tag + 10;
      }
      return 0;
    }
  };

  AddIterationConfig config;
  Run(env, heap_filter, klass_filter, &config);
}

extern "C" JNIEXPORT jstring JNICALL Java_art_Test906_iterateThroughHeapString(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag) {
  struct FindStringCallbacks {
    explicit FindStringCallbacks(jlong t) : tag_to_find(t) {}

    static jint JNICALL HeapIterationCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                              jlong size ATTRIBUTE_UNUSED,
                                              jlong* tag_ptr ATTRIBUTE_UNUSED,
                                              jint length ATTRIBUTE_UNUSED,
                                              void* user_data ATTRIBUTE_UNUSED) {
      return 0;
    }

    static jint JNICALL StringValueCallback(jlong class_tag,
                                            jlong size,
                                            jlong* tag_ptr,
                                            const jchar* value,
                                            jint value_length,
                                            void* user_data) {
      FindStringCallbacks* p = reinterpret_cast<FindStringCallbacks*>(user_data);
      if (*tag_ptr == p->tag_to_find) {
        size_t utf_byte_count = ti::CountUtf8Bytes(value, value_length);
        std::unique_ptr<char[]> mod_utf(new char[utf_byte_count + 1]);
        memset(mod_utf.get(), 0, utf_byte_count + 1);
        ti::ConvertUtf16ToModifiedUtf8(mod_utf.get(), utf_byte_count, value, value_length);
        if (!p->data.empty()) {
          p->data += "\n";
        }
        p->data += android::base::StringPrintf("%" PRId64 "@%" PRId64 " (%" PRId64 ", '%s')",
                                               *tag_ptr,
                                               class_tag,
                                               size,
                                               mod_utf.get());
        // Update the tag to test whether that works.
        *tag_ptr = *tag_ptr + 1;
      }
      return 0;
    }

    std::string data;
    const jlong tag_to_find;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_iteration_callback = FindStringCallbacks::HeapIterationCallback;
  callbacks.string_primitive_value_callback = FindStringCallbacks::StringValueCallback;

  FindStringCallbacks fsc(tag);
  jvmtiError ret = jvmti_env->IterateThroughHeap(0, nullptr, &callbacks, &fsc);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }
  return env->NewStringUTF(fsc.data.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_art_Test906_iterateThroughHeapPrimitiveArray(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag) {
  struct FindArrayCallbacks {
    explicit FindArrayCallbacks(jlong t) : tag_to_find(t) {}

    static jint JNICALL HeapIterationCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                              jlong size ATTRIBUTE_UNUSED,
                                              jlong* tag_ptr ATTRIBUTE_UNUSED,
                                              jint length ATTRIBUTE_UNUSED,
                                              void* user_data ATTRIBUTE_UNUSED) {
      return 0;
    }

    static jint JNICALL ArrayValueCallback(jlong class_tag,
                                           jlong size,
                                           jlong* tag_ptr,
                                           jint element_count,
                                           jvmtiPrimitiveType element_type,
                                           const void* elements,
                                           void* user_data) {
      FindArrayCallbacks* p = reinterpret_cast<FindArrayCallbacks*>(user_data);
      if (*tag_ptr == p->tag_to_find) {
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
    const jlong tag_to_find;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_iteration_callback = FindArrayCallbacks::HeapIterationCallback;
  callbacks.array_primitive_value_callback = FindArrayCallbacks::ArrayValueCallback;

  FindArrayCallbacks fac(tag);
  jvmtiError ret = jvmti_env->IterateThroughHeap(0, nullptr, &callbacks, &fac);
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

extern "C" JNIEXPORT jstring JNICALL Java_art_Test906_iterateThroughHeapPrimitiveFields(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jlong tag) {
  struct FindFieldCallbacks {
    explicit FindFieldCallbacks(jlong t) : tag_to_find(t) {}

    static jint JNICALL HeapIterationCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                              jlong size ATTRIBUTE_UNUSED,
                                              jlong* tag_ptr ATTRIBUTE_UNUSED,
                                              jint length ATTRIBUTE_UNUSED,
                                              void* user_data ATTRIBUTE_UNUSED) {
      return 0;
    }

    static jint JNICALL PrimitiveFieldValueCallback(jvmtiHeapReferenceKind kind,
                                                    const jvmtiHeapReferenceInfo* info,
                                                    jlong class_tag,
                                                    jlong* tag_ptr,
                                                    jvalue value,
                                                    jvmtiPrimitiveType value_type,
                                                    void* user_data) {
      FindFieldCallbacks* p = reinterpret_cast<FindFieldCallbacks*>(user_data);
      if (*tag_ptr >= p->tag_to_find) {
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
        *tag_ptr = *tag_ptr + 1;
      }
      return 0;
    }

    std::string data;
    const jlong tag_to_find;
  };

  jvmtiHeapCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiHeapCallbacks));
  callbacks.heap_iteration_callback = FindFieldCallbacks::HeapIterationCallback;
  callbacks.primitive_field_callback = FindFieldCallbacks::PrimitiveFieldValueCallback;

  FindFieldCallbacks ffc(tag);
  jvmtiError ret = jvmti_env->IterateThroughHeap(0, nullptr, &callbacks, &ffc);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }
  return env->NewStringUTF(ffc.data.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test906_checkInitialized(
    JNIEnv* env, jclass, jclass c) {
  jint status;
  jvmtiError error = jvmti_env->GetClassStatus(c, &status);
  if (JvmtiErrorToException(env, jvmti_env, error)) {
    return false;
  }
  return (status & JVMTI_CLASS_STATUS_INITIALIZED) != 0;
}

}  // namespace Test906IterateHeap
}  // namespace art
