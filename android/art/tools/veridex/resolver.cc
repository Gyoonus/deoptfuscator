/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "resolver.h"

#include "dex/dex_file-inl.h"
#include "dex/primitive.h"
#include "hidden_api.h"
#include "veridex.h"

namespace art {

void VeridexResolver::Run() {
  size_t class_def_count = dex_file_.NumClassDefs();
  for (size_t class_def_index = 0; class_def_index < class_def_count; ++class_def_index) {
    const DexFile::ClassDef& class_def = dex_file_.GetClassDef(class_def_index);
    std::string name(dex_file_.StringByTypeIdx(class_def.class_idx_));
    auto existing = type_map_.find(name);
    if (existing != type_map_.end()) {
      // Class already exists, cache it and move on.
      type_infos_[class_def.class_idx_.index_] = *existing->second;
      continue;
    }
    type_infos_[class_def.class_idx_.index_] = VeriClass(Primitive::Type::kPrimNot, 0, &class_def);
    type_map_[name] = &(type_infos_[class_def.class_idx_.index_]);

    const uint8_t* class_data = dex_file_.GetClassData(class_def);
    if (class_data == nullptr) {
      // Empty class.
      continue;
    }

    ClassDataItemIterator it(dex_file_, class_data);
    for (; it.HasNextStaticField(); it.Next()) {
      field_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
    for (; it.HasNextInstanceField(); it.Next()) {
      field_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
    for (; it.HasNextMethod(); it.Next()) {
      method_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
  }
}

static bool HasSameNameAndSignature(const DexFile& dex_file,
                                    const DexFile::MethodId& method_id,
                                    const char* method_name,
                                    const char* type) {
  return strcmp(method_name, dex_file.GetMethodName(method_id)) == 0 &&
      strcmp(type, dex_file.GetMethodSignature(method_id).ToString().c_str()) == 0;
}

static bool HasSameNameAndSignature(const DexFile& dex_file,
                                    const DexFile::MethodId& method_id,
                                    const char* method_name,
                                    const Signature& signature) {
  return strcmp(method_name, dex_file.GetMethodName(method_id)) == 0 &&
      dex_file.GetMethodSignature(method_id) == signature;
}

static bool HasSameNameAndType(const DexFile& dex_file,
                               const DexFile::FieldId& field_id,
                               const char* field_name,
                               const char* field_type) {
  return strcmp(field_name, dex_file.GetFieldName(field_id)) == 0 &&
      strcmp(field_type, dex_file.GetFieldTypeDescriptor(field_id)) == 0;
}

VeriClass* VeridexResolver::GetVeriClass(dex::TypeIndex index) {
  CHECK_LT(index.index_, dex_file_.NumTypeIds());
  // Lookup in our local cache.
  VeriClass* cls = &type_infos_[index.index_];
  if (cls->IsUninitialized()) {
    // Class is defined in another dex file. Lookup in the global cache.
    std::string name(dex_file_.StringByTypeIdx(index));
    auto existing = type_map_.find(name);
    if (existing == type_map_.end()) {
      // Class hasn't been defined, so check if it's an array class.
      size_t last_array = name.find_last_of('[');
      if (last_array == std::string::npos) {
        // There is no such class.
        return nullptr;
      } else {
        // Class is an array class. Check if its most enclosed component type (which is not
        // an array class) has been defined.
        std::string klass_name = name.substr(last_array + 1);
        existing = type_map_.find(klass_name);
        if (existing == type_map_.end()) {
          // There is no such class, so there is no such array.
          return nullptr;
        } else {
          // Create the type, and cache it locally and globally.
          type_infos_[index.index_] = VeriClass(
              existing->second->GetKind(), last_array + 1, existing->second->GetClassDef());
          cls = &(type_infos_[index.index_]);
          type_map_[name] = cls;
        }
      }
    } else {
      // Cache the found class.
      cls = existing->second;
      type_infos_[index.index_] = *cls;
    }
  }
  return cls;
}

VeridexResolver* VeridexResolver::GetResolverOf(const VeriClass& kls) const {
  auto resolver_it = dex_resolvers_.lower_bound(reinterpret_cast<uintptr_t>(kls.GetClassDef()));
  --resolver_it;

  // Check the class def pointer is indeed in the mapped dex file range.
  const DexFile& dex_file = resolver_it->second->dex_file_;
  CHECK_LT(reinterpret_cast<uintptr_t>(dex_file.Begin()),
           reinterpret_cast<uintptr_t>(kls.GetClassDef()));
  CHECK_GT(reinterpret_cast<uintptr_t>(dex_file.Begin()) + dex_file.Size(),
           reinterpret_cast<uintptr_t>(kls.GetClassDef()));
  return resolver_it->second;
}

VeriMethod VeridexResolver::LookupMethodIn(const VeriClass& kls,
                                           const char* method_name,
                                           const Signature& method_signature) {
  if (kls.IsPrimitive()) {
    // Primitive classes don't have methods.
    return nullptr;
  }
  if (kls.IsArray()) {
    // Array classes don't have methods, but inherit the ones in j.l.Object.
    return LookupMethodIn(*VeriClass::object_, method_name, method_signature);
  }
  // Get the resolver where `kls` is from.
  VeridexResolver* resolver = GetResolverOf(kls);

  // Look at methods declared in `kls`.
  const DexFile& other_dex_file = resolver->dex_file_;
  const uint8_t* class_data = other_dex_file.GetClassData(*kls.GetClassDef());
  if (class_data != nullptr) {
    ClassDataItemIterator it(other_dex_file, class_data);
    it.SkipAllFields();
    for (; it.HasNextMethod(); it.Next()) {
      const DexFile::MethodId& other_method_id = other_dex_file.GetMethodId(it.GetMemberIndex());
      if (HasSameNameAndSignature(other_dex_file,
                                  other_method_id,
                                  method_name,
                                  method_signature)) {
        return it.DataPointer();
      }
    }
  }

  // Look at methods in `kls`'s super class hierarchy.
  if (kls.GetClassDef()->superclass_idx_.IsValid()) {
    VeriClass* super = resolver->GetVeriClass(kls.GetClassDef()->superclass_idx_);
    if (super != nullptr) {
      VeriMethod super_method = resolver->LookupMethodIn(*super, method_name, method_signature);
      if (super_method != nullptr) {
        return super_method;
      }
    }
  }

  // Look at methods in `kls`'s interface hierarchy.
  const DexFile::TypeList* interfaces = other_dex_file.GetInterfacesList(*kls.GetClassDef());
  if (interfaces != nullptr) {
    for (size_t i = 0; i < interfaces->Size(); i++) {
      dex::TypeIndex idx = interfaces->GetTypeItem(i).type_idx_;
      VeriClass* itf = resolver->GetVeriClass(idx);
      if (itf != nullptr) {
        VeriMethod itf_method = resolver->LookupMethodIn(*itf, method_name, method_signature);
        if (itf_method != nullptr) {
          return itf_method;
        }
      }
    }
  }
  return nullptr;
}

VeriField VeridexResolver::LookupFieldIn(const VeriClass& kls,
                                         const char* field_name,
                                         const char* field_type) {
  if (kls.IsPrimitive()) {
    // Primitive classes don't have fields.
    return nullptr;
  }
  if (kls.IsArray()) {
    // Array classes don't have fields.
    return nullptr;
  }
  // Get the resolver where `kls` is from.
  VeridexResolver* resolver = GetResolverOf(kls);

  // Look at fields declared in `kls`.
  const DexFile& other_dex_file = resolver->dex_file_;
  const uint8_t* class_data = other_dex_file.GetClassData(*kls.GetClassDef());
  if (class_data != nullptr) {
    ClassDataItemIterator it(other_dex_file, class_data);
    for (; it.HasNextStaticField() || it.HasNextInstanceField(); it.Next()) {
      const DexFile::FieldId& other_field_id = other_dex_file.GetFieldId(it.GetMemberIndex());
      if (HasSameNameAndType(other_dex_file,
                             other_field_id,
                             field_name,
                             field_type)) {
        return it.DataPointer();
      }
    }
  }

  // Look at fields in `kls`'s interface hierarchy.
  const DexFile::TypeList* interfaces = other_dex_file.GetInterfacesList(*kls.GetClassDef());
  if (interfaces != nullptr) {
    for (size_t i = 0; i < interfaces->Size(); i++) {
      dex::TypeIndex idx = interfaces->GetTypeItem(i).type_idx_;
      VeriClass* itf = resolver->GetVeriClass(idx);
      if (itf != nullptr) {
        VeriField itf_field = resolver->LookupFieldIn(*itf, field_name, field_type);
        if (itf_field != nullptr) {
          return itf_field;
        }
      }
    }
  }

  // Look at fields in `kls`'s super class hierarchy.
  if (kls.GetClassDef()->superclass_idx_.IsValid()) {
    VeriClass* super = resolver->GetVeriClass(kls.GetClassDef()->superclass_idx_);
    if (super != nullptr) {
      VeriField super_field = resolver->LookupFieldIn(*super, field_name, field_type);
      if (super_field != nullptr) {
        return super_field;
      }
    }
  }
  return nullptr;
}

VeriMethod VeridexResolver::LookupDeclaredMethodIn(const VeriClass& kls,
                                                   const char* method_name,
                                                   const char* type) const {
  if (kls.IsPrimitive()) {
    return nullptr;
  }
  if (kls.IsArray()) {
    return nullptr;
  }
  VeridexResolver* resolver = GetResolverOf(kls);
  const DexFile& other_dex_file = resolver->dex_file_;
  const uint8_t* class_data = other_dex_file.GetClassData(*kls.GetClassDef());
  if (class_data != nullptr) {
    ClassDataItemIterator it(other_dex_file, class_data);
    it.SkipAllFields();
    for (; it.HasNextMethod(); it.Next()) {
      const DexFile::MethodId& other_method_id = other_dex_file.GetMethodId(it.GetMemberIndex());
      if (HasSameNameAndSignature(other_dex_file,
                                  other_method_id,
                                  method_name,
                                  type)) {
        return it.DataPointer();
      }
    }
  }
  return nullptr;
}

VeriMethod VeridexResolver::GetMethod(uint32_t method_index) {
  VeriMethod method_info = method_infos_[method_index];
  if (method_info == nullptr) {
    // Method is defined in another dex file.
    const DexFile::MethodId& method_id = dex_file_.GetMethodId(method_index);
    VeriClass* kls = GetVeriClass(method_id.class_idx_);
    if (kls == nullptr) {
      return nullptr;
    }
    // Class found, now lookup the method in it.
    method_info = LookupMethodIn(*kls,
                                 dex_file_.GetMethodName(method_id),
                                 dex_file_.GetMethodSignature(method_id));
    method_infos_[method_index] = method_info;
  }
  return method_info;
}

VeriField VeridexResolver::GetField(uint32_t field_index) {
  VeriField field_info = field_infos_[field_index];
  if (field_info == nullptr) {
    // Field is defined in another dex file.
    const DexFile::FieldId& field_id = dex_file_.GetFieldId(field_index);
    VeriClass* kls = GetVeriClass(field_id.class_idx_);
    if (kls == nullptr) {
      return nullptr;
    }
    // Class found, now lookup the field in it.
    field_info = LookupFieldIn(*kls,
                               dex_file_.GetFieldName(field_id),
                               dex_file_.GetFieldTypeDescriptor(field_id));
    field_infos_[field_index] = field_info;
  }
  return field_info;
}

void VeridexResolver::ResolveAll() {
  for (uint32_t i = 0; i < dex_file_.NumTypeIds(); ++i) {
    if (GetVeriClass(dex::TypeIndex(i)) == nullptr) {
      LOG(WARNING) << "Unresolved " << dex_file_.PrettyType(dex::TypeIndex(i));
    }
  }

  for (uint32_t i = 0; i < dex_file_.NumMethodIds(); ++i) {
    if (GetMethod(i) == nullptr) {
      LOG(WARNING) << "Unresolved: " << dex_file_.PrettyMethod(i);
    }
  }

  for (uint32_t i = 0; i < dex_file_.NumFieldIds(); ++i) {
    if (GetField(i) == nullptr) {
      LOG(WARNING) << "Unresolved: " << dex_file_.PrettyField(i);
    }
  }
}

}  // namespace art
