/* Copyright (C) 2016 The Android Open Source Project
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

#include "ti_field.h"

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "base/enums.h"
#include "dex/dex_file_annotations.h"
#include "dex/modifiers.h"
#include "jni_internal.h"
#include "mirror/object_array-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace openjdkjvmti {

// Note: For all these functions, we could do a check that the field actually belongs to the given
//       class. But the spec seems to assume a certain encoding of the field ID, and so doesn't
//       specify any errors.

jvmtiError FieldUtil::GetFieldName(jvmtiEnv* env,
                                   jclass klass,
                                   jfieldID field,
                                   char** name_ptr,
                                   char** signature_ptr,
                                   char** generic_ptr) {
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ArtField* art_field = art::jni::DecodeArtField(field);

  JvmtiUniquePtr<char[]> name_copy;
  if (name_ptr != nullptr) {
    const char* field_name = art_field->GetName();
    if (field_name == nullptr) {
      field_name = "<error>";
    }
    jvmtiError ret;
    name_copy = CopyString(env, field_name, &ret);
    if (name_copy == nullptr) {
      return ret;
    }
    *name_ptr = name_copy.get();
  }

  JvmtiUniquePtr<char[]> signature_copy;
  if (signature_ptr != nullptr) {
    const char* sig = art_field->GetTypeDescriptor();
    jvmtiError ret;
    signature_copy = CopyString(env, sig, &ret);
    if (signature_copy == nullptr) {
      return ret;
    }
    *signature_ptr = signature_copy.get();
  }

  if (generic_ptr != nullptr) {
    *generic_ptr = nullptr;
    if (!art_field->GetDeclaringClass()->IsProxyClass()) {
      art::mirror::ObjectArray<art::mirror::String>* str_array =
          art::annotations::GetSignatureAnnotationForField(art_field);
      if (str_array != nullptr) {
        std::ostringstream oss;
        for (int32_t i = 0; i != str_array->GetLength(); ++i) {
          oss << str_array->Get(i)->ToModifiedUtf8();
        }
        std::string output_string = oss.str();
        jvmtiError ret;
        JvmtiUniquePtr<char[]> copy = CopyString(env, output_string.c_str(), &ret);
        if (copy == nullptr) {
          return ret;
        }
        *generic_ptr = copy.release();
      } else if (soa.Self()->IsExceptionPending()) {
        // TODO: Should we report an error here?
        soa.Self()->ClearException();
      }
    }
  }

  // Everything is fine, release the buffers.
  name_copy.release();
  signature_copy.release();

  return ERR(NONE);
}

jvmtiError FieldUtil::GetFieldDeclaringClass(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                             jclass klass,
                                             jfieldID field,
                                             jclass* declaring_class_ptr) {
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  if (declaring_class_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ArtField* art_field = art::jni::DecodeArtField(field);
  art::ObjPtr<art::mirror::Class> field_klass = art_field->GetDeclaringClass();

  *declaring_class_ptr = soa.AddLocalReference<jclass>(field_klass);

  return ERR(NONE);
}

jvmtiError FieldUtil::GetFieldModifiers(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        jclass klass,
                                        jfieldID field,
                                        jint* modifiers_ptr) {
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  if (modifiers_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ArtField* art_field = art::jni::DecodeArtField(field);
  // Note: Keep this code in sync with Field.getModifiers.
  uint32_t modifiers = art_field->GetAccessFlags() & 0xFFFF;

  *modifiers_ptr = modifiers;
  return ERR(NONE);
}

jvmtiError FieldUtil::IsFieldSynthetic(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                       jclass klass,
                                       jfieldID field,
                                       jboolean* is_synthetic_ptr) {
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  if (is_synthetic_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ArtField* art_field = art::jni::DecodeArtField(field);
  uint32_t modifiers = art_field->GetAccessFlags();

  *is_synthetic_ptr = ((modifiers & art::kAccSynthetic) != 0) ? JNI_TRUE : JNI_FALSE;
  return ERR(NONE);
}

jvmtiError FieldUtil::SetFieldModificationWatch(jvmtiEnv* jenv, jclass klass, jfieldID field) {
  ArtJvmTiEnv* env = ArtJvmTiEnv::AsArtJvmTiEnv(jenv);
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  auto res_pair = env->modify_watched_fields.insert(art::jni::DecodeArtField(field));
  if (!res_pair.second) {
    // Didn't get inserted because it's already present!
    return ERR(DUPLICATE);
  }
  return OK;
}

jvmtiError FieldUtil::ClearFieldModificationWatch(jvmtiEnv* jenv, jclass klass, jfieldID field) {
  ArtJvmTiEnv* env = ArtJvmTiEnv::AsArtJvmTiEnv(jenv);
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  auto pos = env->modify_watched_fields.find(art::jni::DecodeArtField(field));
  if (pos == env->modify_watched_fields.end()) {
    return ERR(NOT_FOUND);
  }
  env->modify_watched_fields.erase(pos);
  return OK;
}

jvmtiError FieldUtil::SetFieldAccessWatch(jvmtiEnv* jenv, jclass klass, jfieldID field) {
  ArtJvmTiEnv* env = ArtJvmTiEnv::AsArtJvmTiEnv(jenv);
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  auto res_pair = env->access_watched_fields.insert(art::jni::DecodeArtField(field));
  if (!res_pair.second) {
    // Didn't get inserted because it's already present!
    return ERR(DUPLICATE);
  }
  return OK;
}

jvmtiError FieldUtil::ClearFieldAccessWatch(jvmtiEnv* jenv, jclass klass, jfieldID field) {
  ArtJvmTiEnv* env = ArtJvmTiEnv::AsArtJvmTiEnv(jenv);
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  if (field == nullptr) {
    return ERR(INVALID_FIELDID);
  }
  auto pos = env->access_watched_fields.find(art::jni::DecodeArtField(field));
  if (pos == env->access_watched_fields.end()) {
    return ERR(NOT_FOUND);
  }
  env->access_watched_fields.erase(pos);
  return OK;
}

}  // namespace openjdkjvmti
