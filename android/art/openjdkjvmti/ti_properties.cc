/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_properties.h"

#include <string.h>
#include <vector>

#include "jni.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"

#include "art_jvmti.h"
#include "runtime.h"
#include "thread-current-inl.h"
#include "ti_phase.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

// Hardcoded properties. Tests ensure that these are consistent with libcore's view, as seen
// in System.java and AndroidHardcodedSystemProperties.java.
static constexpr const char* kProperties[][2] = {
    // Recommended by the spec.
    { "java.vm.vendor", "The Android Project" },
    { "java.vm.version", "2.1.0" },  // This is Runtime::GetVersion().
    { "java.vm.name", "Dalvik" },
    // Android does not provide java.vm.info.
    //
    // These are other values provided by AndroidHardcodedSystemProperties.
    { "java.class.version", "50.0" },
    { "java.version", "0" },
    { "java.compiler", "" },
    { "java.ext.dirs", "" },

    { "java.specification.name", "Dalvik Core Library" },
    { "java.specification.vendor", "The Android Project" },
    { "java.specification.version", "0.9" },

    { "java.vendor", "The Android Project" },
    { "java.vendor.url", "http://www.android.com/" },
    { "java.vm.name", "Dalvik" },
    { "java.vm.specification.name", "Dalvik Virtual Machine Specification" },
    { "java.vm.specification.vendor", "The Android Project" },
    { "java.vm.specification.version", "0.9" },
    { "java.vm.vendor", "The Android Project" },

    { "java.vm.vendor.url", "http://www.android.com/" },

    { "java.net.preferIPv6Addresses", "false" },

    { "file.encoding", "UTF-8" },

    { "file.separator", "/" },
    { "line.separator", "\n" },
    { "path.separator", ":" },

    { "os.name", "Linux" },
};
static constexpr size_t kPropertiesSize = arraysize(kProperties);
static constexpr const char* kPropertyLibraryPath = "java.library.path";
static constexpr const char* kPropertyClassPath = "java.class.path";

jvmtiError PropertiesUtil::GetSystemProperties(jvmtiEnv* env,
                                               jint* count_ptr,
                                               char*** property_ptr) {
  if (count_ptr == nullptr || property_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  jvmtiError array_alloc_result;
  JvmtiUniquePtr<char*[]> array_data_ptr = AllocJvmtiUniquePtr<char*[]>(env,
                                                                        kPropertiesSize + 2,
                                                                        &array_alloc_result);
  if (array_data_ptr == nullptr) {
    return array_alloc_result;
  }

  std::vector<JvmtiUniquePtr<char[]>> property_copies;

  {
    jvmtiError libpath_result;
    JvmtiUniquePtr<char[]> libpath_data = CopyString(env, kPropertyLibraryPath, &libpath_result);
    if (libpath_data == nullptr) {
      return libpath_result;
    }
    array_data_ptr.get()[0] = libpath_data.get();
    property_copies.push_back(std::move(libpath_data));
  }

  {
    jvmtiError classpath_result;
    JvmtiUniquePtr<char[]> classpath_data = CopyString(env, kPropertyClassPath, &classpath_result);
    if (classpath_data == nullptr) {
      return classpath_result;
    }
    array_data_ptr.get()[1] = classpath_data.get();
    property_copies.push_back(std::move(classpath_data));
  }

  for (size_t i = 0; i != kPropertiesSize; ++i) {
    jvmtiError data_result;
    JvmtiUniquePtr<char[]> data = CopyString(env, kProperties[i][0], &data_result);
    if (data == nullptr) {
      return data_result;
    }
    array_data_ptr.get()[i + 2] = data.get();
    property_copies.push_back(std::move(data));
  }

  // Everything is OK, release the data.
  *count_ptr = kPropertiesSize + 2;
  *property_ptr = array_data_ptr.release();
  for (auto& uptr : property_copies) {
    uptr.release();
  }

  return ERR(NONE);
}

static jvmtiError Copy(jvmtiEnv* env, const char* in, char** out) {
  jvmtiError result;
  JvmtiUniquePtr<char[]> data = CopyString(env, in, &result);
  *out = data.release();
  return result;
}

// See dalvik_system_VMRuntime.cpp.
static const char* DefaultToDot(const std::string& class_path) {
  return class_path.empty() ? "." : class_path.c_str();
}

// Handle kPropertyLibraryPath.
static jvmtiError GetLibraryPath(jvmtiEnv* env, char** value_ptr) {
  const std::vector<std::string>& runtime_props = art::Runtime::Current()->GetProperties();
  for (const std::string& prop_assignment : runtime_props) {
    size_t assign_pos = prop_assignment.find('=');
    if (assign_pos != std::string::npos && assign_pos > 0) {
      if (prop_assignment.substr(0, assign_pos) == kPropertyLibraryPath) {
        return Copy(env, prop_assignment.substr(assign_pos + 1).c_str(), value_ptr);
      }
    }
  }
  if (!PhaseUtil::IsLivePhase()) {
    return ERR(NOT_AVAILABLE);
  }
  // We expect this call to be rare. So don't optimize.
  DCHECK(art::Thread::Current() != nullptr);
  JNIEnv* jni_env = art::Thread::Current()->GetJniEnv();
  jmethodID get_prop = jni_env->GetStaticMethodID(art::WellKnownClasses::java_lang_System,
                                                  "getProperty",
                                                  "(Ljava/lang/String;)Ljava/lang/String;");
  CHECK(get_prop != nullptr);

  ScopedLocalRef<jobject> input_str(jni_env, jni_env->NewStringUTF(kPropertyLibraryPath));
  if (input_str.get() == nullptr) {
    jni_env->ExceptionClear();
    return ERR(OUT_OF_MEMORY);
  }

  ScopedLocalRef<jobject> prop_res(
      jni_env, jni_env->CallStaticObjectMethod(art::WellKnownClasses::java_lang_System,
                                               get_prop,
                                               input_str.get()));
  if (jni_env->ExceptionCheck() == JNI_TRUE) {
    jni_env->ExceptionClear();
    return ERR(INTERNAL);
  }
  if (prop_res.get() == nullptr) {
    *value_ptr = nullptr;
    return ERR(NONE);
  }

  ScopedUtfChars chars(jni_env, reinterpret_cast<jstring>(prop_res.get()));
  return Copy(env, chars.c_str(), value_ptr);
}

jvmtiError PropertiesUtil::GetSystemProperty(jvmtiEnv* env,
                                             const char* property,
                                             char** value_ptr) {
  if (property == nullptr || value_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  if (strcmp(property, kPropertyLibraryPath) == 0) {
    return GetLibraryPath(env, value_ptr);
  }

  if (strcmp(property, kPropertyClassPath) == 0) {
    return Copy(env, DefaultToDot(art::Runtime::Current()->GetClassPathString()), value_ptr);
  }

  for (size_t i = 0; i != kPropertiesSize; ++i) {
    if (strcmp(property, kProperties[i][0]) == 0) {
      return Copy(env, kProperties[i][1], value_ptr);
    }
  }

  return ERR(NOT_AVAILABLE);
}

jvmtiError PropertiesUtil::SetSystemProperty(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                             const char* property ATTRIBUTE_UNUSED,
                                             const char* value ATTRIBUTE_UNUSED) {
  // We do not allow manipulation of any property here.
  return ERR(NOT_AVAILABLE);
}

}  // namespace openjdkjvmti
