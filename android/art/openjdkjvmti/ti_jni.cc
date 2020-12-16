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

#include "ti_jni.h"

#include "jni.h"

#include "art_jvmti.h"
#include "base/mutex.h"
#include "java_vm_ext.h"
#include "jni_env_ext.h"
#include "runtime.h"
#include "thread-current-inl.h"

namespace openjdkjvmti {

jvmtiError JNIUtil::SetJNIFunctionTable(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        const jniNativeInterface* function_table) {
  // While we supporting setting null (which will reset the table), the spec says no.
  if (function_table == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::JNIEnvExt::SetTableOverride(function_table);
  return ERR(NONE);
}

jvmtiError JNIUtil::GetJNIFunctionTable(jvmtiEnv* env, jniNativeInterface** function_table) {
  if (function_table == nullptr) {
    return ERR(NULL_POINTER);
  }

  // We use the generic JNIEnvExt::GetFunctionTable instead of querying a specific JNIEnv, as
  // this has to work in the start phase.

  // Figure out which table is current. Conservatively assume check-jni is off.
  bool check_jni = false;
  art::Runtime* runtime = art::Runtime::Current();
  if (runtime != nullptr && runtime->GetJavaVM() != nullptr) {
    check_jni = runtime->GetJavaVM()->IsCheckJniEnabled();
  }

  // Get that table.
  const JNINativeInterface* current_table;
  {
    art::MutexLock mu(art::Thread::Current(), *art::Locks::jni_function_table_lock_);
    current_table = art::JNIEnvExt::GetFunctionTable(check_jni);
  }

  // Allocate memory and copy the table.
  unsigned char* data;
  jvmtiError data_result = env->Allocate(sizeof(JNINativeInterface), &data);
  if (data_result != ERR(NONE)) {
    return data_result;
  }
  memcpy(data, current_table, sizeof(JNINativeInterface));

  *function_table = reinterpret_cast<JNINativeInterface*>(data);

  return ERR(NONE);
}

}  // namespace openjdkjvmti
