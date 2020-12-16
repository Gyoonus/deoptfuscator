// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "instruction_decoder.h"

#include "dex/dex_instruction_list.h"

#include <android-base/logging.h>

namespace titrace {

class ClassInstructionDecoder : public InstructionDecoder {
 public:
  size_t GetMaximumOpcode() override {
    return 0xff;
  }

  const char* GetName(size_t opcode) override {
    Bytecode::Opcode op = static_cast<Bytecode::Opcode>(opcode);
    return Bytecode::ToString(op);
  }

  virtual size_t LocationToOffset(size_t j_location) {
    return j_location;
  }

 private:
  class Bytecode {
   public:
    enum Opcode {
      // Java bytecode opcodes from 0x00 to 0xFF.
      kNop = 0x00,
      kAconst_null = 0x01,
      kIconst_m1 = 0x02,
      kIconst_0 = 0x03,
      kIconst_1 = 0x04,
      kIconst_2 = 0x05,
      kIconst_3 = 0x06,
      kIconst_4 = 0x07,
      kIconst_5 = 0x08,
      kLconst_0 = 0x09,
      kLconst_1 = 0x0a,
      kFconst_0 = 0x0b,
      kFconst_1 = 0x0c,
      kFconst_2 = 0x0d,
      kDconst_0 = 0x0e,
      kDconst_1 = 0x0f,
      kBipush = 0x10,
      kSipush = 0x11,
      kLdc = 0x12,
      kLdc_w = 0x13,
      kLdc2_w = 0x14,
      kIload = 0x15,
      kLload = 0x16,
      kFload = 0x17,
      kDload = 0x18,
      kAload = 0x19,
      kIload_0 = 0x1a,
      kIload_1 = 0x1b,
      kIload_2 = 0x1c,
      kIload_3 = 0x1d,
      kLload_0 = 0x1e,
      kLload_1 = 0x1f,
      kLload_2 = 0x20,
      kLload_3 = 0x21,
      kFload_0 = 0x22,
      kFload_1 = 0x23,
      kFload_2 = 0x24,
      kFload_3 = 0x25,
      kDload_0 = 0x26,
      kDload_1 = 0x27,
      kDload_2 = 0x28,
      kDload_3 = 0x29,
      kAload_0 = 0x2a,
      kAload_1 = 0x2b,
      kAload_2 = 0x2c,
      kAload_3 = 0x2d,
      kIaload = 0x2e,
      kLaload = 0x2f,
      kFaload = 0x30,
      kDaload = 0x31,
      kAaload = 0x32,
      kBaload = 0x33,
      kCaload = 0x34,
      kSaload = 0x35,
      kIstore = 0x36,
      kLstore = 0x37,
      kFstore = 0x38,
      kDstore = 0x39,
      kAstore = 0x3a,
      kIstore_0 = 0x3b,
      kIstore_1 = 0x3c,
      kIstore_2 = 0x3d,
      kIstore_3 = 0x3e,
      kLstore_0 = 0x3f,
      kLstore_1 = 0x40,
      kLstore_2 = 0x41,
      kLstore_3 = 0x42,
      kFstore_0 = 0x43,
      kFstore_1 = 0x44,
      kFstore_2 = 0x45,
      kFstore_3 = 0x46,
      kDstore_0 = 0x47,
      kDstore_1 = 0x48,
      kDstore_2 = 0x49,
      kDstore_3 = 0x4a,
      kAstore_0 = 0x4b,
      kAstore_1 = 0x4c,
      kAstore_2 = 0x4d,
      kAstore_3 = 0x4e,
      kIastore = 0x4f,
      kLastore = 0x50,
      kFastore = 0x51,
      kDastore = 0x52,
      kAastore = 0x53,
      kBastore = 0x54,
      kCastore = 0x55,
      kSastore = 0x56,
      kPop = 0x57,
      kPop2 = 0x58,
      kDup = 0x59,
      kDup_x1 = 0x5a,
      kDup_x2 = 0x5b,
      kDup2 = 0x5c,
      kDup2_x1 = 0x5d,
      kDup2_x2 = 0x5e,
      kSwap = 0x5f,
      kIadd = 0x60,
      kLadd = 0x61,
      kFadd = 0x62,
      kDadd = 0x63,
      kIsub = 0x64,
      kLsub = 0x65,
      kFsub = 0x66,
      kDsub = 0x67,
      kImul = 0x68,
      kLmul = 0x69,
      kFmul = 0x6a,
      kDmul = 0x6b,
      kIdiv = 0x6c,
      kLdiv = 0x6d,
      kFdiv = 0x6e,
      kDdiv = 0x6f,
      kIrem = 0x70,
      kLrem = 0x71,
      kFrem = 0x72,
      kDrem = 0x73,
      kIneg = 0x74,
      kLneg = 0x75,
      kFneg = 0x76,
      kDneg = 0x77,
      kIshl = 0x78,
      kLshl = 0x79,
      kIshr = 0x7a,
      kLshr = 0x7b,
      kIushr = 0x7c,
      kLushr = 0x7d,
      kIand = 0x7e,
      kLand = 0x7f,
      kIor = 0x80,
      kLor = 0x81,
      kIxor = 0x82,
      kLxor = 0x83,
      kIinc = 0x84,
      kI2l = 0x85,
      kI2f = 0x86,
      kI2d = 0x87,
      kL2i = 0x88,
      kL2f = 0x89,
      kL2d = 0x8a,
      kF2i = 0x8b,
      kF2l = 0x8c,
      kF2d = 0x8d,
      kD2i = 0x8e,
      kD2l = 0x8f,
      kD2f = 0x90,
      kI2b = 0x91,
      kI2c = 0x92,
      kI2s = 0x93,
      kLcmp = 0x94,
      kFcmpl = 0x95,
      kFcmpg = 0x96,
      kDcmpl = 0x97,
      kDcmpg = 0x98,
      kIfeq = 0x99,
      kIfne = 0x9a,
      kIflt = 0x9b,
      kIfge = 0x9c,
      kIfgt = 0x9d,
      kIfle = 0x9e,
      kIf_icmpeq = 0x9f,
      kIf_icmpne = 0xa0,
      kIf_icmplt = 0xa1,
      kIf_icmpge = 0xa2,
      kIf_icmpgt = 0xa3,
      kIf_icmple = 0xa4,
      kIf_acmpeq = 0xa5,
      kIf_acmpne = 0xa6,
      kGoto = 0xa7,
      kJsr = 0xa8,
      kRet = 0xa9,
      kTableswitch = 0xaa,
      kLookupswitch = 0xab,
      kIreturn = 0xac,
      kLreturn = 0xad,
      kFreturn = 0xae,
      kDreturn = 0xaf,
      kAreturn = 0xb0,
      kReturn = 0xb1,
      kGetstatic = 0xb2,
      kPutstatic = 0xb3,
      kGetfield = 0xb4,
      kPutfield = 0xb5,
      kInvokevirtual = 0xb6,
      kInvokespecial = 0xb7,
      kInvokestatic = 0xb8,
      kInvokeinterface = 0xb9,
      kInvokedynamic = 0xba,
      kNew = 0xbb,
      kNewarray = 0xbc,
      kAnewarray = 0xbd,
      kArraylength = 0xbe,
      kAthrow = 0xbf,
      kCheckcast = 0xc0,
      kInstanceof = 0xc1,
      kMonitorenter = 0xc2,
      kMonitorexit = 0xc3,
      kWide = 0xc4,
      kMultianewarray = 0xc5,
      kIfnull = 0xc6,
      kIfnonnull = 0xc7,
      kGoto_w = 0xc8,
      kJsr_w = 0xc9,
      kBreakpoint = 0xca,
      // Instructions 0xcb-0xfd are undefined.
          kImpdep1 = 0xfe,
      kImpdep2 = 0xff,
    };

