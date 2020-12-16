/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "inline_method_analyser.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_instruction.h"
#include "dex/dex_instruction_utils.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"

/*
 * NOTE: This code is part of the quick compiler. It lives in the runtime
 * only to allow the debugger to check whether a method has been inlined.
 */

namespace art {

namespace {  // anonymous namespace

// Helper class for matching a pattern.
class Matcher {
 public:
  // Match function type.
  typedef bool MatchFn(Matcher* matcher);

  template <size_t size>
  static bool Match(const CodeItemDataAccessor* code_item, MatchFn* const (&pattern)[size]);

  // Match and advance.

  static bool Mark(Matcher* matcher);

  template <bool (Matcher::*Fn)()>
  static bool Required(Matcher* matcher);

  template <bool (Matcher::*Fn)()>
  static bool Repeated(Matcher* matcher);  // On match, returns to the mark.

  // Match an individual instruction.

  template <Instruction::Code opcode> bool Opcode();
  bool Const0();
  bool IPutOnThis();

 private:
  explicit Matcher(const CodeItemDataAccessor* code_item)
      : code_item_(code_item),
        instruction_(code_item->begin()) {}

  static bool DoMatch(const CodeItemDataAccessor* code_item, MatchFn* const* pattern, size_t size);

  const CodeItemDataAccessor* const code_item_;
  DexInstructionIterator instruction_;
  size_t pos_ = 0u;
  size_t mark_ = 0u;
};

template <size_t size>
bool Matcher::Match(const CodeItemDataAccessor* code_item, MatchFn* const (&pattern)[size]) {
  return DoMatch(code_item, pattern, size);
}

bool Matcher::Mark(Matcher* matcher) {
  matcher->pos_ += 1u;  // Advance to the next match function before marking.
  matcher->mark_ = matcher->pos_;
  return true;
}

template <bool (Matcher::*Fn)()>
bool Matcher::Required(Matcher* matcher) {
  if (!(matcher->*Fn)()) {
    return false;
  }
  matcher->pos_ += 1u;
  ++matcher->instruction_;
  return true;
}

template <bool (Matcher::*Fn)()>
bool Matcher::Repeated(Matcher* matcher) {
  if (!(matcher->*Fn)()) {
    // Didn't match optional instruction, try the next match function.
    matcher->pos_ += 1u;
    return true;
  }
  matcher->pos_ = matcher->mark_;
  ++matcher->instruction_;
  return true;
}

template <Instruction::Code opcode>
bool Matcher::Opcode() {
  return instruction_->Opcode() == opcode;
}

// Match const 0.
bool Matcher::Const0() {
  return IsInstructionDirectConst(instruction_->Opcode()) &&
      (instruction_->Opcode() == Instruction::CONST_WIDE ? instruction_->VRegB_51l() == 0
                                                         : instruction_->VRegB() == 0);
}

bool Matcher::IPutOnThis() {
  DCHECK_NE(code_item_->InsSize(), 0u);
  return IsInstructionIPut(instruction_->Opcode()) &&
      instruction_->VRegB_22c() == code_item_->RegistersSize() - code_item_->InsSize();
}

bool Matcher::DoMatch(const CodeItemDataAccessor* code_item, MatchFn* const* pattern, size_t size) {
  Matcher matcher(code_item);
  while (matcher.pos_ != size) {
    if (!pattern[matcher.pos_](&matcher)) {
      return false;
    }
  }
  return true;
}

// Used for a single invoke in a constructor. In that situation, the method verifier makes
// sure we invoke a constructor either in the same class or superclass with at least "this".
ArtMethod* GetTargetConstructor(ArtMethod* method, const Instruction* invoke_direct)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(invoke_direct->Opcode(), Instruction::INVOKE_DIRECT);
  if (kIsDebugBuild) {
    CodeItemDataAccessor accessor(method->DexInstructionData());
    DCHECK_EQ(invoke_direct->VRegC_35c(),
              accessor.RegistersSize() - accessor.InsSize());
  }
  uint32_t method_index = invoke_direct->VRegB_35c();
  ArtMethod* target_method = Runtime::Current()->GetClassLinker()->LookupResolvedMethod(
      method_index, method->GetDexCache(), method->GetClassLoader());
  if (kIsDebugBuild && target_method != nullptr) {
    CHECK(!target_method->IsStatic());
    CHECK(target_method->IsConstructor());
    CHECK(target_method->GetDeclaringClass() == method->GetDeclaringClass() ||
          target_method->GetDeclaringClass() == method->GetDeclaringClass()->GetSuperClass());
  }
  return target_method;
}

// Return the forwarded arguments and check that all remaining arguments are zero.
// If the check fails, return static_cast<size_t>(-1).
size_t CountForwardedConstructorArguments(const CodeItemDataAccessor* code_item,
                                          const Instruction* invoke_direct,
                                          uint16_t zero_vreg_mask) {
  DCHECK_EQ(invoke_direct->Opcode(), Instruction::INVOKE_DIRECT);
  size_t number_of_args = invoke_direct->VRegA_35c();
  DCHECK_NE(number_of_args, 0u);
  uint32_t args[Instruction::kMaxVarArgRegs];
  invoke_direct->GetVarArgs(args);
  uint16_t this_vreg = args[0];
  DCHECK_EQ(this_vreg, code_item->RegistersSize() - code_item->InsSize());  // Checked by verifier.
  size_t forwarded = 1u;
  while (forwarded < number_of_args &&
      args[forwarded] == this_vreg + forwarded &&
      (zero_vreg_mask & (1u << args[forwarded])) == 0) {
    ++forwarded;
  }
  for (size_t i = forwarded; i != number_of_args; ++i) {
    if ((zero_vreg_mask & (1u << args[i])) == 0) {
      return static_cast<size_t>(-1);
    }
  }
  return forwarded;
}

uint16_t GetZeroVRegMask(const Instruction* const0) {
  DCHECK(IsInstructionDirectConst(const0->Opcode()));
  DCHECK((const0->Opcode() == Instruction::CONST_WIDE) ? const0->VRegB_51l() == 0u
                                                       : const0->VRegB() == 0);
  uint16_t base_mask = IsInstructionConstWide(const0->Opcode()) ? 3u : 1u;
  return base_mask << const0->VRegA();
}

// We limit the number of IPUTs storing parameters. There can be any number
// of IPUTs that store the value 0 as they are useless in a constructor as
// the object always starts zero-initialized. We also eliminate all but the
// last store to any field as they are not observable; not even if the field
// is volatile as no reference to the object can escape from a constructor
// with this pattern.
static constexpr size_t kMaxConstructorIPuts = 3u;

struct ConstructorIPutData {
  ConstructorIPutData() : field_index(DexFile::kDexNoIndex16), arg(0u) { }

