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

#ifndef ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_
#define ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_

#include "dex/code_item_accessors.h"
#include "dex/dex_file_reference.h"
#include "dex/method_reference.h"
#include "hidden_api.h"
#include "resolver.h"
#include "veridex.h"

namespace art {

/**
 * The source where a dex register comes from.
 */
enum class RegisterSource {
  kParameter,
  kField,
  kMethod,
  kClass,
  kString,
  kConstant,
  kNone
};

/**
 * Abstract representation of a dex register value.
 */
class RegisterValue {
 public:
  RegisterValue() : source_(RegisterSource::kNone),
                    value_(0),
                    reference_(nullptr, 0),
                    type_(nullptr) {}
  RegisterValue(RegisterSource source, DexFileReference reference, const VeriClass* type)
      : source_(source), value_(0), reference_(reference), type_(type) {}

  RegisterValue(RegisterSource source,
                uint32_t value,
                DexFileReference reference,
                const VeriClass* type)
      : source_(source), value_(value), reference_(reference), type_(type) {}

  RegisterSource GetSource() const { return source_; }
  DexFileReference GetDexFileReference() const { return reference_; }
  const VeriClass* GetType() const { return type_; }
  uint32_t GetParameterIndex() const {
    CHECK(IsParameter());
    return value_;
  }
  uint32_t GetConstant() const {
    CHECK(IsConstant());
    return value_;
  }
  bool IsParameter() const { return source_ == RegisterSource::kParameter; }
  bool IsClass() const { return source_ == RegisterSource::kClass; }
  bool IsString() const { return source_ == RegisterSource::kString; }
  bool IsConstant() const { return source_ == RegisterSource::kConstant; }

  std::string ToString() const {
    switch (source_) {
      case RegisterSource::kString: {
        const char* str = reference_.dex_file->StringDataByIdx(dex::StringIndex(reference_.index));
        if (type_ == VeriClass::class_) {
          // Class names at the Java level are of the form x.y.z, but the list encodes
          // them of the form Lx/y/z;. Inner classes have '$' for both Java level class
          // names in strings, and hidden API lists.
          return HiddenApi::ToInternalName(str);
        } else {
          return str;
        }
      }
      case RegisterSource::kClass:
        return reference_.dex_file->StringByTypeIdx(dex::TypeIndex(reference_.index));
      case RegisterSource::kParameter:
        return std::string("Parameter of ") + reference_.dex_file->PrettyMethod(reference_.index);
      default:
        return "<unknown>";
    }
  }

 private:
  RegisterSource source_;
  uint32_t value_;
  DexFileReference reference_;
  const VeriClass* type_;
};

struct InstructionInfo {
  bool has_been_visited;
};

class VeriFlowAnalysis {
 public:
  VeriFlowAnalysis(VeridexResolver* resolver, const ClassDataItemIterator& it)
      : resolver_(resolver),
        method_id_(it.GetMemberIndex()),
        code_item_accessor_(resolver->GetDexFile(), it.GetMethodCodeItem()),
        dex_registers_(code_item_accessor_.InsnsSizeInCodeUnits()),
        instruction_infos_(code_item_accessor_.InsnsSizeInCodeUnits()) {}

  void Run();

  virtual RegisterValue AnalyzeInvoke(const Instruction& instruction, bool is_range) = 0;
  virtual void AnalyzeFieldSet(const Instruction& instruction) = 0;
  virtual ~VeriFlowAnalysis() {}

 private:
  // Find all branches in the code.
  void FindBranches();

  // Analyze all non-deead instructions in the code.
  void AnalyzeCode();

  // Set the instruction at the given pc as a branch target.
  void SetAsBranchTarget(uint32_t dex_pc);

  // Whether the instruction at the given pc is a branch target.
  bool IsBranchTarget(uint32_t dex_pc);

  // Merge the register values at the given pc with `current_registers`.
  // Return whether the register values have changed, and the instruction needs
  // to be visited again.
  bool MergeRegisterValues(uint32_t dex_pc);

  void UpdateRegister(
      uint32_t dex_register, RegisterSource kind, VeriClass* cls, uint32_t source_id);
  void UpdateRegister(uint32_t dex_register, const RegisterValue& value);
  void UpdateRegister(uint32_t dex_register, const VeriClass* cls);
  void UpdateRegister(uint32_t dex_register, int32_t value, const VeriClass* cls);
  void ProcessDexInstruction(const Instruction& inst);
  void SetVisited(uint32_t dex_pc);
  RegisterValue GetFieldType(uint32_t field_index);

  int GetBranchFlags(const Instruction& instruction) const;

 protected:
  const RegisterValue& GetRegister(uint32_t dex_register) const;
  RegisterValue GetReturnType(uint32_t method_index);

  VeridexResolver* resolver_;

 private:
  const uint32_t method_id_;
  CodeItemDataAccessor code_item_accessor_;

  // Vector of register values for all branch targets.
  std::vector<std::unique_ptr<std::vector<RegisterValue>>> dex_registers_;

  // The current values of dex registers.
  std::vector<RegisterValue> current_registers_;

  // Information on each instruction useful for the analysis.
  std::vector<InstructionInfo> instruction_infos_;

  // The value of invoke instructions, to be fetched when visiting move-result.
  RegisterValue last_result_;
};

struct ReflectAccessInfo {
  RegisterValue cls;
  RegisterValue name;
  bool is_method;

  ReflectAccessInfo(RegisterValue c, RegisterValue n, bool m) : cls(c), name(n), is_method(m) {}

  bool IsConcrete() const {
    // We capture RegisterSource::kString for the class, for example in Class.forName.
    return (cls.IsClass() || cls.IsString()) && name.IsString();
  }
};

// Collects all reflection uses.
class FlowAnalysisCollector : public VeriFlowAnalysis {
 public:
  FlowAnalysisCollector(VeridexResolver* resolver, const ClassDataItemIterator& it)
      : VeriFlowAnalysis(resolver, it) {}

  const std::vector<ReflectAccessInfo>& GetUses() const {
    return uses_;
  }

  RegisterValue AnalyzeInvoke(const Instruction& instruction, bool is_range) OVERRIDE;
  void AnalyzeFieldSet(const Instruction& instruction) OVERRIDE;

 private:
  // List of reflection uses found, concrete and abstract.
  std::vector<ReflectAccessInfo> uses_;
};

// Substitutes reflection uses by new ones.
class FlowAnalysisSubstitutor : public VeriFlowAnalysis {
 public:
  FlowAnalysisSubstitutor(VeridexResolver* resolver,
                          const ClassDataItemIterator& it,
                          const std::map<MethodReference, std::vector<ReflectAccessInfo>>& accesses)
      : VeriFlowAnalysis(resolver, it), accesses_(accesses) {}

  const std::vector<ReflectAccessInfo>& GetUses() const {
    return uses_;
  }

  RegisterValue AnalyzeInvoke(const Instruction& instruction, bool is_range) OVERRIDE;
  void AnalyzeFieldSet(const Instruction& instruction) OVERRIDE;

 private:
  // List of reflection uses found, concrete and abstract.
  std::vector<ReflectAccessInfo> uses_;
  // The abstract uses we are trying to subsititute.
  const std::map<MethodReference, std::vector<ReflectAccessInfo>>& accesses_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_