    static const char* ToString(Bytecode::Opcode op) {
      switch (op) {
        case kNop: return "nop";
        case kAconst_null: return "aconst_null";
        case kIconst_m1: return "iconst_m1";
        case kIconst_0: return "iconst_0";
        case kIconst_1: return "iconst_1";
        case kIconst_2: return "iconst_2";
        case kIconst_3: return "iconst_3";
        case kIconst_4: return "iconst_4";
        case kIconst_5: return "iconst_5";
        case kLconst_0: return "lconst_0";
        case kLconst_1: return "lconst_1";
        case kFconst_0: return "fconst_0";
        case kFconst_1: return "fconst_1";
        case kFconst_2: return "fconst_2";
        case kDconst_0: return "dconst_0";
        case kDconst_1: return "dconst_1";
        case kBipush: return "bipush";
        case kSipush: return "sipush";
        case kLdc: return "ldc";
        case kLdc_w: return "ldc_w";
        case kLdc2_w: return "ldc2_w";
        case kIload: return "iload";
        case kLload: return "lload";
        case kFload: return "fload";
        case kDload: return "dload";
        case kAload: return "aload";
        case kIload_0: return "iload_0";
        case kIload_1: return "iload_1";
        case kIload_2: return "iload_2";
        case kIload_3: return "iload_3";
        case kLload_0: return "lload_0";
        case kLload_1: return "lload_1";
        case kLload_2: return "lload_2";
        case kLload_3: return "lload_3";
        case kFload_0: return "fload_0";
        case kFload_1: return "fload_1";
        case kFload_2: return "fload_2";
        case kFload_3: return "fload_3";
        case kDload_0: return "dload_0";
        case kDload_1: return "dload_1";
        case kDload_2: return "dload_2";
        case kDload_3: return "dload_3";
        case kAload_0: return "aload_0";
        case kAload_1: return "aload_1";
        case kAload_2: return "aload_2";
        case kAload_3: return "aload_3";
        case kIaload: return "iaload";
        case kLaload: return "laload";
        case kFaload: return "faload";
        case kDaload: return "daload";
        case kAaload: return "aaload";
        case kBaload: return "baload";
        case kCaload: return "caload";
        case kSaload: return "saload";
        case kIstore: return "istore";
        case kLstore: return "lstore";
        case kFstore: return "fstore";
        case kDstore: return "dstore";
        case kAstore: return "astore";
        case kIstore_0: return "istore_0";
        case kIstore_1: return "istore_1";
        case kIstore_2: return "istore_2";
        case kIstore_3: return "istore_3";
        case kLstore_0: return "lstore_0";
        case kLstore_1: return "lstore_1";
        case kLstore_2: return "lstore_2";
        case kLstore_3: return "lstore_3";
        case kFstore_0: return "fstore_0";
        case kFstore_1: return "fstore_1";
        case kFstore_2: return "fstore_2";
        case kFstore_3: return "fstore_3";
        case kDstore_0: return "dstore_0";
        case kDstore_1: return "dstore_1";
        case kDstore_2: return "dstore_2";
        case kDstore_3: return "dstore_3";
        case kAstore_0: return "astore_0";
        case kAstore_1: return "astore_1";
        case kAstore_2: return "astore_2";
        case kAstore_3: return "astore_3";
        case kIastore: return "iastore";
        case kLastore: return "lastore";
        case kFastore: return "fastore";
        case kDastore: return "dastore";
        case kAastore: return "aastore";
        case kBastore: return "bastore";
        case kCastore: return "castore";
        case kSastore: return "sastore";
        case kPop: return "pop";
        case kPop2: return "pop2";
        case kDup: return "dup";
        case kDup_x1: return "dup_x1";
        case kDup_x2: return "dup_x2";
        case kDup2: return "dup2";
        case kDup2_x1: return "dup2_x1";
        case kDup2_x2: return "dup2_x2";
        case kSwap: return "swap";
        case kIadd: return "iadd";
        case kLadd: return "ladd";
        case kFadd: return "fadd";
        case kDadd: return "dadd";
        case kIsub: return "isub";
        case kLsub: return "lsub";
        case kFsub: return "fsub";
        case kDsub: return "dsub";
        case kImul: return "imul";
        case kLmul: return "lmul";
        case kFmul: return "fmul";
        case kDmul: return "dmul";
        case kIdiv: return "idiv";
        case kLdiv: return "ldiv";
        case kFdiv: return "fdiv";
        case kDdiv: return "ddiv";
        case kIrem: return "irem";
        case kLrem: return "lrem";
        case kFrem: return "frem";
        case kDrem: return "drem";
        case kIneg: return "ineg";
        case kLneg: return "lneg";
        case kFneg: return "fneg";
        case kDneg: return "dneg";
        case kIshl: return "ishl";
        case kLshl: return "lshl";
        case kIshr: return "ishr";
        case kLshr: return "lshr";
        case kIushr: return "iushr";
        case kLushr: return "lushr";
        case kIand: return "iand";
        case kLand: return "land";
        case kIor: return "ior";
        case kLor: return "lor";
        case kIxor: return "ixor";
        case kLxor: return "lxor";
        case kIinc: return "iinc";
        case kI2l: return "i2l";
        case kI2f: return "i2f";
        case kI2d: return "i2d";
        case kL2i: return "l2i";
        case kL2f: return "l2f";
        case kL2d: return "l2d";
        case kF2i: return "f2i";
        case kF2l: return "f2l";
        case kF2d: return "f2d";
        case kD2i: return "d2i";
        case kD2l: return "d2l";
        case kD2f: return "d2f";
        case kI2b: return "i2b";
        case kI2c: return "i2c";
        case kI2s: return "i2s";
        case kLcmp: return "lcmp";
        case kFcmpl: return "fcmpl";
        case kFcmpg: return "fcmpg";
        case kDcmpl: return "dcmpl";
        case kDcmpg: return "dcmpg";
        case kIfeq: return "ifeq";
        case kIfne: return "ifne";
        case kIflt: return "iflt";
        case kIfge: return "ifge";
        case kIfgt: return "ifgt";
        case kIfle: return "ifle";
        case kIf_icmpeq: return "if_icmpeq";
        case kIf_icmpne: return "if_icmpne";
        case kIf_icmplt: return "if_icmplt";
        case kIf_icmpge: return "if_icmpge";
        case kIf_icmpgt: return "if_icmpgt";
        case kIf_icmple: return "if_icmple";
        case kIf_acmpeq: return "if_acmpeq";
        case kIf_acmpne: return "if_acmpne";
        case kGoto: return "goto";
        case kJsr: return "jsr";
        case kRet: return "ret";
        case kTableswitch: return "tableswitch";
        case kLookupswitch: return "lookupswitch";
        case kIreturn: return "ireturn";
        case kLreturn: return "lreturn";
        case kFreturn: return "freturn";
        case kDreturn: return "dreturn";
        case kAreturn: return "areturn";
        case kReturn: return "return";
        case kGetstatic: return "getstatic";
        case kPutstatic: return "putstatic";
        case kGetfield: return "getfield";
        case kPutfield: return "putfield";
        case kInvokevirtual: return "invokevirtual";
        case kInvokespecial: return "invokespecial";
        case kInvokestatic: return "invokestatic";
        case kInvokeinterface: return "invokeinterface";
        case kInvokedynamic: return "invokedynamic";
        case kNew: return "new";
        case kNewarray: return "newarray";
        case kAnewarray: return "anewarray";
        case kArraylength: return "arraylength";
        case kAthrow: return "athrow";
        case kCheckcast: return "checkcast";
        case kInstanceof: return "instanceof";
        case kMonitorenter: return "monitorenter";
        case kMonitorexit: return "monitorexit";
        case kWide: return "wide";
        case kMultianewarray: return "multianewarray";
        case kIfnull: return "ifnull";
        case kIfnonnull: return "ifnonnull";
        case kGoto_w: return "goto_w";
        case kJsr_w: return "jsr_w";
        case kBreakpoint: return "breakpoint";
        case kImpdep1: return "impdep1";
        case kImpdep2: return "impdep2";
        default: LOG(FATAL) << "Unknown opcode " << op;
       }
       return "";
     }
  };
};

class DexInstructionDecoder : public InstructionDecoder {
 public:
  size_t GetMaximumOpcode() override {
    return 0xff;
  }

