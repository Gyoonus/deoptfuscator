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

#ifndef ART_OPENJDKJVMTI_TI_CLASS_LOADER_INL_H_
#define ART_OPENJDKJVMTI_TI_CLASS_LOADER_INL_H_

#include "ti_class_loader.h"
#include "art_field-inl.h"
#include "handle.h"
#include "handle_scope.h"
#include "jni_internal.h"
#include "mirror/object.h"
#include "mirror/object_array-inl.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

template<typename Visitor>
inline void ClassLoaderHelper::VisitDexFileObjects(art::Thread* self,
                                                   art::Handle<art::mirror::ClassLoader> loader,
                                                   const Visitor& visitor) {
  art::StackHandleScope<1> hs(self);
  art::ArtField* element_dex_file_field = art::jni::DecodeArtField(
      art::WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);

  art::Handle<art::mirror::ObjectArray<art::mirror::Object>> dex_elements_list(
      hs.NewHandle(GetDexElementList(self, loader)));
  if (dex_elements_list == nullptr) {
    return;
  }

  size_t num_elements = dex_elements_list->GetLength();
  // Iterate over the DexPathList$Element to find the right one
  for (size_t i = 0; i < num_elements; i++) {
    art::ObjPtr<art::mirror::Object> current_element = dex_elements_list->Get(i);
    CHECK(!current_element.IsNull());
    art::ObjPtr<art::mirror::Object> dex_file(element_dex_file_field->GetObject(current_element));
    if (!dex_file.IsNull()) {
      if (!visitor(dex_file)) {
        return;
      }
    }
  }
}

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_TI_CLASS_LOADER_INL_H_