  uint16_t field_index;
  uint16_t arg;
};

bool RecordConstructorIPut(ArtMethod* method,
                           const Instruction* new_iput,
                           uint16_t this_vreg,
                           uint16_t zero_vreg_mask,
                           /*inout*/ ConstructorIPutData (&iputs)[kMaxConstructorIPuts])
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(IsInstructionIPut(new_iput->Opcode()));
  uint32_t field_index = new_iput->VRegC_22c();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ArtField* field = class_linker->LookupResolvedField(field_index, method, /* is_static */ false);
  if (UNLIKELY(field == nullptr)) {
    return false;
  }
  // Remove previous IPUT to the same field, if any. Different field indexes may refer
  // to the same field, so we need to compare resolved fields from the dex cache.
  for (size_t old_pos = 0; old_pos != arraysize(iputs); ++old_pos) {
    if (iputs[old_pos].field_index == DexFile::kDexNoIndex16) {
      break;
    }
    ArtField* f = class_linker->LookupResolvedField(iputs[old_pos].field_index,
                                                    method,
                                                    /* is_static */ false);
    DCHECK(f != nullptr);
    if (f == field) {
      auto back_it = std::copy(iputs + old_pos + 1, iputs + arraysize(iputs), iputs + old_pos);
      *back_it = ConstructorIPutData();
      break;
    }
  }
  // If the stored value isn't zero, record the IPUT.
  if ((zero_vreg_mask & (1u << new_iput->VRegA_22c())) == 0u) {
    size_t new_pos = 0;
    while (new_pos != arraysize(iputs) && iputs[new_pos].field_index != DexFile::kDexNoIndex16) {
      ++new_pos;
    }
    if (new_pos == arraysize(iputs)) {
      return false;  // Exceeded capacity of the output array.
    }
    iputs[new_pos].field_index = field_index;
    iputs[new_pos].arg = new_iput->VRegA_22c() - this_vreg;
  }
  return true;
}

