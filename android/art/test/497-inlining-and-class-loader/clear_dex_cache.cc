/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "art_method.h"
#include "base/enums.h"
#include "jni.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

extern "C" JNIEXPORT jobject JNICALL Java_Main_cloneResolvedMethods(JNIEnv* env,
                                                                    jclass,
                                                                    jclass cls) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = soa.Decode<mirror::Class>(cls)->GetDexCache();
  size_t num_methods = dex_cache->NumResolvedMethods();
  mirror::MethodDexCacheType* methods = dex_cache->GetResolvedMethods();
  CHECK_EQ(num_methods != 0u, methods != nullptr);
  if (num_methods == 0u) {
    return nullptr;
  }
  jarray array;
  if (sizeof(void*) == 4) {
    array = env->NewIntArray(2u * num_methods);
  } else {
    array = env->NewLongArray(2u * num_methods);
  }
  CHECK(array != nullptr);
  ObjPtr<mirror::Array> decoded_array = soa.Decode<mirror::Array>(array);
  for (size_t i = 0; i != num_methods; ++i) {
    auto pair = mirror::DexCache::GetNativePairPtrSize(methods, i, kRuntimePointerSize);
    uint32_t index = pair.index;
    ArtMethod* method = pair.object;
    if (sizeof(void*) == 4) {
      ObjPtr<mirror::IntArray> int_array = down_cast<mirror::IntArray*>(decoded_array.Ptr());
      int_array->Set(2u * i, index);
      int_array->Set(2u * i + 1u, static_cast<jint>(reinterpret_cast<uintptr_t>(method)));
    } else {
      ObjPtr<mirror::LongArray> long_array = down_cast<mirror::LongArray*>(decoded_array.Ptr());
      long_array->Set(2u * i, index);
      long_array->Set(2u * i + 1u, reinterpret_cast64<jlong>(method));
    }
  }
  return array;
}

extern "C" JNIEXPORT void JNICALL Java_Main_restoreResolvedMethods(
    JNIEnv*, jclass, jclass cls, jobject old_cache) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = soa.Decode<mirror::Class>(cls)->GetDexCache();
  size_t num_methods = dex_cache->NumResolvedMethods();
  mirror::MethodDexCacheType* methods =
      soa.Decode<mirror::Class>(cls)->GetDexCache()->GetResolvedMethods();
  CHECK_EQ(num_methods != 0u, methods != nullptr);
  ObjPtr<mirror::Array> old = soa.Decode<mirror::Array>(old_cache);
  CHECK_EQ(methods != nullptr, old != nullptr);
  CHECK_EQ(num_methods, static_cast<size_t>(old->GetLength()));
  for (size_t i = 0; i != num_methods; ++i) {
    uint32_t index;
    ArtMethod* method;
    if (sizeof(void*) == 4) {
      ObjPtr<mirror::IntArray> int_array = down_cast<mirror::IntArray*>(old.Ptr());
      index = static_cast<uint32_t>(int_array->Get(2u * i));
      method = reinterpret_cast<ArtMethod*>(static_cast<uint32_t>(int_array->Get(2u * i + 1u)));
    } else {
      ObjPtr<mirror::LongArray> long_array = down_cast<mirror::LongArray*>(old.Ptr());
      index = dchecked_integral_cast<uint32_t>(long_array->Get(2u * i));
      method = reinterpret_cast64<ArtMethod*>(long_array->Get(2u * i + 1u));
    }
    mirror::MethodDexCachePair pair(method, index);
    mirror::DexCache::SetNativePairPtrSize(methods, i, pair, kRuntimePointerSize);
  }
}

}  // namespace

}  // namespace art