  const char* GetName(size_t opcode) override {
    Bytecode::Opcode op = static_cast<Bytecode::Opcode>(opcode);
    return Bytecode::ToString(op);
  }

  virtual size_t LocationToOffset(size_t j_location) {
    // dex pc is uint16_t*, but offset needs to be in bytes.
    return j_location * (sizeof(uint16_t) / sizeof(uint8_t));
  }

 private:
  class Bytecode {
   public:
    enum Opcode {
#define MAKE_ENUM_DEFINITION(opcode, instruction_code, name, format, index, flags, extended_flags, verifier_flags) \
      instruction_code = opcode,
DEX_INSTRUCTION_LIST(MAKE_ENUM_DEFINITION)
#undef MAKE_ENUM_DEFINITION
    };

    static_assert(static_cast<uint32_t>(Bytecode::Opcode::NOP) == 0, "");
    static_assert(static_cast<uint32_t>(Bytecode::Opcode::MOVE) == 1, "");

    static const char* ToString(Bytecode::Opcode op) {
      switch (op) {
#define MAKE_ENUM_DEFINITION(opcode, instruction_code, name, format, index, flags, extended_flags, verifier_flags) \
        case instruction_code: return (name);
DEX_INSTRUCTION_LIST(MAKE_ENUM_DEFINITION)
#undef MAKE_ENUM_DEFINITION
        default: LOG(FATAL) << "Unknown opcode " << op;
      }
      return "";
    }
  };
};

InstructionDecoder* InstructionDecoder::NewInstance(InstructionFileFormat file_format) {
  switch (file_format) {
    case InstructionFileFormat::kClass:
      return new ClassInstructionDecoder();
    case InstructionFileFormat::kDex:
      return new DexInstructionDecoder();
    default:
      return nullptr;
  }
}
}  // namespace titrace
