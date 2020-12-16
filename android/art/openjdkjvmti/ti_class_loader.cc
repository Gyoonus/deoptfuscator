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

#include "ti_class_loader-inl.h"

#include <limits>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "events-inl.h"
#include "gc/allocation_listener.h"
#include "gc/heap.h"
#include "instrumentation.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni_env_ext-inl.h"
#include "jvmti_allocator.h"
#include "mirror/class.h"
#include "mirror/class_ext.h"
#include "mirror/object.h"
#include "nativehelper/scoped_local_ref.h"
#include "object_lock.h"
#include "runtime.h"
#include "transform.h"

namespace openjdkjvmti {

bool ClassLoaderHelper::AddToClassLoader(art::Thread* self,
                                         art::Handle<art::mirror::ClassLoader> loader,
                                         const art::DexFile* dex_file) {
  art::ScopedObjectAccessUnchecked soa(self);
  art::StackHandleScope<3> hs(self);
  if (art::ClassLinker::IsBootClassLoader(soa, loader.Get())) {
    art::Runtime::Current()->GetClassLinker()->AppendToBootClassPath(self, *dex_file);
    return true;
  }
  art::Handle<art::mirror::Object> java_dex_file_obj(
      hs.NewHandle(FindSourceDexFileObject(self, loader)));
  if (java_dex_file_obj.IsNull()) {
    return false;
  }
  art::Handle<art::mirror::LongArray> old_cookie(hs.NewHandle(GetDexFileCookie(java_dex_file_obj)));
  art::Handle<art::mirror::LongArray> cookie(hs.NewHandle(
      AllocateNewDexFileCookie(self, old_cookie, dex_file)));
  if (cookie.IsNull()) {
    return false;
  }
  art::ScopedAssertNoThreadSuspension nts("Replacing cookie fields in j.l.DexFile object");
  UpdateJavaDexFile(java_dex_file_obj.Get(), cookie.Get());
  return true;
}

void ClassLoaderHelper::UpdateJavaDexFile(art::ObjPtr<art::mirror::Object> java_dex_file,
                                          art::ObjPtr<art::mirror::LongArray> new_cookie) {
  art::ArtField* internal_cookie_field = java_dex_file->GetClass()->FindDeclaredInstanceField(
      "mInternalCookie", "Ljava/lang/Object;");
  art::ArtField* cookie_field = java_dex_file->GetClass()->FindDeclaredInstanceField(
      "mCookie", "Ljava/lang/Object;");
  CHECK(internal_cookie_field != nullptr);
  art::ObjPtr<art::mirror::LongArray> orig_internal_cookie(
      internal_cookie_field->GetObject(java_dex_file)->AsLongArray());
  art::ObjPtr<art::mirror::LongArray> orig_cookie(
      cookie_field->GetObject(java_dex_file)->AsLongArray());
  internal_cookie_field->SetObject<false>(java_dex_file, new_cookie);
  if (!orig_cookie.IsNull()) {
    cookie_field->SetObject<false>(java_dex_file, new_cookie);
  }
}

art::ObjPtr<art::mirror::LongArray> ClassLoaderHelper::GetDexFileCookie(
    art::Handle<art::mirror::Object> java_dex_file_obj) {
  // mCookie is nulled out if the DexFile has been closed but mInternalCookie sticks around until
  // the object is finalized. Since they always point to the same array if mCookie is not null we
  // just use the mInternalCookie field. We will update one or both of these fields later.
  art::ArtField* internal_cookie_field = java_dex_file_obj->GetClass()->FindDeclaredInstanceField(
      "mInternalCookie", "Ljava/lang/Object;");
  // TODO Add check that mCookie is either null or same as mInternalCookie
  CHECK(internal_cookie_field != nullptr);
  return internal_cookie_field->GetObject(java_dex_file_obj.Get())->AsLongArray();
}

art::ObjPtr<art::mirror::LongArray> ClassLoaderHelper::AllocateNewDexFileCookie(
    art::Thread* self,
    art::Handle<art::mirror::LongArray> cookie,
    const art::DexFile* dex_file) {
  art::StackHandleScope<1> hs(self);
  CHECK(cookie != nullptr);
  CHECK_GE(cookie->GetLength(), 1);
  art::Handle<art::mirror::LongArray> new_cookie(
      hs.NewHandle(art::mirror::LongArray::Alloc(self, cookie->GetLength() + 1)));
  if (new_cookie == nullptr) {
    self->AssertPendingOOMException();
    return nullptr;
  }
  // Copy the oat-dex field at the start.
  new_cookie->SetWithoutChecks<false>(0, cookie->GetWithoutChecks(0));
  // This must match the casts in runtime/native/dalvik_system_DexFile.cc:ConvertDexFilesToJavaArray
  new_cookie->SetWithoutChecks<false>(
      1, static_cast<int64_t>(reinterpret_cast<uintptr_t>(dex_file)));
  new_cookie->Memcpy(2, cookie.Get(), 1, cookie->GetLength() - 1);
  return new_cookie.Get();
}

art::ObjPtr<art::mirror::ObjectArray<art::mirror::Object>> ClassLoaderHelper::GetDexElementList(
    art::Thread* self,
    art::Handle<art::mirror::ClassLoader> loader) {
  art::StackHandleScope<4> hs(self);

  art::Handle<art::mirror::Class>
      base_dex_loader_class(hs.NewHandle(self->DecodeJObject(
          art::WellKnownClasses::dalvik_system_BaseDexClassLoader)->AsClass()));

  // Get all the ArtFields so we can look in the BaseDexClassLoader
  art::ArtField* path_list_field = art::jni::DecodeArtField(
      art::WellKnownClasses::dalvik_system_BaseDexClassLoader_pathList);
  art::ArtField* dex_path_list_element_field =
      art::jni::DecodeArtField(art::WellKnownClasses::dalvik_system_DexPathList_dexElements);

  // Check if loader is a BaseDexClassLoader
  art::Handle<art::mirror::Class> loader_class(hs.NewHandle(loader->GetClass()));
  // Currently only base_dex_loader is allowed to actually define classes but if this changes in the
  // future we should make sure to support all class loader types.
  if (!loader_class->IsSubClass(base_dex_loader_class.Get())) {
    LOG(ERROR) << "The classloader " << loader_class->PrettyClass() << " is not a "
               << base_dex_loader_class->PrettyClass() << " which is currently the only "
               << "supported class loader type!";
    return nullptr;
  }
  // Start navigating the fields of the loader (now known to be a BaseDexClassLoader derivative)
  art::Handle<art::mirror::Object> path_list(
      hs.NewHandle(path_list_field->GetObject(loader.Get())));
  CHECK(path_list != nullptr);
  art::ObjPtr<art::mirror::ObjectArray<art::mirror::Object>> dex_elements_list =
      dex_path_list_element_field->GetObject(path_list.Get())->AsObjectArray<art::mirror::Object>();
  return dex_elements_list;
}

// TODO This should return the actual source java.lang.DexFile object for the klass being loaded.
art::ObjPtr<art::mirror::Object> ClassLoaderHelper::FindSourceDexFileObject(
    art::Thread* self, art::Handle<art::mirror::ClassLoader> loader) {
  art::ObjPtr<art::mirror::Object> res = nullptr;
  VisitDexFileObjects(self,
                      loader,
                      [&] (art::ObjPtr<art::mirror::Object> dex_file) {
                        res = dex_file;
                        // Just stop at the first one.
                        // TODO It would be cleaner to put the art::DexFile into the
                        // dalvik.system.DexFile the class comes from but it is more annoying
                        // because we would need to find this class. It is not necessary for proper
                        // function since we just need to be in front of the classes old dex file in
                        // the path.
                        return false;
                      });
  return res;
}

}  // namespace openjdkjvmti