bool DoAnalyseConstructor(const CodeItemDataAccessor* code_item,
                          ArtMethod* method,
                          /*inout*/ ConstructorIPutData (&iputs)[kMaxConstructorIPuts])
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // On entry we should not have any IPUTs yet.
  DCHECK_EQ(0, std::count_if(
      iputs,
      iputs + arraysize(iputs),
      [](const ConstructorIPutData& iput_data) {
        return iput_data.field_index != DexFile::kDexNoIndex16;
      }));

  // Limit the maximum number of code units we're willing to match.
  static constexpr size_t kMaxCodeUnits = 16u;

  // Limit the number of registers that the constructor may use to 16.
  // Given that IPUTs must use low 16 registers and we do not match MOVEs,
  // this is a reasonable limitation.
  static constexpr size_t kMaxVRegs = 16u;

  // We try to match a constructor that calls another constructor (either in
  // superclass or in the same class) with the same parameters, or with some
  // parameters truncated (allowed only for calls to superclass constructor)
  // or with extra parameters with value 0 (with any type, including null).
  // This call can be followed by optional IPUTs on "this" storing either one
  // of the parameters or 0 and the code must then finish with RETURN_VOID.
  // The called constructor must be either java.lang.Object.<init>() or it
  // must also match the same pattern.
  static Matcher::MatchFn* const kConstructorPattern[] = {
      &Matcher::Mark,
      &Matcher::Repeated<&Matcher::Const0>,
      &Matcher::Required<&Matcher::Opcode<Instruction::INVOKE_DIRECT>>,
      &Matcher::Mark,
      &Matcher::Repeated<&Matcher::Const0>,
      &Matcher::Repeated<&Matcher::IPutOnThis>,
      &Matcher::Required<&Matcher::Opcode<Instruction::RETURN_VOID>>,
  };

  DCHECK(method != nullptr);
  DCHECK(!method->IsStatic());
  DCHECK(method->IsConstructor());
  DCHECK(code_item != nullptr);
  if (!method->GetDeclaringClass()->IsVerified() ||
      code_item->InsnsSizeInCodeUnits() > kMaxCodeUnits ||
      code_item->RegistersSize() > kMaxVRegs ||
      !Matcher::Match(code_item, kConstructorPattern)) {
    return false;
  }

  // Verify the invoke, prevent a few odd cases and collect IPUTs.
  uint16_t this_vreg = code_item->RegistersSize() - code_item->InsSize();
  uint16_t zero_vreg_mask = 0u;

  for (const DexInstructionPcPair& pair : *code_item) {
    const Instruction& instruction = pair.Inst();
    if (instruction.Opcode() == Instruction::RETURN_VOID) {
      break;
    } else if (instruction.Opcode() == Instruction::INVOKE_DIRECT) {
      ArtMethod* target_method = GetTargetConstructor(method, &instruction);
      if (target_method == nullptr) {
        return false;
      }
      // We allow forwarding constructors only if they pass more arguments
      // to prevent infinite recursion.
      if (target_method->GetDeclaringClass() == method->GetDeclaringClass() &&
          instruction.VRegA_35c() <= code_item->InsSize()) {
        return false;
      }
      size_t forwarded = CountForwardedConstructorArguments(code_item, &instruction, zero_vreg_mask);
      if (forwarded == static_cast<size_t>(-1)) {
        return false;
      }
      if (target_method->GetDeclaringClass()->IsObjectClass()) {
        DCHECK_EQ(target_method->DexInstructionData().begin()->Opcode(), Instruction::RETURN_VOID);
      } else {
        CodeItemDataAccessor target_code_item(target_method->DexInstructionData());
        if (!target_code_item.HasCodeItem()) {
          return false;  // Native constructor?
        }
        if (!DoAnalyseConstructor(&target_code_item, target_method, iputs)) {
          return false;
        }
        // Prune IPUTs with zero input.
        auto kept_end = std::remove_if(
            iputs,
            iputs + arraysize(iputs),
            [forwarded](const ConstructorIPutData& iput_data) {
              return iput_data.arg >= forwarded;
            });
        std::fill(kept_end, iputs + arraysize(iputs), ConstructorIPutData());
        // If we have any IPUTs from the call, check that the target method is in the same
        // dex file (compare DexCache references), otherwise field_indexes would be bogus.
        if (iputs[0].field_index != DexFile::kDexNoIndex16 &&
            target_method->GetDexCache() != method->GetDexCache()) {
          return false;
        }
      }
    } else if (IsInstructionDirectConst(instruction.Opcode())) {
      zero_vreg_mask |= GetZeroVRegMask(&instruction);
      if ((zero_vreg_mask & (1u << this_vreg)) != 0u) {
        return false;  // Overwriting `this` is unsupported.
      }
    } else {
      DCHECK(IsInstructionIPut(instruction.Opcode()));
      DCHECK_EQ(instruction.VRegB_22c(), this_vreg);
      if (!RecordConstructorIPut(method, &instruction, this_vreg, zero_vreg_mask, iputs)) {
        return false;
      }
    }
  }
  return true;
}

}  // anonymous namespace

