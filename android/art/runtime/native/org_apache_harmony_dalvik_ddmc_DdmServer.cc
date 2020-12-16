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

#include "org_apache_harmony_dalvik_ddmc_DdmServer.h"

#include <android-base/logging.h>

#include "base/array_ref.h"
#include "debugger.h"
#include "jni_internal.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_primitive_array.h"
#include "scoped_fast_native_object_access-inl.h"

namespace art {

static void DdmServer_nativeSendChunk(JNIEnv* env, jclass, jint type,
                                      jbyteArray javaData, jint offset, jint length) {
  ScopedFastNativeObjectAccess soa(env);
  ScopedByteArrayRO data(env, javaData);
  DCHECK_LE(offset + length, static_cast<int32_t>(data.size()));
  ArrayRef<const uint8_t> chunk(reinterpret_cast<const uint8_t*>(&data[offset]),
                                static_cast<size_t>(length));
  Runtime::Current()->GetRuntimeCallbacks()->DdmPublishChunk(static_cast<uint32_t>(type), chunk);
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(DdmServer, nativeSendChunk, "(I[BII)V"),
};

void register_org_apache_harmony_dalvik_ddmc_DdmServer(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("org/apache/harmony/dalvik/ddmc/DdmServer");
}

}  // namespace art
