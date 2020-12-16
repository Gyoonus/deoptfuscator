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

#include "OTest.h"
#include "art_method-inl.h"


namespace art {
    /**
     * Fixture class for the constant folding and dce tests.
     */
    HGraph* OTest::CreateCFG(const std::vector<uint16_t>& data,
            uint32_t access_flags,
            InvokeType invoke_type ATTRIBUTE_UNUSED,
            uint16_t class_def_idx,
            uint32_t method_idx,
            jobject class_loader,
            const DexFile& dex_file)
    {
        HGraph* graph = CreateGraph();
        OptimizingCompilerStats* stats_ = nullptr;
        const size_t code_item_size = data.size() * sizeof(data.front());
        void* aligned_data = GetAllocator()->Alloc(code_item_size);
        memcpy(aligned_data, &data[0], code_item_size);
        CHECK_ALIGNED(aligned_data, StandardDexFile::CodeItem::kAlignment);
        const DexFile::CodeItem* code_item = reinterpret_cast<const DexFile::CodeItem*>(aligned_data);

        ScopedObjectAccess soa(Thread::Current());
        StackHandleScope<2> hs(soa.Self());
        VariableSizedHandleScope handles(soa.Self());

        ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
        Handle<mirror::ClassLoader> h_class_loader(
                hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader)));
        mirror::Class* class_ = class_linker->FindClass(soa.Self(), "com/test/helloandroid/MainActivity", h_class_loader);
        CHECK(class_ != nullptr) << "Class not found: 1";
        Handle<mirror::DexCache> dex_cache = hs.NewHandle(class_linker->FindDexCache(soa.Self(), dex_file));//hs.NewHandle(class_linker->GetDexCache());
            art::DexCompilationUnit unit(
                    h_class_loader,
                    class_linker,
                    dex_file,
                    code_item,
                    class_def_idx,
                    method_idx,
                    access_flags,
                    compiler_driver_->GetVerifiedMethod(&dex_file, method_idx),
                    dex_cache);
        std::unique_ptr<CodeGenerator> codegen(
                CodeGenerator::Create(graph,
                    compiler_driver_->GetInstructionSet(),
                    *compiler_driver_->GetInstructionSetFeatures(),
                    compiler_driver_->GetCompilerOptions(),
                    nullptr));
        CodeItemDebugInfoAccessor accessor(dex_file, code_item, method_idx);

        ArtMethod* method = compiler_driver_->ResolveMethod(soa, dex_cache, h_class_loader, &unit, method_idx, invoke_type);
        std::cout << "aa " << method->GetShorty();
        ArrayRef<const uint8_t> interpreter_metadata;
        if (method != nullptr) {
            graph->SetArtMethod(method);
            interpreter_metadata = method->GetQuickenedInfo();
        }
        (void)interpreter_metadata;
        (void)stats_;

        HGraphBuilder builder(graph, accessor, &unit, &unit, compiler_driver_.get(), codegen.get(), stats_,  interpreter_metadata, &handles);
        bool graph_built = (builder.BuildGraph() == kAnalysisSuccess);

        return graph_built ? graph : nullptr;
    }
    void OTest::TestCode(const std::vector<uint16_t>& data, jobject class_loader)
    {
        const DexFile* dex_file = dex_files_.front();           
        graph_ = CreateCFG(data, 1, InvokeType::kVirtual, 3, 7, class_loader, *dex_file);
        TestCodeOnReadyGraph();
    }

    void OTest::TestCodeOnReadyGraph()
    {

        ASSERT_NE(graph_, nullptr);

        StringPrettyPrinter printer_before(graph_);
        printer_before.VisitInsertionOrder();
        std::string actual_before = printer_before.str();
        std::cout << "before==\n" << actual_before << std::endl;

        std::unique_ptr<const X86InstructionSetFeatures> features_x86(
                X86InstructionSetFeatures::FromCppDefines());
        x86::CodeGeneratorX86 codegenX86(graph_, *features_x86.get(), CompilerOptions());
        HConstantFolding(graph_, "constant_folding").Run();
        GraphChecker graph_checker_cf(graph_);
        graph_checker_cf.Run();
        ASSERT_TRUE(graph_checker_cf.IsValid());

        StringPrettyPrinter printer_after_cf(graph_);
        printer_after_cf.VisitInsertionOrder();
        std::string actual_after_cf = printer_after_cf.str();


        HDeadCodeElimination(graph_, nullptr /* stats */, "dead_code_elimination").Run();
        GraphChecker graph_checker_dce(graph_);
        graph_checker_dce.Run();
        ASSERT_TRUE(graph_checker_dce.IsValid());
        RemoveSuspendChecks(graph_);

        StringPrettyPrinter printer_after_dce(graph_);
        printer_after_dce.VisitInsertionOrder();
        std::string actual_after_dce = printer_after_dce.str();

        std::cout << "after==\n" << actual_after_dce << std::endl;
    }
    void OTest::CompileAll(jobject class_loader) REQUIRES(!Locks::mutator_lock_) {
        TimingLogger timings("CompilerDriverTest::CompileAll", false, false);
        TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
        dex_files_ = CommonRuntimeTestImpl::GetDexFiles(class_loader);
        std::cout << dex_files_.size()<< std::endl;
        ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
        for (const auto& dex_file : dex_files_) {
            ScopedObjectAccess soa(Thread::Current());
            // Registering the dex cache adds a strong root in the class loader that prevents the dex
            // cache from being unloaded early.
            ObjPtr<mirror::DexCache> dex_cache = class_linker->RegisterDexFile(
                    *dex_file,
                    soa.Decode<mirror::ClassLoader>(class_loader));
            std::cout << "dex_cache\n";
            CHECK(dex_cache != nullptr) << "Class not found: ";
        }
        this->compiler_driver_->SetDexFilesForOatFile(dex_files_);
        this->compiler_driver_->CompileAll(class_loader, dex_files_, &timings);
    }

    std::string OTest::GetTestDexFileName(const char* name) {
        CHECK(name != nullptr);
        std::string filename = name;
        return name;
    }
    std::vector<std::unique_ptr<const DexFile>> OTest::OpenTestDexFiles(
            const char* name) {
        std::string filename = GetTestDexFileName(name);
        static constexpr bool kVerifyChecksum = true;
        std::string error_msg;
        const ArtDexFileLoader dex_file_loader;
        std::vector<std::unique_ptr<const DexFile>> dex_files;
        bool success = dex_file_loader.Open(filename.c_str(),
                filename.c_str(),
                /* verify */ true,
                kVerifyChecksum,
                &error_msg, &dex_files);
        CHECK(success) << "Failed to open '" << filename << "': " << error_msg;
        for (auto& dex_file : dex_files) {
            CHECK_EQ(PROT_READ, dex_file->GetPermissions());
            CHECK(dex_file->IsReadOnly());
        } 
        return dex_files;
    }
    void OTest::EnsureCompiled(jobject class_loader, const char* class_name, const char* method,
            const char* signature, bool is_virtual)
        REQUIRES(!Locks::mutator_lock_) {
            CompileAll(class_loader);
            Thread::Current()->TransitionFromSuspendedToRunnable();
            bool started = runtime_->Start();
            CHECK(started);
            env_ = Thread::Current()->GetJniEnv();
            jclass class_ = env_->FindClass(class_name);
            CHECK(class_ != nullptr) << "Class not found: " << class_name;
            
            jmethodID mid_;
            if (is_virtual) {
                mid_ = env_->GetMethodID(class_, method, signature);
            } else {
                mid_ = env_->GetStaticMethodID(class_, method, signature);
            }
            CHECK(mid_ != nullptr) << "Method not found: " << class_name << "." << method << signature;
        }
    jobject OTest::LoadDexInPathClassLoader(const std::string& dex_name,
            jobject parent_loader) {
        return LoadDexInWellKnownClassLoader(dex_name,
                WellKnownClasses::dalvik_system_PathClassLoader,
                parent_loader);
    }
    
    jobject OTest::LoadDexInWellKnownClassLoader(const std::string& dex_name,
            jclass loader_class,
            jobject parent_loader) {
        std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles(dex_name.c_str());
        std::vector<const DexFile*> class_path;
        CHECK_NE(0U, dex_files.size());
        for (auto& dex_file : dex_files) {
            class_path.push_back(dex_file.get());
            loaded_dex_files_.push_back(std::move(dex_file));
        }
        Thread* self = Thread::Current();
        ScopedObjectAccess soa(self);
        std::cout << "LoadDexInWellKnownClassLoader\n";       
        jobject result = Runtime::Current()->GetClassLinker()->CreateWellKnownClassLoader(
                self,
                class_path,
                loader_class,
                parent_loader);
        {   
            // Verify we build the correct chain.

            ObjPtr<mirror::ClassLoader> actual_class_loader = soa.Decode<mirror::ClassLoader>(result);
            // Verify that the result has the correct class.
            CHECK_EQ(soa.Decode<mirror::Class>(loader_class), actual_class_loader->GetClass());
            // Verify that the parent is not null. The boot class loader will be set up as a
            // proper object.
            ObjPtr<mirror::ClassLoader> actual_parent(actual_class_loader->GetParent());
            CHECK(actual_parent != nullptr);

            if (parent_loader != nullptr) {
                // We were given a parent. Verify that it's what we expect.
                ObjPtr<mirror::ClassLoader> expected_parent = soa.Decode<mirror::ClassLoader>(parent_loader);
                CHECK_EQ(expected_parent, actual_parent);
            } else {
                // No parent given. The parent must be the BootClassLoader.
                //CHECK(Runtime::Current()->GetClassLinker()->IsBootClassLoader(soa, actual_parent));
            } 
        } 
        return result;
    }
   
   jobject OTest::LoadDex(const char* dex_name) {
        std::cout << "LoadDex()\n";
        jobject class_loader = LoadDexInPathClassLoader(dex_name, nullptr);
        Thread::Current()->SetClassLoaderOverride(class_loader);
        return class_loader;
    }

   TEST_F(OTest, OpenTestDexFiles) {
       Thread* self = Thread::Current();
       jobject class_loader;
       {
           ScopedObjectAccess soa(self);
           class_loader = LoadDex("/mnt/test/2.apk");
       }
       CompileAll(class_loader);
       const std::vector<uint16_t> data =
       {0x0005,
       0x0001,
       0x0000,
       0x0000,
       0x08f7,
       0x0000,
       0x001e,
       0x0000,
       0x4007,
       0x2212,
       0x2101,
       0x0260,
       0x0004,
       0x0313,
       0x0080,
       0x22d4,
       0x0080,
       0x1312,
       0x3233,
       0x0013,
       0x0260,
       0x0003,
       0x0313,
       0x0040,
       0x02dc,
       0x4002,
       0x0239,
       0x000b,
       0x0260,
       0x0003,
       0x1312,
       0x02db,
       0x0102,
       0x0267,
       0x0003,
       0x1212,
       0x2101,
       0x000e};
       TestCode(data, class_loader); 

       ASSERT_NE(class_loader, nullptr);
      
}
}  // namespace art