bool AnalyseConstructor(const CodeItemDataAccessor* code_item,
                        ArtMethod* method,
                        InlineMethod* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ConstructorIPutData iputs[kMaxConstructorIPuts];
  if (!DoAnalyseConstructor(code_item, method, iputs)) {
    return false;
  }
  static_assert(kMaxConstructorIPuts == 3, "Unexpected limit");  // Code below depends on this.
  DCHECK(iputs[0].field_index != DexFile::kDexNoIndex16 ||
         iputs[1].field_index == DexFile::kDexNoIndex16);
  DCHECK(iputs[1].field_index != DexFile::kDexNoIndex16 ||
         iputs[2].field_index == DexFile::kDexNoIndex16);

#define STORE_IPUT(n)                                                         \
  do {                                                                        \
    result->d.constructor_data.iput##n##_field_index = iputs[n].field_index;  \
    result->d.constructor_data.iput##n##_arg = iputs[n].arg;                  \
  } while (false)

  STORE_IPUT(0);
  STORE_IPUT(1);
  STORE_IPUT(2);
#undef STORE_IPUT

  result->opcode = kInlineOpConstructor;
  result->d.constructor_data.reserved = 0u;
  return true;
}

static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET), "iget type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_WIDE), "iget_wide type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_OBJECT),
              "iget_object type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_BOOLEAN),
              "iget_boolean type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_BYTE), "iget_byte type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_CHAR), "iget_char type");
static_assert(InlineMethodAnalyser::IsInstructionIGet(Instruction::IGET_SHORT), "iget_short type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT), "iput type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_WIDE), "iput_wide type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_OBJECT),
              "iput_object type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_BOOLEAN),
              "iput_boolean type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_BYTE), "iput_byte type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_CHAR), "iput_char type");
static_assert(InlineMethodAnalyser::IsInstructionIPut(Instruction::IPUT_SHORT), "iput_short type");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT), "iget/iput variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_WIDE) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_WIDE), "iget/iput_wide variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_OBJECT) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_OBJECT), "iget/iput_object variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_BOOLEAN) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_BOOLEAN), "iget/iput_boolean variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_BYTE) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_BYTE), "iget/iput_byte variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_CHAR) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_CHAR), "iget/iput_char variant");
static_assert(InlineMethodAnalyser::IGetVariant(Instruction::IGET_SHORT) ==
    InlineMethodAnalyser::IPutVariant(Instruction::IPUT_SHORT), "iget/iput_short variant");

bool InlineMethodAnalyser::AnalyseMethodCode(ArtMethod* method, InlineMethod* result) {
  CodeItemDataAccessor code_item(method->DexInstructionData());
  if (!code_item.HasCodeItem()) {
    // Native or abstract.
    return false;
  }
  return AnalyseMethodCode(&code_item,
                           MethodReference(method->GetDexFile(), method->GetDexMethodIndex()),
                           method->IsStatic(),
                           method,
                           result);
}

