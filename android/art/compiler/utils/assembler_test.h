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

#ifndef ART_COMPILER_UTILS_ASSEMBLER_TEST_H_
#define ART_COMPILER_UTILS_ASSEMBLER_TEST_H_

#include "assembler.h"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include "assembler_test_base.h"
#include "common_runtime_test.h"  // For ScratchFile

namespace art {

// Helper for a constexpr string length.
constexpr size_t ConstexprStrLen(char const* str, size_t count = 0) {
  return ('\0' == str[0]) ? count : ConstexprStrLen(str+1, count+1);
}

enum class RegisterView {  // private
  kUsePrimaryName,
  kUseSecondaryName,
  kUseTertiaryName,
  kUseQuaternaryName,
};

// For use in the template as the default type to get a nonvector registers version.
struct NoVectorRegs {};

template<typename Ass,
         typename Addr,
         typename Reg,
         typename FPReg,
         typename Imm,
         typename VecReg = NoVectorRegs>
class AssemblerTest : public testing::Test {
 public:
  Ass* GetAssembler() {
    return assembler_.get();
  }

  typedef std::string (*TestFn)(AssemblerTest* assembler_test, Ass* assembler);

  void DriverFn(TestFn f, const std::string& test_name) {
    DriverWrapper(f(this, assembler_.get()), test_name);
  }

  // This driver assumes the assembler has already been called.
  void DriverStr(const std::string& assembly_string, const std::string& test_name) {
    DriverWrapper(assembly_string, test_name);
  }

  //
  // Register repeats.
  //

  std::string RepeatR(void (Ass::*f)(Reg), const std::string& fmt) {
    return RepeatTemplatedRegister<Reg>(f,
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string Repeatr(void (Ass::*f)(Reg), const std::string& fmt) {
    return RepeatTemplatedRegister<Reg>(f,
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        fmt);
  }

  std::string RepeatRR(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string RepeatRRNoDupes(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegistersNoDupes<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string Repeatrr(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        fmt);
  }

  std::string Repeatww(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseTertiaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUseTertiaryName>,
        fmt);
  }

  std::string Repeatbb(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseQuaternaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUseQuaternaryName>,
        fmt);
  }

  std::string RepeatRRR(void (Ass::*f)(Reg, Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string Repeatrb(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUseQuaternaryName>,
        fmt);
  }

  std::string RepeatRr(void (Ass::*f)(Reg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, Reg>(f,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        fmt);
  }

  std::string RepeatRI(void (Ass::*f)(Reg, const Imm&), size_t imm_bytes, const std::string& fmt) {
    return RepeatRegisterImm<RegisterView::kUsePrimaryName>(f, imm_bytes, fmt);
  }

  std::string RepeatrI(void (Ass::*f)(Reg, const Imm&), size_t imm_bytes, const std::string& fmt) {
    return RepeatRegisterImm<RegisterView::kUseSecondaryName>(f, imm_bytes, fmt);
  }

  std::string RepeatwI(void (Ass::*f)(Reg, const Imm&), size_t imm_bytes, const std::string& fmt) {
    return RepeatRegisterImm<RegisterView::kUseTertiaryName>(f, imm_bytes, fmt);
  }

  std::string RepeatbI(void (Ass::*f)(Reg, const Imm&), size_t imm_bytes, const std::string& fmt) {
    return RepeatRegisterImm<RegisterView::kUseQuaternaryName>(f, imm_bytes, fmt);
  }

  template <typename Reg1, typename Reg2, typename ImmType>
  std::string RepeatTemplatedRegistersImmBits(void (Ass::*f)(Reg1, Reg2, ImmType),
                                              int imm_bits,
                                              const std::vector<Reg1*> reg1_registers,
                                              const std::vector<Reg2*> reg2_registers,
                                              std::string (AssemblerTest::*GetName1)(const Reg1&),
                                              std::string (AssemblerTest::*GetName2)(const Reg2&),
                                              const std::string& fmt,
                                              int bias = 0,
                                              int multiplier = 1) {
    std::string str;
    std::vector<int64_t> imms = CreateImmediateValuesBits(abs(imm_bits), (imm_bits > 0));

    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        for (int64_t imm : imms) {
          ImmType new_imm = CreateImmediate(imm);
          if (f != nullptr) {
            (assembler_.get()->*f)(*reg1, *reg2, new_imm * multiplier + bias);
          }
          std::string base = fmt;

          std::string reg1_string = (this->*GetName1)(*reg1);
          size_t reg1_index;
          while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
            base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
          }

          std::string reg2_string = (this->*GetName2)(*reg2);
          size_t reg2_index;
          while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
            base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
          }

          size_t imm_index = base.find(IMM_TOKEN);
          if (imm_index != std::string::npos) {
            std::ostringstream sreg;
            sreg << imm * multiplier + bias;
            std::string imm_string = sreg.str();
            base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
          }

          if (str.size() > 0) {
            str += "\n";
          }
          str += base;
        }
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename Reg1, typename Reg2, typename Reg3, typename ImmType>
  std::string RepeatTemplatedRegistersImmBits(void (Ass::*f)(Reg1, Reg2, Reg3, ImmType),
                                              int imm_bits,
                                              const std::vector<Reg1*> reg1_registers,
                                              const std::vector<Reg2*> reg2_registers,
                                              const std::vector<Reg3*> reg3_registers,
                                              std::string (AssemblerTest::*GetName1)(const Reg1&),
                                              std::string (AssemblerTest::*GetName2)(const Reg2&),
                                              std::string (AssemblerTest::*GetName3)(const Reg3&),
                                              std::string fmt,
                                              int bias) {
    std::string str;
    std::vector<int64_t> imms = CreateImmediateValuesBits(abs(imm_bits), (imm_bits > 0));

    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        for (auto reg3 : reg3_registers) {
          for (int64_t imm : imms) {
            ImmType new_imm = CreateImmediate(imm);
            if (f != nullptr) {
              (assembler_.get()->*f)(*reg1, *reg2, *reg3, new_imm + bias);
            }
            std::string base = fmt;

            std::string reg1_string = (this->*GetName1)(*reg1);
            size_t reg1_index;
            while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
              base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
            }

            std::string reg2_string = (this->*GetName2)(*reg2);
            size_t reg2_index;
            while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
              base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
            }

            std::string reg3_string = (this->*GetName3)(*reg3);
            size_t reg3_index;
            while ((reg3_index = base.find(REG3_TOKEN)) != std::string::npos) {
              base.replace(reg3_index, ConstexprStrLen(REG3_TOKEN), reg3_string);
            }

            size_t imm_index = base.find(IMM_TOKEN);
            if (imm_index != std::string::npos) {
              std::ostringstream sreg;
              sreg << imm + bias;
              std::string imm_string = sreg.str();
              base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
            }

            if (str.size() > 0) {
              str += "\n";
            }
            str += base;
          }
        }
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename ImmType, typename Reg1, typename Reg2>
  std::string RepeatTemplatedImmBitsRegisters(void (Ass::*f)(ImmType, Reg1, Reg2),
                                              const std::vector<Reg1*> reg1_registers,
                                              const std::vector<Reg2*> reg2_registers,
                                              std::string (AssemblerTest::*GetName1)(const Reg1&),
                                              std::string (AssemblerTest::*GetName2)(const Reg2&),
                                              int imm_bits,
                                              const std::string& fmt) {
    std::vector<int64_t> imms = CreateImmediateValuesBits(abs(imm_bits), (imm_bits > 0));

    WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * imms.size());

    std::string str;
    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        for (int64_t imm : imms) {
          ImmType new_imm = CreateImmediate(imm);
          if (f != nullptr) {
            (assembler_.get()->*f)(new_imm, *reg1, *reg2);
          }
          std::string base = fmt;

          std::string reg1_string = (this->*GetName1)(*reg1);
          size_t reg1_index;
          while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
            base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
          }

          std::string reg2_string = (this->*GetName2)(*reg2);
          size_t reg2_index;
          while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
            base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
          }

