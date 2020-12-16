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

#include "jni.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "object_lock.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

extern "C" JNIEXPORT void JNICALL Java_Main_nativeClearResolvedTypes(JNIEnv*, jclass, jclass cls) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = soa.Decode<mirror::Class>(cls)->GetDexCache();
  for (size_t i = 0, num_types = dex_cache->NumResolvedTypes(); i != num_types; ++i) {
    mirror::TypeDexCachePair cleared(nullptr, mirror::TypeDexCachePair::InvalidIndexForSlot(i));
    dex_cache->GetResolvedTypes()[i].store(cleared, std::memory_order_relaxed);
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_nativeSkipVerification(JNIEnv*, jclass, jclass cls) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass = hs.NewHandle(soa.Decode<mirror::Class>(cls));
  ClassStatus status = klass->GetStatus();
  if (status == ClassStatus::kResolved) {
    ObjectLock<mirror::Class> lock(soa.Self(), klass);
    klass->SetStatus(klass, ClassStatus::kVerified, soa.Self());
  } else {
    LOG(ERROR) << klass->PrettyClass() << " has unexpected status: " << status;
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_nativeDumpClasses(JNIEnv*, jclass, jobjectArray array) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::Object>> classes =
      hs.NewHandle(soa.Decode<mirror::ObjectArray<mirror::Object>>(array));
  CHECK(classes != nullptr);
  for (size_t i = 0, length = classes->GetLength(); i != length; ++i) {
    CHECK(classes->Get(i) != nullptr) << i;
    CHECK(classes->Get(i)->IsClass())
        << i << " " << classes->Get(i)->GetClass()->PrettyDescriptor();
    mirror::Class* as_class = classes->Get(i)->AsClass();
    mirror::ClassLoader* loader = as_class->GetClassLoader();
    LOG(ERROR) << "Class #" << i << ": " << as_class->PrettyDescriptor()
        << " @" << static_cast<const void*>(as_class)
        << " status:" << as_class->GetStatus()
        << " definingLoader:" << static_cast<const void*>(loader)
        << " definingLoaderClass:"
        << (loader != nullptr ? loader->GetClass()->PrettyDescriptor() : "N/A");
  }
}

}  // namespace art