bool InlineMethodAnalyser::AnalyseMethodCode(const CodeItemDataAccessor* code_item,
                                             const MethodReference& method_ref,
                                             bool is_static,
                                             ArtMethod* method,
                                             InlineMethod* result) {
  // We currently support only plain return or 2-instruction methods.

  DCHECK_NE(code_item->InsnsSizeInCodeUnits(), 0u);
  Instruction::Code opcode = code_item->begin()->Opcode();

  switch (opcode) {
    case Instruction::RETURN_VOID:
      if (result != nullptr) {
        result->opcode = kInlineOpNop;
        result->d.data = 0u;
      }
      return true;
    case Instruction::RETURN:
    case Instruction::RETURN_OBJECT:
    case Instruction::RETURN_WIDE:
      return AnalyseReturnMethod(code_item, result);
    case Instruction::CONST:
    case Instruction::CONST_4:
    case Instruction::CONST_16:
    case Instruction::CONST_HIGH16:
      // TODO: Support wide constants (RETURN_WIDE).
      if (AnalyseConstMethod(code_item, result)) {
        return true;
      }
      FALLTHROUGH_INTENDED;
    case Instruction::CONST_WIDE:
    case Instruction::CONST_WIDE_16:
    case Instruction::CONST_WIDE_32:
    case Instruction::CONST_WIDE_HIGH16:
    case Instruction::INVOKE_DIRECT:
      if (method != nullptr && !method->IsStatic() && method->IsConstructor()) {
        return AnalyseConstructor(code_item, method, result);
      }
      return false;
    case Instruction::IGET:
    case Instruction::IGET_OBJECT:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT:
    case Instruction::IGET_WIDE:
    // TODO: Add handling for JIT.
    // case Instruction::IGET_QUICK:
    // case Instruction::IGET_WIDE_QUICK:
    // case Instruction::IGET_OBJECT_QUICK:
      return AnalyseIGetMethod(code_item, method_ref, is_static, method, result);
    case Instruction::IPUT:
    case Instruction::IPUT_OBJECT:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT:
    case Instruction::IPUT_WIDE:
      // TODO: Add handling for JIT.
    // case Instruction::IPUT_QUICK:
    // case Instruction::IPUT_WIDE_QUICK:
    // case Instruction::IPUT_OBJECT_QUICK:
      return AnalyseIPutMethod(code_item, method_ref, is_static, method, result);
    default:
      return false;
  }
}

bool InlineMethodAnalyser::IsSyntheticAccessor(MethodReference ref) {
  const DexFile::MethodId& method_id = ref.dex_file->GetMethodId(ref.index);
  const char* method_name = ref.dex_file->GetMethodName(method_id);
  // javac names synthetic accessors "access$nnn",
  // jack names them "-getN", "-putN", "-wrapN".
  return strncmp(method_name, "access$", strlen("access$")) == 0 ||
      strncmp(method_name, "-", strlen("-")) == 0;
}

bool InlineMethodAnalyser::AnalyseReturnMethod(const CodeItemDataAccessor* code_item,
                                               InlineMethod* result) {
  DexInstructionIterator return_instruction = code_item->begin();
  Instruction::Code return_opcode = return_instruction->Opcode();
  uint32_t reg = return_instruction->VRegA_11x();
  uint32_t arg_start = code_item->RegistersSize() - code_item->InsSize();
  DCHECK_GE(reg, arg_start);
  DCHECK_LT((return_opcode == Instruction::RETURN_WIDE) ? reg + 1 : reg,
      code_item->RegistersSize());

  if (result != nullptr) {
    result->opcode = kInlineOpReturnArg;
    InlineReturnArgData* data = &result->d.return_data;
    data->arg = reg - arg_start;
    data->is_wide = (return_opcode == Instruction::RETURN_WIDE) ? 1u : 0u;
    data->is_object = (return_opcode == Instruction::RETURN_OBJECT) ? 1u : 0u;
    data->reserved = 0u;
    data->reserved2 = 0u;
  }
  return true;
}