          size_t imm_index = base.find(IMM_TOKEN);
          if (imm_index != std::string::npos) {
            std::ostringstream sreg;
            sreg << imm;
            std::string imm_string = sreg.str();
            base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
          }

          if (str.size() > 0) {
            str += "\n";
          }
          str += base;
        }
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename RegType, typename ImmType>
  std::string RepeatTemplatedRegisterImmBits(void (Ass::*f)(RegType, ImmType),
                                             int imm_bits,
                                             const std::vector<RegType*> registers,
                                             std::string (AssemblerTest::*GetName)(const RegType&),
                                             const std::string& fmt,
                                             int bias) {
    std::string str;
    std::vector<int64_t> imms = CreateImmediateValuesBits(abs(imm_bits), (imm_bits > 0));

    for (auto reg : registers) {
      for (int64_t imm : imms) {
        ImmType new_imm = CreateImmediate(imm);
        if (f != nullptr) {
          (assembler_.get()->*f)(*reg, new_imm + bias);
        }
        std::string base = fmt;

        std::string reg_string = (this->*GetName)(*reg);
        size_t reg_index;
        while ((reg_index = base.find(REG_TOKEN)) != std::string::npos) {
          base.replace(reg_index, ConstexprStrLen(REG_TOKEN), reg_string);
        }

        size_t imm_index = base.find(IMM_TOKEN);
        if (imm_index != std::string::npos) {
          std::ostringstream sreg;
          sreg << imm + bias;
          std::string imm_string = sreg.str();
          base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename ImmType>
  std::string RepeatRRIb(void (Ass::*f)(Reg, Reg, ImmType),
                         int imm_bits,
                         const std::string& fmt,
                         int bias = 0) {
    return RepeatTemplatedRegistersImmBits<Reg, Reg, ImmType>(f,
        imm_bits,
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt,
        bias);
  }

  template <typename ImmType>
  std::string RepeatRRRIb(void (Ass::*f)(Reg, Reg, Reg, ImmType),
                          int imm_bits,
                          const std::string& fmt,
                          int bias = 0) {
    return RepeatTemplatedRegistersImmBits<Reg, Reg, Reg, ImmType>(f,
        imm_bits,
        GetRegisters(),
        GetRegisters(),
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt,
        bias);
  }

  template <typename ImmType>
  std::string RepeatRIb(void (Ass::*f)(Reg, ImmType), int imm_bits, std::string fmt, int bias = 0) {
    return RepeatTemplatedRegisterImmBits<Reg, ImmType>(f,
        imm_bits,
        GetRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt,
        bias);
  }

  template <typename ImmType>
  std::string RepeatFRIb(void (Ass::*f)(FPReg, Reg, ImmType),
                         int imm_bits,
                         const std::string& fmt,
                         int bias = 0) {
    return RepeatTemplatedRegistersImmBits<FPReg, Reg, ImmType>(f,
        imm_bits,
        GetFPRegisters(),
        GetRegisters(),
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt,
        bias);
  }

  std::string RepeatFF(void (Ass::*f)(FPReg, FPReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<FPReg, FPReg>(f,
                                                  GetFPRegisters(),
                                                  GetFPRegisters(),
                                                  &AssemblerTest::GetFPRegName,
                                                  &AssemblerTest::GetFPRegName,
                                                  fmt);
  }

  std::string RepeatFFF(void (Ass::*f)(FPReg, FPReg, FPReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<FPReg, FPReg, FPReg>(f,
                                                         GetFPRegisters(),
                                                         GetFPRegisters(),
                                                         GetFPRegisters(),
                                                         &AssemblerTest::GetFPRegName,
                                                         &AssemblerTest::GetFPRegName,
                                                         &AssemblerTest::GetFPRegName,
                                                         fmt);
  }

  std::string RepeatFFR(void (Ass::*f)(FPReg, FPReg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<FPReg, FPReg, Reg>(
        f,
        GetFPRegisters(),
        GetFPRegisters(),
        GetRegisters(),
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string RepeatFFI(void (Ass::*f)(FPReg, FPReg, const Imm&),
                        size_t imm_bytes,
                        const std::string& fmt) {
    return RepeatTemplatedRegistersImm<FPReg, FPReg>(f,
                                                     GetFPRegisters(),
                                                     GetFPRegisters(),
                                                     &AssemblerTest::GetFPRegName,
                                                     &AssemblerTest::GetFPRegName,
                                                     imm_bytes,
                                                     fmt);
  }

  template <typename ImmType>
  std::string RepeatFFIb(void (Ass::*f)(FPReg, FPReg, ImmType),
                         int imm_bits,
                         const std::string& fmt) {
    return RepeatTemplatedRegistersImmBits<FPReg, FPReg, ImmType>(f,
                                                                  imm_bits,
                                                                  GetFPRegisters(),
                                                                  GetFPRegisters(),
                                                                  &AssemblerTest::GetFPRegName,
                                                                  &AssemblerTest::GetFPRegName,
                                                                  fmt);
  }

  template <typename ImmType>
  std::string RepeatIbFF(void (Ass::*f)(ImmType, FPReg, FPReg),
                         int imm_bits,
                         const std::string& fmt) {
    return RepeatTemplatedImmBitsRegisters<ImmType, FPReg, FPReg>(f,
                                                                  GetFPRegisters(),
                                                                  GetFPRegisters(),
                                                                  &AssemblerTest::GetFPRegName,
                                                                  &AssemblerTest::GetFPRegName,
                                                                  imm_bits,
                                                                  fmt);
  }

  std::string RepeatFR(void (Ass::*f)(FPReg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<FPReg, Reg>(f,
        GetFPRegisters(),
        GetRegisters(),
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  std::string RepeatFr(void (Ass::*f)(FPReg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<FPReg, Reg>(f,
        GetFPRegisters(),
        GetRegisters(),
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        fmt);
  }

  std::string RepeatRF(void (Ass::*f)(Reg, FPReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, FPReg>(f,
        GetRegisters(),
        GetFPRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetFPRegName,
        fmt);
  }

  std::string RepeatrF(void (Ass::*f)(Reg, FPReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<Reg, FPReg>(f,
        GetRegisters(),
        GetFPRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        &AssemblerTest::GetFPRegName,
        fmt);
  }

  std::string RepeatI(void (Ass::*f)(const Imm&),
                      size_t imm_bytes,
                      const std::string& fmt,
                      bool as_uint = false) {
    std::string str;
    std::vector<int64_t> imms = CreateImmediateValues(imm_bytes, as_uint);

    WarnOnCombinations(imms.size());

    for (int64_t imm : imms) {
      Imm new_imm = CreateImmediate(imm);
      if (f != nullptr) {
        (assembler_.get()->*f)(new_imm);
      }
      std::string base = fmt;

      size_t imm_index = base.find(IMM_TOKEN);
      if (imm_index != std::string::npos) {
        std::ostringstream sreg;
        sreg << imm;
        std::string imm_string = sreg.str();
        base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
      }

      if (str.size() > 0) {
        str += "\n";
      }
      str += base;
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  std::string RepeatVV(void (Ass::*f)(VecReg, VecReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<VecReg, VecReg>(f,
                                                    GetVectorRegisters(),
                                                    GetVectorRegisters(),
                                                    &AssemblerTest::GetVecRegName,
                                                    &AssemblerTest::GetVecRegName,
                                                    fmt);
  }

  std::string RepeatVVV(void (Ass::*f)(VecReg, VecReg, VecReg), const std::string& fmt) {
    return RepeatTemplatedRegisters<VecReg, VecReg, VecReg>(f,
                                                            GetVectorRegisters(),
                                                            GetVectorRegisters(),
                                                            GetVectorRegisters(),
                                                            &AssemblerTest::GetVecRegName,
                                                            &AssemblerTest::GetVecRegName,
                                                            &AssemblerTest::GetVecRegName,
                                                            fmt);
  }

  std::string RepeatVR(void (Ass::*f)(VecReg, Reg), const std::string& fmt) {
    return RepeatTemplatedRegisters<VecReg, Reg>(
        f,
        GetVectorRegisters(),
        GetRegisters(),
        &AssemblerTest::GetVecRegName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  template <typename ImmType>
  std::string RepeatVIb(void (Ass::*f)(VecReg, ImmType),
                        int imm_bits,
                        std::string fmt,
                        int bias = 0) {
    return RepeatTemplatedRegisterImmBits<VecReg, ImmType>(f,
                                                           imm_bits,
                                                           GetVectorRegisters(),
                                                           &AssemblerTest::GetVecRegName,
                                                           fmt,
                                                           bias);
  }

  template <typename ImmType>
  std::string RepeatVRIb(void (Ass::*f)(VecReg, Reg, ImmType),
                         int imm_bits,
                         const std::string& fmt,
                         int bias = 0,
                         int multiplier = 1) {
    return RepeatTemplatedRegistersImmBits<VecReg, Reg, ImmType>(
        f,
        imm_bits,
        GetVectorRegisters(),
        GetRegisters(),
        &AssemblerTest::GetVecRegName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt,
        bias,
        multiplier);
  }

  template <typename ImmType>
  std::string RepeatRVIb(void (Ass::*f)(Reg, VecReg, ImmType),
                         int imm_bits,
                         const std::string& fmt,
                         int bias = 0,
                         int multiplier = 1) {
    return RepeatTemplatedRegistersImmBits<Reg, VecReg, ImmType>(
        f,
        imm_bits,
        GetRegisters(),
        GetVectorRegisters(),
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetVecRegName,
        fmt,
        bias,
        multiplier);
  }

  template <typename ImmType>
  std::string RepeatVVIb(void (Ass::*f)(VecReg, VecReg, ImmType),
                         int imm_bits,
                         const std::string& fmt,
                         int bias = 0) {
    return RepeatTemplatedRegistersImmBits<VecReg, VecReg, ImmType>(f,
                                                                    imm_bits,
                                                                    GetVectorRegisters(),
                                                                    GetVectorRegisters(),
                                                                    &AssemblerTest::GetVecRegName,
                                                                    &AssemblerTest::GetVecRegName,
                                                                    fmt,
                                                                    bias);
  }

  // This is intended to be run as a test.
  bool CheckTools() {
    return test_helper_->CheckTools();
  }

  // The following functions are public so that TestFn can use them...

  // Returns a vector of address used by any of the repeat methods
  // involving an "A" (e.g. RepeatA).
  virtual std::vector<Addr> GetAddresses() = 0;

  // Returns a vector of registers used by any of the repeat methods
  // involving an "R" (e.g. RepeatR).
  virtual std::vector<Reg*> GetRegisters() = 0;

  // Returns a vector of fp-registers used by any of the repeat methods
  // involving an "F" (e.g. RepeatFF).
  virtual std::vector<FPReg*> GetFPRegisters() {
    UNIMPLEMENTED(FATAL) << "Architecture does not support floating-point registers";
    UNREACHABLE();
  }

  // Returns a vector of dedicated simd-registers used by any of the repeat
  // methods involving an "V" (e.g. RepeatVV).
  virtual std::vector<VecReg*> GetVectorRegisters() {
    UNIMPLEMENTED(FATAL) << "Architecture does not support vector registers";
    UNREACHABLE();
  }

  // Secondary register names are the secondary view on registers, e.g., 32b on 64b systems.
  virtual std::string GetSecondaryRegisterName(const Reg& reg ATTRIBUTE_UNUSED) {
    UNIMPLEMENTED(FATAL) << "Architecture does not support secondary registers";
    UNREACHABLE();
  }

  // Tertiary register names are the tertiary view on registers, e.g., 16b on 64b systems.
  virtual std::string GetTertiaryRegisterName(const Reg& reg ATTRIBUTE_UNUSED) {
    UNIMPLEMENTED(FATAL) << "Architecture does not support tertiary registers";
    UNREACHABLE();
  }

  // Quaternary register names are the quaternary view on registers, e.g., 8b on 64b systems.
  virtual std::string GetQuaternaryRegisterName(const Reg& reg ATTRIBUTE_UNUSED) {
    UNIMPLEMENTED(FATAL) << "Architecture does not support quaternary registers";
    UNREACHABLE();
  }

  std::string GetRegisterName(const Reg& reg) {
    return GetRegName<RegisterView::kUsePrimaryName>(reg);
  }

 protected:
  AssemblerTest() {}

  void SetUp() OVERRIDE {
    allocator_.reset(new ArenaAllocator(&pool_));
    assembler_.reset(CreateAssembler(allocator_.get()));
    test_helper_.reset(
        new AssemblerTestInfrastructure(GetArchitectureString(),
                                        GetAssemblerCmdName(),
                                        GetAssemblerParameters(),
                                        GetObjdumpCmdName(),
                                        GetObjdumpParameters(),
                                        GetDisassembleCmdName(),
                                        GetDisassembleParameters(),
                                        GetAssemblyHeader()));

    SetUpHelpers();
  }

  void TearDown() OVERRIDE {
    test_helper_.reset();  // Clean up the helper.
    assembler_.reset();
    allocator_.reset();
  }

  // Override this to set up any architecture-specific things, e.g., CPU revision.
  virtual Ass* CreateAssembler(ArenaAllocator* allocator) {
    return new (allocator) Ass(allocator);
  }

  // Override this to set up any architecture-specific things, e.g., register vectors.
  virtual void SetUpHelpers() {}

  // Get the typically used name for this architecture, e.g., aarch64, x86_64, ...
  virtual std::string GetArchitectureString() = 0;

  // Get the name of the assembler, e.g., "as" by default.
  virtual std::string GetAssemblerCmdName() {
    return "as";
  }

  // Switches to the assembler command. Default none.
  virtual std::string GetAssemblerParameters() {
    return "";
  }

  // Get the name of the objdump, e.g., "objdump" by default.
  virtual std::string GetObjdumpCmdName() {
    return "objdump";
  }

  // Switches to the objdump command. Default is " -h".
  virtual std::string GetObjdumpParameters() {
    return " -h";
  }

  // Get the name of the objdump, e.g., "objdump" by default.
  virtual std::string GetDisassembleCmdName() {
    return "objdump";
  }

  // Switches to the objdump command. As it's a binary, one needs to push the architecture and
  // such to objdump, so it's architecture-specific and there is no default.
  virtual std::string GetDisassembleParameters() = 0;

  // Create a couple of immediate values up to the number of bytes given.
  virtual std::vector<int64_t> CreateImmediateValues(size_t imm_bytes, bool as_uint = false) {
    std::vector<int64_t> res;
    res.push_back(0);
    if (!as_uint) {
      res.push_back(-1);
    } else {
      res.push_back(0xFF);
    }
    res.push_back(0x12);
    if (imm_bytes >= 2) {
      res.push_back(0x1234);
      if (!as_uint) {
        res.push_back(-0x1234);
      } else {
        res.push_back(0xFFFF);
      }
      if (imm_bytes >= 4) {
        res.push_back(0x12345678);
        if (!as_uint) {
          res.push_back(-0x12345678);
        } else {
          res.push_back(0xFFFFFFFF);
        }
        if (imm_bytes >= 6) {
          res.push_back(0x123456789ABC);
          if (!as_uint) {
            res.push_back(-0x123456789ABC);
          }
          if (imm_bytes >= 8) {
            res.push_back(0x123456789ABCDEF0);
            if (!as_uint) {
              res.push_back(-0x123456789ABCDEF0);
            } else {
              res.push_back(0xFFFFFFFFFFFFFFFF);
            }
          }
        }
      }
    }
    return res;
  }

  const int kMaxBitsExhaustiveTest = 8;

  // Create a couple of immediate values up to the number of bits given.
  virtual std::vector<int64_t> CreateImmediateValuesBits(const int imm_bits, bool as_uint = false) {
    CHECK_GT(imm_bits, 0);
    CHECK_LE(imm_bits, 64);
    std::vector<int64_t> res;

    if (imm_bits <= kMaxBitsExhaustiveTest) {
      if (as_uint) {
        for (uint64_t i = MinInt<uint64_t>(imm_bits); i <= MaxInt<uint64_t>(imm_bits); i++) {
          res.push_back(static_cast<int64_t>(i));
        }
      } else {
        for (int64_t i = MinInt<int64_t>(imm_bits); i <= MaxInt<int64_t>(imm_bits); i++) {
          res.push_back(i);
        }
      }
    } else {
      if (as_uint) {
        for (uint64_t i = MinInt<uint64_t>(kMaxBitsExhaustiveTest);
             i <= MaxInt<uint64_t>(kMaxBitsExhaustiveTest);
             i++) {
          res.push_back(static_cast<int64_t>(i));
        }
        for (int i = 0; i <= imm_bits; i++) {
          uint64_t j = (MaxInt<uint64_t>(kMaxBitsExhaustiveTest) + 1) +
                       ((MaxInt<uint64_t>(imm_bits) -
                        (MaxInt<uint64_t>(kMaxBitsExhaustiveTest) + 1))
                        * i / imm_bits);
          res.push_back(static_cast<int64_t>(j));
        }
      } else {
        for (int i = 0; i <= imm_bits; i++) {
          int64_t j = MinInt<int64_t>(imm_bits) +
                      ((((MinInt<int64_t>(kMaxBitsExhaustiveTest) - 1) -
                         MinInt<int64_t>(imm_bits))
                        * i) / imm_bits);
          res.push_back(static_cast<int64_t>(j));
        }
        for (int64_t i = MinInt<int64_t>(kMaxBitsExhaustiveTest);
             i <= MaxInt<int64_t>(kMaxBitsExhaustiveTest);
             i++) {
          res.push_back(static_cast<int64_t>(i));
        }
        for (int i = 0; i <= imm_bits; i++) {
          int64_t j = (MaxInt<int64_t>(kMaxBitsExhaustiveTest) + 1) +
                      ((MaxInt<int64_t>(imm_bits) - (MaxInt<int64_t>(kMaxBitsExhaustiveTest) + 1))
                       * i / imm_bits);
          res.push_back(static_cast<int64_t>(j));
        }
      }
    }

    return res;
  }

  // Create an immediate from the specific value.
  virtual Imm CreateImmediate(int64_t imm_value) = 0;

  //
  // Addresses repeats.
  //

  // Repeats over addresses provided by fixture.
  std::string RepeatA(void (Ass::*f)(const Addr&), const std::string& fmt) {
    return RepeatA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatA(void (Ass::*f)(const Addr&),
                      const std::vector<Addr>& a,
                      const std::string& fmt) {
    return RepeatTemplatedMem<Addr>(f, a, &AssemblerTest::GetAddrName, fmt);
  }

  // Repeats over addresses and immediates provided by fixture.
  std::string RepeatAI(void (Ass::*f)(const Addr&, const Imm&),
                       size_t imm_bytes,
                       const std::string& fmt) {
    return RepeatAI(f, imm_bytes, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAI(void (Ass::*f)(const Addr&, const Imm&),
                       size_t imm_bytes,
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemImm<Addr>(f, imm_bytes, a, &AssemblerTest::GetAddrName, fmt);
  }

  // Repeats over registers and addresses provided by fixture.
  std::string RepeatRA(void (Ass::*f)(Reg, const Addr&), const std::string& fmt) {
    return RepeatRA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatRA(void (Ass::*f)(Reg, const Addr&),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedRegMem<Reg, Addr>(
        f,
        GetRegisters(),
        a,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        &AssemblerTest::GetAddrName,
        fmt);
  }

  // Repeats over secondary registers and addresses provided by fixture.
  std::string RepeatrA(void (Ass::*f)(Reg, const Addr&), const std::string& fmt) {
    return RepeatrA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatrA(void (Ass::*f)(Reg, const Addr&),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedRegMem<Reg, Addr>(
        f,
        GetRegisters(),
        a,
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        &AssemblerTest::GetAddrName,
        fmt);
  }

  // Repeats over tertiary registers and addresses provided by fixture.
  std::string RepeatwA(void (Ass::*f)(Reg, const Addr&), const std::string& fmt) {
    return RepeatwA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatwA(void (Ass::*f)(Reg, const Addr&),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedRegMem<Reg, Addr>(
        f,
        GetRegisters(),
        a,
        &AssemblerTest::GetRegName<RegisterView::kUseTertiaryName>,
        &AssemblerTest::GetAddrName,
        fmt);
  }

  // Repeats over quaternary registers and addresses provided by fixture.
  std::string RepeatbA(void (Ass::*f)(Reg, const Addr&), const std::string& fmt) {
    return RepeatbA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatbA(void (Ass::*f)(Reg, const Addr&),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedRegMem<Reg, Addr>(
        f,
        GetRegisters(),
        a,
        &AssemblerTest::GetRegName<RegisterView::kUseQuaternaryName>,
        &AssemblerTest::GetAddrName,
        fmt);
  }

  // Repeats over fp-registers and addresses provided by fixture.
  std::string RepeatFA(void (Ass::*f)(FPReg, const Addr&), const std::string& fmt) {
    return RepeatFA(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatFA(void (Ass::*f)(FPReg, const Addr&),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedRegMem<FPReg, Addr>(
        f,
        GetFPRegisters(),
        a,
        &AssemblerTest::GetFPRegName,
        &AssemblerTest::GetAddrName,
        fmt);
  }

  // Repeats over addresses and registers provided by fixture.
  std::string RepeatAR(void (Ass::*f)(const Addr&, Reg), const std::string& fmt) {
    return RepeatAR(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAR(void (Ass::*f)(const Addr&, Reg),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemReg<Addr, Reg>(
        f,
        a,
        GetRegisters(),
        &AssemblerTest::GetAddrName,
        &AssemblerTest::GetRegName<RegisterView::kUsePrimaryName>,
        fmt);
  }

  // Repeats over addresses and secondary registers provided by fixture.
  std::string RepeatAr(void (Ass::*f)(const Addr&, Reg), const std::string& fmt) {
    return RepeatAr(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAr(void (Ass::*f)(const Addr&, Reg),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemReg<Addr, Reg>(
        f,
        a,
        GetRegisters(),
        &AssemblerTest::GetAddrName,
        &AssemblerTest::GetRegName<RegisterView::kUseSecondaryName>,
        fmt);
  }

  // Repeats over addresses and tertiary registers provided by fixture.
  std::string RepeatAw(void (Ass::*f)(const Addr&, Reg), const std::string& fmt) {
    return RepeatAw(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAw(void (Ass::*f)(const Addr&, Reg),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemReg<Addr, Reg>(
        f,
        a,
        GetRegisters(),
        &AssemblerTest::GetAddrName,
        &AssemblerTest::GetRegName<RegisterView::kUseTertiaryName>,
        fmt);
  }

  // Repeats over addresses and quaternary registers provided by fixture.
  std::string RepeatAb(void (Ass::*f)(const Addr&, Reg), const std::string& fmt) {
    return RepeatAb(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAb(void (Ass::*f)(const Addr&, Reg),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemReg<Addr, Reg>(
        f,
        a,
        GetRegisters(),
        &AssemblerTest::GetAddrName,
        &AssemblerTest::GetRegName<RegisterView::kUseQuaternaryName>,
        fmt);
  }

  // Repeats over addresses and fp-registers provided by fixture.
  std::string RepeatAF(void (Ass::*f)(const Addr&, FPReg), const std::string& fmt) {
    return RepeatAF(f, GetAddresses(), fmt);
  }

  // Variant that takes explicit vector of addresss
  // (to test restricted addressing modes set).
  std::string RepeatAF(void (Ass::*f)(const Addr&, FPReg),
                       const std::vector<Addr>& a,
                       const std::string& fmt) {
    return RepeatTemplatedMemReg<Addr, FPReg>(
        f,
        a,
        GetFPRegisters(),
        &AssemblerTest::GetAddrName,
        &AssemblerTest::GetFPRegName,
        fmt);
  }

  template <typename AddrType>
  std::string RepeatTemplatedMem(void (Ass::*f)(const AddrType&),
                                 const std::vector<AddrType> addresses,
                                 std::string (AssemblerTest::*GetAName)(const AddrType&),
                                 const std::string& fmt) {
    WarnOnCombinations(addresses.size());
    std::string str;
    for (auto addr : addresses) {
      if (f != nullptr) {
        (assembler_.get()->*f)(addr);
      }
      std::string base = fmt;

      std::string addr_string = (this->*GetAName)(addr);
      size_t addr_index;
      if ((addr_index = base.find(ADDRESS_TOKEN)) != std::string::npos) {
        base.replace(addr_index, ConstexprStrLen(ADDRESS_TOKEN), addr_string);
      }

      if (str.size() > 0) {
        str += "\n";
      }
      str += base;
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename AddrType>
  std::string RepeatTemplatedMemImm(void (Ass::*f)(const AddrType&, const Imm&),
                                    size_t imm_bytes,
                                    const std::vector<AddrType> addresses,
                                    std::string (AssemblerTest::*GetAName)(const AddrType&),
                                    const std::string& fmt) {
    std::vector<int64_t> imms = CreateImmediateValues(imm_bytes);
    WarnOnCombinations(addresses.size() * imms.size());
    std::string str;
    for (auto addr : addresses) {
      for (int64_t imm : imms) {
        Imm new_imm = CreateImmediate(imm);
        if (f != nullptr) {
          (assembler_.get()->*f)(addr, new_imm);
        }
        std::string base = fmt;

        std::string addr_string = (this->*GetAName)(addr);
        size_t addr_index;
        if ((addr_index = base.find(ADDRESS_TOKEN)) != std::string::npos) {
          base.replace(addr_index, ConstexprStrLen(ADDRESS_TOKEN), addr_string);
        }

        size_t imm_index = base.find(IMM_TOKEN);
        if (imm_index != std::string::npos) {
          std::ostringstream sreg;
          sreg << imm;
          std::string imm_string = sreg.str();
          base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename RegType, typename AddrType>
  std::string RepeatTemplatedRegMem(void (Ass::*f)(RegType, const AddrType&),
                                    const std::vector<RegType*> registers,
                                    const std::vector<AddrType> addresses,
                                    std::string (AssemblerTest::*GetRName)(const RegType&),
                                    std::string (AssemblerTest::*GetAName)(const AddrType&),
                                    const std::string& fmt) {
    WarnOnCombinations(addresses.size() * registers.size());
    std::string str;
    for (auto reg : registers) {
      for (auto addr : addresses) {
        if (f != nullptr) {
          (assembler_.get()->*f)(*reg, addr);
        }
        std::string base = fmt;

        std::string reg_string = (this->*GetRName)(*reg);
        size_t reg_index;
        if ((reg_index = base.find(REG_TOKEN)) != std::string::npos) {
          base.replace(reg_index, ConstexprStrLen(REG_TOKEN), reg_string);
        }

        std::string addr_string = (this->*GetAName)(addr);
        size_t addr_index;
        if ((addr_index = base.find(ADDRESS_TOKEN)) != std::string::npos) {
          base.replace(addr_index, ConstexprStrLen(ADDRESS_TOKEN), addr_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename AddrType, typename RegType>
  std::string RepeatTemplatedMemReg(void (Ass::*f)(const AddrType&, RegType),
                                    const std::vector<AddrType> addresses,
                                    const std::vector<RegType*> registers,
                                    std::string (AssemblerTest::*GetAName)(const AddrType&),
                                    std::string (AssemblerTest::*GetRName)(const RegType&),
                                    const std::string& fmt) {
    WarnOnCombinations(addresses.size() * registers.size());
    std::string str;
    for (auto addr : addresses) {
      for (auto reg : registers) {
        if (f != nullptr) {
          (assembler_.get()->*f)(addr, *reg);
        }
        std::string base = fmt;

        std::string addr_string = (this->*GetAName)(addr);
        size_t addr_index;
        if ((addr_index = base.find(ADDRESS_TOKEN)) != std::string::npos) {
          base.replace(addr_index, ConstexprStrLen(ADDRESS_TOKEN), addr_string);
        }

        std::string reg_string = (this->*GetRName)(*reg);
        size_t reg_index;
        if ((reg_index = base.find(REG_TOKEN)) != std::string::npos) {
          base.replace(reg_index, ConstexprStrLen(REG_TOKEN), reg_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  //
  // Register repeats.
  //

  template <typename RegType>
  std::string RepeatTemplatedRegister(void (Ass::*f)(RegType),
                                      const std::vector<RegType*> registers,
                                      std::string (AssemblerTest::*GetName)(const RegType&),
                                      const std::string& fmt) {
    std::string str;
    for (auto reg : registers) {
      if (f != nullptr) {
        (assembler_.get()->*f)(*reg);
      }
      std::string base = fmt;

      std::string reg_string = (this->*GetName)(*reg);
      size_t reg_index;
      if ((reg_index = base.find(REG_TOKEN)) != std::string::npos) {
        base.replace(reg_index, ConstexprStrLen(REG_TOKEN), reg_string);
      }

      if (str.size() > 0) {
        str += "\n";
      }
      str += base;
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename Reg1, typename Reg2>
  std::string RepeatTemplatedRegisters(void (Ass::*f)(Reg1, Reg2),
                                       const std::vector<Reg1*> reg1_registers,
                                       const std::vector<Reg2*> reg2_registers,
                                       std::string (AssemblerTest::*GetName1)(const Reg1&),
                                       std::string (AssemblerTest::*GetName2)(const Reg2&),
                                       const std::string& fmt) {
    WarnOnCombinations(reg1_registers.size() * reg2_registers.size());

    std::string str;
    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        if (f != nullptr) {
          (assembler_.get()->*f)(*reg1, *reg2);
        }
        std::string base = fmt;

        std::string reg1_string = (this->*GetName1)(*reg1);
        size_t reg1_index;
        while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
          base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
        }

        std::string reg2_string = (this->*GetName2)(*reg2);
        size_t reg2_index;
        while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
          base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename Reg1, typename Reg2>
  std::string RepeatTemplatedRegistersNoDupes(void (Ass::*f)(Reg1, Reg2),
                                              const std::vector<Reg1*> reg1_registers,
                                              const std::vector<Reg2*> reg2_registers,
                                              std::string (AssemblerTest::*GetName1)(const Reg1&),
                                              std::string (AssemblerTest::*GetName2)(const Reg2&),
                                              const std::string& fmt) {
    WarnOnCombinations(reg1_registers.size() * reg2_registers.size());

    std::string str;
    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        if (reg1 == reg2) continue;
        if (f != nullptr) {
          (assembler_.get()->*f)(*reg1, *reg2);
        }
        std::string base = fmt;

        std::string reg1_string = (this->*GetName1)(*reg1);
        size_t reg1_index;
        while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
          base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
        }

        std::string reg2_string = (this->*GetName2)(*reg2);
        size_t reg2_index;
        while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
          base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename Reg1, typename Reg2, typename Reg3>
  std::string RepeatTemplatedRegisters(void (Ass::*f)(Reg1, Reg2, Reg3),
                                       const std::vector<Reg1*> reg1_registers,
                                       const std::vector<Reg2*> reg2_registers,
                                       const std::vector<Reg3*> reg3_registers,
                                       std::string (AssemblerTest::*GetName1)(const Reg1&),
                                       std::string (AssemblerTest::*GetName2)(const Reg2&),
                                       std::string (AssemblerTest::*GetName3)(const Reg3&),
                                       const std::string& fmt) {
    std::string str;
    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        for (auto reg3 : reg3_registers) {
          if (f != nullptr) {
            (assembler_.get()->*f)(*reg1, *reg2, *reg3);
          }
          std::string base = fmt;

          std::string reg1_string = (this->*GetName1)(*reg1);
          size_t reg1_index;
          while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
            base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
          }

          std::string reg2_string = (this->*GetName2)(*reg2);
          size_t reg2_index;
          while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
            base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
          }

          std::string reg3_string = (this->*GetName3)(*reg3);
          size_t reg3_index;
          while ((reg3_index = base.find(REG3_TOKEN)) != std::string::npos) {
            base.replace(reg3_index, ConstexprStrLen(REG3_TOKEN), reg3_string);
          }

          if (str.size() > 0) {
            str += "\n";
          }
          str += base;
        }
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  template <typename Reg1, typename Reg2>
  std::string RepeatTemplatedRegistersImm(void (Ass::*f)(Reg1, Reg2, const Imm&),
                                          const std::vector<Reg1*> reg1_registers,
                                          const std::vector<Reg2*> reg2_registers,
                                          std::string (AssemblerTest::*GetName1)(const Reg1&),
                                          std::string (AssemblerTest::*GetName2)(const Reg2&),
                                          size_t imm_bytes,
                                          const std::string& fmt) {
    std::vector<int64_t> imms = CreateImmediateValues(imm_bytes);
    WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * imms.size());

    std::string str;
    for (auto reg1 : reg1_registers) {
      for (auto reg2 : reg2_registers) {
        for (int64_t imm : imms) {
          Imm new_imm = CreateImmediate(imm);
          if (f != nullptr) {
            (assembler_.get()->*f)(*reg1, *reg2, new_imm);
          }
          std::string base = fmt;

          std::string reg1_string = (this->*GetName1)(*reg1);
          size_t reg1_index;
          while ((reg1_index = base.find(REG1_TOKEN)) != std::string::npos) {
            base.replace(reg1_index, ConstexprStrLen(REG1_TOKEN), reg1_string);
          }

          std::string reg2_string = (this->*GetName2)(*reg2);
          size_t reg2_index;
          while ((reg2_index = base.find(REG2_TOKEN)) != std::string::npos) {
            base.replace(reg2_index, ConstexprStrLen(REG2_TOKEN), reg2_string);
          }

          size_t imm_index = base.find(IMM_TOKEN);
          if (imm_index != std::string::npos) {
            std::ostringstream sreg;
            sreg << imm;
            std::string imm_string = sreg.str();
            base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
          }

          if (str.size() > 0) {
            str += "\n";
          }
          str += base;
        }
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  std::string GetAddrName(const Addr& addr) {
    std::ostringstream saddr;
    saddr << addr;
    return saddr.str();
  }

  template <RegisterView kRegView>
  std::string GetRegName(const Reg& reg) {
    std::ostringstream sreg;
    switch (kRegView) {
      case RegisterView::kUsePrimaryName:
        sreg << reg;
        break;

      case RegisterView::kUseSecondaryName:
        sreg << GetSecondaryRegisterName(reg);
        break;

      case RegisterView::kUseTertiaryName:
        sreg << GetTertiaryRegisterName(reg);
        break;

      case RegisterView::kUseQuaternaryName:
        sreg << GetQuaternaryRegisterName(reg);
        break;
    }
    return sreg.str();
  }

  std::string GetFPRegName(const FPReg& reg) {
    std::ostringstream sreg;
    sreg << reg;
    return sreg.str();
  }

  std::string GetVecRegName(const VecReg& reg) {
    std::ostringstream sreg;
    sreg << reg;
    return sreg.str();
  }

  // If the assembly file needs a header, return it in a sub-class.
  virtual const char* GetAssemblyHeader() {
    return nullptr;
  }

  void WarnOnCombinations(size_t count) {
    if (count > kWarnManyCombinationsThreshold) {
      GTEST_LOG_(WARNING) << "Many combinations (" << count << "), test generation might be slow.";
    }
  }

  static constexpr const char* ADDRESS_TOKEN = "{mem}";
  static constexpr const char* REG_TOKEN = "{reg}";
  static constexpr const char* REG1_TOKEN = "{reg1}";
  static constexpr const char* REG2_TOKEN = "{reg2}";
  static constexpr const char* REG3_TOKEN = "{reg3}";
  static constexpr const char* IMM_TOKEN = "{imm}";

 private:
  template <RegisterView kRegView>
  std::string RepeatRegisterImm(void (Ass::*f)(Reg, const Imm&),
                                size_t imm_bytes,
                                const std::string& fmt) {
    const std::vector<Reg*> registers = GetRegisters();
    std::string str;
    std::vector<int64_t> imms = CreateImmediateValues(imm_bytes);

    WarnOnCombinations(registers.size() * imms.size());

    for (auto reg : registers) {
      for (int64_t imm : imms) {
        Imm new_imm = CreateImmediate(imm);
        if (f != nullptr) {
          (assembler_.get()->*f)(*reg, new_imm);
        }
        std::string base = fmt;

        std::string reg_string = GetRegName<kRegView>(*reg);
        size_t reg_index;
        while ((reg_index = base.find(REG_TOKEN)) != std::string::npos) {
          base.replace(reg_index, ConstexprStrLen(REG_TOKEN), reg_string);
        }

        size_t imm_index = base.find(IMM_TOKEN);
        if (imm_index != std::string::npos) {
          std::ostringstream sreg;
          sreg << imm;
          std::string imm_string = sreg.str();
          base.replace(imm_index, ConstexprStrLen(IMM_TOKEN), imm_string);
        }

        if (str.size() > 0) {
          str += "\n";
        }
        str += base;
      }
    }
    // Add a newline at the end.
    str += "\n";
    return str;
  }

  // Override this to pad the code with NOPs to a certain size if needed.
  virtual void Pad(std::vector<uint8_t>& data ATTRIBUTE_UNUSED) {
  }

  void DriverWrapper(const std::string& assembly_text, const std::string& test_name) {
    assembler_->FinalizeCode();
    size_t cs = assembler_->CodeSize();
    std::unique_ptr<std::vector<uint8_t>> data(new std::vector<uint8_t>(cs));
    MemoryRegion code(&(*data)[0], data->size());
    assembler_->FinalizeInstructions(code);
    Pad(*data);
    test_helper_->Driver(*data, assembly_text, test_name);
  }

  static constexpr size_t kWarnManyCombinationsThreshold = 500;

  ArenaPool pool_;
  std::unique_ptr<ArenaAllocator> allocator_;
  std::unique_ptr<Ass> assembler_;
  std::unique_ptr<AssemblerTestInfrastructure> test_helper_;

  DISALLOW_COPY_AND_ASSIGN(AssemblerTest);
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_ASSEMBLER_TEST_H_
