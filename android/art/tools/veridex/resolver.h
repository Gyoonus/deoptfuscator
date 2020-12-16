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

#ifndef ART_TOOLS_VERIDEX_RESOLVER_H_
#define ART_TOOLS_VERIDEX_RESOLVER_H_

#include "dex/dex_file.h"
#include "veridex.h"

namespace art {

class HiddenApi;
class VeridexResolver;

/**
 * Map from the start of a dex file (ie DexFile::Begin()), to
 * its corresponding resolver.
 */
using DexResolverMap = std::map<uintptr_t, VeridexResolver*>;

class VeridexResolver {
 public:
  VeridexResolver(const DexFile& dex_file,
                  const DexResolverMap& dex_resolvers,
                  TypeMap& type_map)
      : dex_file_(dex_file),
        type_map_(type_map),
        dex_resolvers_(dex_resolvers),
        type_infos_(dex_file.NumTypeIds(), VeriClass()),
        method_infos_(dex_file.NumMethodIds(), nullptr),
        field_infos_(dex_file.NumFieldIds(), nullptr) {}

  // Run on the defined classes of that dex file and populate our
  // local type cache.
  void Run();

  // Return the class declared at `index`.
  VeriClass* GetVeriClass(dex::TypeIndex index);

  // Return the method declared at `method_index`.
  VeriMethod GetMethod(uint32_t method_index);

  // Return the field declared at `field_index`.
  VeriField GetField(uint32_t field_index);

  // Do a JLS lookup in `kls` to find a method.
  VeriMethod LookupMethodIn(const VeriClass& kls,
                            const char* method_name,
                            const Signature& method_signature);

  // Do a JLS lookup in `kls` to find a field.
  VeriField LookupFieldIn(const VeriClass& kls,
                          const char* field_name,
                          const char* field_type);

  // Lookup a method declared in `kls`.
  VeriMethod LookupDeclaredMethodIn(const VeriClass& kls,
                                    const char* method_name,
                                    const char* signature) const;

  // Resolve all type_id/method_id/field_id.
  void ResolveAll();

  // The dex file this resolver is associated to.
  const DexFile& GetDexFile() const {
    return dex_file_;
  }

  const DexFile& GetDexFileOf(const VeriClass& kls) {
    return GetResolverOf(kls)->dex_file_;
  }

 private:
  // Return the resolver where `kls` is from.
  VeridexResolver* GetResolverOf(const VeriClass& kls) const;

  const DexFile& dex_file_;
  TypeMap& type_map_;
  const DexResolverMap& dex_resolvers_;
  std::vector<VeriClass> type_infos_;
  std::vector<VeriMethod> method_infos_;
  std::vector<VeriField> field_infos_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_RESOLVER_H_