bool InlineMethodAnalyser::AnalyseConstMethod(const CodeItemDataAccessor* code_item,
                                              InlineMethod* result) {
  DexInstructionIterator instruction = code_item->begin();
  const Instruction* return_instruction = instruction->Next();
  Instruction::Code return_opcode = return_instruction->Opcode();
  if (return_opcode != Instruction::RETURN &&
      return_opcode != Instruction::RETURN_OBJECT) {
    return false;
  }

  int32_t return_reg = return_instruction->VRegA_11x();
  DCHECK_LT(return_reg, code_item->RegistersSize());

  int32_t const_value = instruction->VRegB();
  if (instruction->Opcode() == Instruction::CONST_HIGH16) {
    const_value <<= 16;
  }
  DCHECK_LT(instruction->VRegA(), code_item->RegistersSize());
  if (instruction->VRegA() != return_reg) {
    return false;  // Not returning the value set by const?
  }
  if (return_opcode == Instruction::RETURN_OBJECT && const_value != 0) {
    return false;  // Returning non-null reference constant?
  }
  if (result != nullptr) {
    result->opcode = kInlineOpNonWideConst;
    result->d.data = static_cast<uint64_t>(const_value);
  }
  return true;
}

bool InlineMethodAnalyser::AnalyseIGetMethod(const CodeItemDataAccessor* code_item,
                                             const MethodReference& method_ref,
                                             bool is_static,
                                             ArtMethod* method,
                                             InlineMethod* result) {
  DexInstructionIterator instruction = code_item->begin();
  Instruction::Code opcode = instruction->Opcode();
  DCHECK(IsInstructionIGet(opcode));

  const Instruction* return_instruction = instruction->Next();
  Instruction::Code return_opcode = return_instruction->Opcode();
  if (!(return_opcode == Instruction::RETURN_WIDE && opcode == Instruction::IGET_WIDE) &&
      !(return_opcode == Instruction::RETURN_OBJECT && opcode == Instruction::IGET_OBJECT) &&
      !(return_opcode == Instruction::RETURN && opcode != Instruction::IGET_WIDE &&
          opcode != Instruction::IGET_OBJECT)) {
    return false;
  }

  uint32_t return_reg = return_instruction->VRegA_11x();
  DCHECK_LT(return_opcode == Instruction::RETURN_WIDE ? return_reg + 1 : return_reg,
            code_item->RegistersSize());

  uint32_t dst_reg = instruction->VRegA_22c();
  uint32_t object_reg = instruction->VRegB_22c();
  uint32_t field_idx = instruction->VRegC_22c();
  uint32_t arg_start = code_item->RegistersSize() - code_item->InsSize();
  DCHECK_GE(object_reg, arg_start);
  DCHECK_LT(object_reg, code_item->RegistersSize());
  uint32_t object_arg = object_reg - arg_start;

  DCHECK_LT(opcode == Instruction::IGET_WIDE ? dst_reg + 1 : dst_reg, code_item->RegistersSize());
  if (dst_reg != return_reg) {
    return false;  // Not returning the value retrieved by IGET?
  }

  if (is_static || object_arg != 0u) {
    // TODO: Implement inlining of IGET on non-"this" registers (needs correct stack trace for NPE).
    // Allow synthetic accessors. We don't care about losing their stack frame in NPE.
    if (!IsSyntheticAccessor(method_ref)) {
      return false;
    }
  }

  // InlineIGetIPutData::object_arg is only 4 bits wide.
  static constexpr uint16_t kMaxObjectArg = 15u;
  if (object_arg > kMaxObjectArg) {
    return false;
  }

  if (result != nullptr) {
    InlineIGetIPutData* data = &result->d.ifield_data;
    if (!ComputeSpecialAccessorInfo(method, field_idx, false, data)) {
      return false;
    }
    result->opcode = kInlineOpIGet;
    data->op_variant = IGetVariant(opcode);
    data->method_is_static = is_static ? 1u : 0u;
    data->object_arg = object_arg;  // Allow IGET on any register, not just "this".
    data->src_arg = 0u;
    data->return_arg_plus1 = 0u;
  }
  return true;
}

