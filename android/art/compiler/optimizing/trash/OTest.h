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

#include <functional>
#include <stdio.h>
#include <sys/mman.h> //for PROT__

#include "art_method.h"
#include "arch/x86/instruction_set_features_x86.h"
#include "code_generator_x86.h"
#include "constant_folding.h"
#include "dead_code_elimination.h"
#include "driver/compiler_options.h"
#include "graph_checker.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"
#include "class_linker.h"
#include "well_known_classes.h"
#include "driver/compiler_driver.h"
#include "gtest/gtest.h"
#include "builder.h"
namespace art {

/**
 * Fixture class for the constant folding and dce tests.
 */
class OTest : public OptimizingUnitTest {
 public:
  OTest() : graph_(nullptr) { }
  
  std::string GetTestDexFileName(const char* name);
 
  std::vector<std::unique_ptr<const DexFile>> OpenTestDexFiles(const char* name);
    
  jobject LoadDexInWellKnownClassLoader(const std::string& dex_name,
                                                               jclass loader_class,
                                                               jobject parent_loader);
  jobject LoadDex(const char* dex_name);

  void TestCode(const std::vector<uint16_t>& data, jobject class_loader);

  jobject LoadDexInPathClassLoader(const std::string& dex_name, jobject parent_loader);
  
  void TestCodeOnReadyGraph();
  virtual void SetUp() {
    CommonCompilerTest::SetUp();
    std::cout << "setup()\n";
  }
  void EnsureCompiled(jobject class_loader, const char* class_name, const char* method,
              const char* signature, bool is_virtual);

  void CompileAll(jobject class_loader) REQUIRES(!Locks::mutator_lock_);
  const std::vector<const DexFile*>& GetDexFiles() const {
            return dex_files_;
              }
  HGraph* CreateCFG(const std::vector<uint16_t>& data,
          uint32_t access_flags,
          InvokeType invoke_type ATTRIBUTE_UNUSED,
          uint16_t class_def_idx,
          uint32_t method_idx,
          jobject class_loader,
          const DexFile& dex_file);
  std::vector<const DexFile*> dex_files_;
  HGraph* graph_;
  JNIEnv* env_;
};

}  // namespace art