bool InlineMethodAnalyser::AnalyseIPutMethod(const CodeItemDataAccessor* code_item,
                                             const MethodReference& method_ref,
                                             bool is_static,
                                             ArtMethod* method,
                                             InlineMethod* result) {
  DexInstructionIterator instruction = code_item->begin();
  Instruction::Code opcode = instruction->Opcode();
  DCHECK(IsInstructionIPut(opcode));

  const Instruction* return_instruction = instruction->Next();
  Instruction::Code return_opcode = return_instruction->Opcode();
  uint32_t arg_start = code_item->RegistersSize() - code_item->InsSize();
  uint16_t return_arg_plus1 = 0u;
  if (return_opcode != Instruction::RETURN_VOID) {
    if (return_opcode != Instruction::RETURN &&
        return_opcode != Instruction::RETURN_OBJECT &&
        return_opcode != Instruction::RETURN_WIDE) {
      return false;
    }
    // Returning an argument.
    uint32_t return_reg = return_instruction->VRegA_11x();
    DCHECK_GE(return_reg, arg_start);
    DCHECK_LT(return_opcode == Instruction::RETURN_WIDE ? return_reg + 1u : return_reg,
              code_item->RegistersSize());
    return_arg_plus1 = return_reg - arg_start + 1u;
  }

  uint32_t src_reg = instruction->VRegA_22c();
  uint32_t object_reg = instruction->VRegB_22c();
  uint32_t field_idx = instruction->VRegC_22c();
  DCHECK_GE(object_reg, arg_start);
  DCHECK_LT(object_reg, code_item->RegistersSize());
  DCHECK_GE(src_reg, arg_start);
  DCHECK_LT(opcode == Instruction::IPUT_WIDE ? src_reg + 1 : src_reg, code_item->RegistersSize());
  uint32_t object_arg = object_reg - arg_start;
  uint32_t src_arg = src_reg - arg_start;

  if (is_static || object_arg != 0u) {
    // TODO: Implement inlining of IPUT on non-"this" registers (needs correct stack trace for NPE).
    // Allow synthetic accessors. We don't care about losing their stack frame in NPE.
    if (!IsSyntheticAccessor(method_ref)) {
      return false;
    }
  }

  // InlineIGetIPutData::object_arg/src_arg/return_arg_plus1 are each only 4 bits wide.
  static constexpr uint16_t kMaxObjectArg = 15u;
  static constexpr uint16_t kMaxSrcArg = 15u;
  static constexpr uint16_t kMaxReturnArgPlus1 = 15u;
  if (object_arg > kMaxObjectArg || src_arg > kMaxSrcArg || return_arg_plus1 > kMaxReturnArgPlus1) {
    return false;
  }

  if (result != nullptr) {
    InlineIGetIPutData* data = &result->d.ifield_data;
    if (!ComputeSpecialAccessorInfo(method, field_idx, true, data)) {
      return false;
    }
    result->opcode = kInlineOpIPut;
    data->op_variant = IPutVariant(opcode);
    data->method_is_static = is_static ? 1u : 0u;
    data->object_arg = object_arg;  // Allow IPUT on any register, not just "this".
    data->src_arg = src_arg;
    data->return_arg_plus1 = return_arg_plus1;
  }
  return true;
}

bool InlineMethodAnalyser::ComputeSpecialAccessorInfo(ArtMethod* method,
                                                      uint32_t field_idx,
                                                      bool is_put,
                                                      InlineIGetIPutData* result) {
  if (method == nullptr) {
    return false;
  }
  ObjPtr<mirror::DexCache> dex_cache = method->GetDexCache();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ArtField* field = class_linker->LookupResolvedField(field_idx, method, /* is_static */ false);
  if (field == nullptr || field->IsStatic()) {
    return false;
  }
  ObjPtr<mirror::Class> method_class = method->GetDeclaringClass();
  ObjPtr<mirror::Class> field_class = field->GetDeclaringClass();
  if (!method_class->CanAccessResolvedField(field_class, field, dex_cache, field_idx) ||
      (is_put && field->IsFinal() && method_class != field_class)) {
    return false;
  }
  DCHECK_GE(field->GetOffset().Int32Value(), 0);
  // Do not interleave function calls with bit field writes to placate valgrind. Bug: 27552451.
  uint32_t field_offset = field->GetOffset().Uint32Value();
  bool is_volatile = field->IsVolatile();
  result->field_idx = field_idx;
  result->field_offset = field_offset;
  result->is_volatile = is_volatile ? 1u : 0u;
  return true;
}

}  // namespace art
