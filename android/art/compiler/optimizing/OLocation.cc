#include <sys/mman.h>
#include <stdio.h>
#include <typeinfo> 

#include "arch/instruction_set_features.h"
#include "art_method.h"
#include "art_method-inl.h"

#include "base/arena_allocator.h"
#include "base/array_slice.h"
#include "base/callee_save_type.h"
#include "base/stl_util.h"
#include "base/timing_logger.h"

#include "builder.h"
#include "class_linker.h"
#include "class_table.h"
#include "code_generator.h"
#include "common_runtime_test.h"
#include "compiler.h"
#include "constant_folding.h"
#include "opaque_location.h"
#include "opaque_clinit.h"
#include "dead_code_elimination.h"

#include "dex/code_item_accessors.h"
#include "dex/verification_results.h"
#include "dex/dex_file.h"
#include "dex/dex_file-inl.h"

#include "driver/compiler_driver.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "driver/dex_compilation_unit.h"

#include "graph_checker.h"
#include "handle_scope-inl.h"

#include "mirror/class_loader.h"
#include "mirror/class-inl.h"

#include "nodes.h"
#include "optimizing_compiler_stats.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"
#include "well_known_classes.h"



namespace art{
class OLocation: public CommonRuntimeTest {

public: 
  void CreateCompilerDriver(Compiler::Kind kind, InstructionSet isa,
      size_t number_of_threads) {
    
    compiler_options_.reset(new CompilerOptions());
    compiler_options_->SetCompilerFilter(CompilerFilter::kQuicken);
    verification_results_.reset(new VerificationResults(compiler_options_.get()));
    compiler_driver_.reset(new CompilerDriver(compiler_options_.get(),
          verification_results_.get(),
          kind,
          isa,
          instruction_set_features_.get(),
          image_classes_.release(),
          compiled_classes_.release(),
          compiled_methods_.release(),
          number_of_threads,
          /* swap_fd */ -1,
          nullptr));
  (void)kind;
  (void)isa;
  (void)number_of_threads;
#if 0
  std::cout << "compiler options : " << runtime_->GetCompilerOptions().size() << std::endl;
  for(auto &option : runtime_->GetCompilerOptions())
  {
    std::cout << option << std::endl;
  }
#endif
    // We typically don't generate an image in unit tests, disable this optimization by default.
    //compiler_driver_->SetSupportBootImageFixup(false);
  }
  void SetUp() {
    CommonRuntimeTest::SetUp();
    {
      ScopedObjectAccess soa(Thread::Current());

      const InstructionSet instruction_set = kRuntimeISA;
      // Take the default set of instruction features from the build.
      instruction_set_features_ = InstructionSetFeatures::FromCppDefines();

      runtime_->SetInstructionSet(instruction_set);
      for (uint32_t i = 0; i < static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType); ++i) {
        CalleeSaveType type = CalleeSaveType(i);
        if (!runtime_->HasCalleeSaveMethod(type)) {
          runtime_->SetCalleeSaveMethod(runtime_->CreateCalleeSaveMethod(), type);
        }
      }
      compilation_stats_.reset(new OptimizingCompilerStats());
      CreateCompilerDriver(Compiler::kOptimizing, kRuntimeISA, 1);
    }
  }
  std::vector<std::unique_ptr<const DexFile>> OpenTestDexFiles(const char* name) {

    std::string filename(name);
    //std::cout << filename << std::endl;
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

  jobject LoadDexInWellKnownClassLoader(const std::string& dex_name,
      jclass loader_class,
      jobject parent_loader) {
    std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles(dex_name.c_str());
    std::vector<const DexFile*> class_path;
    CHECK_NE(0U, dex_files.size());
    for (auto& dex_file : dex_files) {
      class_path.push_back(dex_file.get());
      loaded_dex_files_.push_back(std::move(dex_file));
    }
    dex_files_ = MakeNonOwningPointerVector(loaded_dex_files_);

    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);

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
        CHECK(Runtime::Current()->GetClassLinker()->IsBootClassLoader(soa, actual_parent));
      } 
    }
    return result;
  }
  jobject LoadDexInPathClassLoader(const std::string& dex_name,
      jobject parent_loader) {
    
    return LoadDexInWellKnownClassLoader(dex_name,
        WellKnownClasses::dalvik_system_PathClassLoader,
        parent_loader);
  }

  jobject LoadDex(const char* dex_name) {
    jobject parent_loader = Runtime::Current()->GetSystemClassLoader();
    jobject class_loader = LoadDexInPathClassLoader(dex_name, parent_loader);
    Thread::Current()->SetClassLoaderOverride(class_loader);
    return class_loader;
  }
  void CompileMethod(const char *class_name, 
      ArtMethod& m, 
      jobject cl, uint32_t ref_1, uint32_t ref_2)
  {
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);

    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(cl)));
    mirror::Class* klass = class_linker_->FindClass(self, class_name, class_loader);
    

    uint16_t class_idx = klass->GetDexClassDefIndex();
    uint32_t method_idx = m.GetDexMethodIndex();
    uint32_t access_flag = m.GetAccessFlags();
    uint32_t code_off = m.GetCodeItemOffset();
    const DexFile *dex_file = m.GetDexFile();
    uint32_t is_clinit = 0;
    if(code_off == 0x0 || access_flag & 0x500000 || m.IsConstructor()) //native , abstract, default interface(0x500000)
    {
        if( access_flag & 0x10008 )
            is_clinit = 0x01;
        else
            return;
    }

    //1. HGraph
    ArenaAllocator allocator(runtime_->GetArenaPool());
    ArenaStack arena_stack(runtime_->GetArenaPool());
    //std::cout << dex_file->GetLocation() << std::endl;

    HGraph graph(&allocator,
        &arena_stack,
        *dex_file,
        method_idx,
        kRuntimeISA,
        kInvalidInvokeType,
        false,
        false,
        0);

    //2. DexCompilation Unit
    StackHandleScope<1> hs2(self);
    Handle<mirror::DexCache> dex_cache(hs2.NewHandle(m.GetDexCache()));
    const DexCompilationUnit unit_(
        class_loader,
        class_linker_,
        *dex_file,
        m.GetCodeItem(),
        class_idx,
        method_idx,
        access_flag,
        /* verified_method */ nullptr, // Not needed by the Optimizing compiler.
        dex_cache);
    //3. HGraphBuilder
    //HGraphBuilder

    std::unique_ptr<CodeGenerator> codegen(
        CodeGenerator::Create(&graph,
          kRuntimeISA,
          *(compiler_driver_->GetInstructionSetFeatures()),
          compiler_driver_->GetCompilerOptions(),
          compilation_stats_.get()));

    const CodeItemDebugInfoAccessor code_item_accessor(*dex_file, m.GetCodeItem(), method_idx);
    ArrayRef<const uint8_t> interpreter_metadata;
    interpreter_metadata = m.GetQuickenedInfo();
    VariableSizedHandleScope handles(self);
    HGraphBuilder builder(&graph,
        code_item_accessor,
        &unit_,
        &unit_,
        compiler_driver_.get(),
        codegen.get(),
        compilation_stats_.get(),
        interpreter_metadata,
        &handles);

    if( builder.BuildGraph() != GraphAnalysisResult::kAnalysisSuccess)
      return;

    StringPrettyPrinter printer_before(&graph);
    printer_before.VisitInsertionOrder();
    std::string actual_before = printer_before.str();
    
    
    HConstantFolding(&graph, "constant_folding").Run();
    GraphChecker graph_checker_cf(&graph);
    graph_checker_cf.Run();
    ASSERT_TRUE(graph_checker_cf.IsValid());

    StringPrettyPrinter printer_after_cf(&graph);
    printer_after_cf.VisitInsertionOrder();
    std::string actual_after_cf = printer_after_cf.str();
    
    HDeadCodeElimination(&graph, nullptr /* stats */, "dead_code_elimination").Run();
    GraphChecker graph_checker_dce(&graph);
    graph_checker_dce.Run();
    ASSERT_TRUE(graph_checker_dce.IsValid());
    RemoveSuspendChecks(&graph);
    if(is_clinit)
    {
       HOpaqueClinit(&graph, "opaque_clinit").Run(ref_1, ref_2, code_off);
    }
    else
        HOpaqueLocation(&graph, "opaque_location").Run(ref_1, ref_2, code_off);
  }

  void CompileAll(jobject cl) {

    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(cl)));
    
    for (auto &dex_file_ : loaded_dex_files_)
    {
      uint32_t num_class_defs = dex_file_->NumClassDefs();
      for(uint32_t i = 0; i < num_class_defs; ++i)
      {
        const DexFile::ClassDef& class_def = dex_file_->GetClassDef(i);

        const char *class_descriptor = dex_file_->GetClassDescriptor(class_def);
               mirror::Class* klass = class_linker_->FindClass(self, class_descriptor, class_loader);
        if (klass == nullptr)
        {
          DCHECK(Thread::Current()->IsExceptionPending());
          Thread::Current()->ClearException();
          continue;
        }

        if (klass->IsAbstract() || klass->IsBootStrapClassLoaded())
          continue;
 
        auto pointer_size = class_linker_->GetImagePointerSize();
        for (auto& m : klass->GetMethods(pointer_size)) {
          CompileMethod(class_descriptor, m, cl, 1, 2);
        }
      }
    }
  }
  //Compile
  void CompileClass(jobject cl, const char* class_descriptor, uint32_t ref_1, uint32_t ref_2) {

    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(cl)));
    mirror::Class* klass = class_linker_->FindClass(self, class_descriptor, class_loader);
    if (klass == nullptr)
    {
      DCHECK(Thread::Current()->IsExceptionPending());
      Thread::Current()->ClearException();
      return;
    }

    if (klass->IsAbstract() || klass->IsBootStrapClassLoaded())
      return;
    
    auto pointer_size = class_linker_->GetImagePointerSize();
    
    for (auto& m : klass->GetMethods(pointer_size)) 
    {
      CompileMethod(class_descriptor, m, cl, ref_1, ref_2);
    }
  }

  std::vector<std::unique_ptr<const DexFile>> loaded_dex_files_;
  std::vector<const DexFile*> dex_files_;
  Compiler::Kind compiler_kind_ = Compiler::kOptimizing;
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;
  std::unique_ptr<std::unordered_set<std::string>> compiled_classes_;
  std::unique_ptr<std::unordered_set<std::string>> compiled_methods_;
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<CompilerDriver> compiler_driver_;
  std::unique_ptr<VerificationResults> verification_results_;
  std::unique_ptr<std::unordered_set<std::string>> image_classes_;
  std::unique_ptr<OptimizingCompilerStats> compilation_stats_; 

};

TEST_F(OLocation, CCC)
{

  std::string app_name, class_name;
  uint32_t ref_1, ref_2;
  std::cin >> app_name >> class_name >> ref_1 >> ref_2;
  //std::cout << runtime_ << std::endl;
  jobject cl;
  { 
    ScopedObjectAccess soa(Thread::Current());
    cl = LoadDex(app_name.c_str());
  }
  TimingLogger timings("OLOCATION::CCC", false, false);
  std::vector<const DexFile*> dex_files = GetDexFiles(cl);
  
  for ( auto& de : dex_files)
  {
    ScopedObjectAccess soa(Thread::Current()); 
    class_linker_->RegisterDexFile(*de, soa.Decode<mirror::ClassLoader>(cl));
  }
  compiler_driver_->SetDexFilesForOatFile(dex_files);
  


  //CompileAll(cl);
  //class_name <-- array
  CompileClass(cl, class_name.c_str(), ref_1, ref_2);
  #if 0
  ScopedObjectAccess soa(self);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(cl)));
  std::string class_name;
  std::cin >> class_name;
  mirror::Class* clazz = Runtime::Current()->GetClassLinker()->FindClass(soa.Self(), class_name.c_str(), class_loader);
  
  if( clazz == nullptr)
  {
    std::cout << "no clazz\n";
  }
  (void)clazz;
 #endif
  {
    //std::cout << dex_files_.size()<<std::endl;
    //std::cout << loaded_dex_files_.size()<<std::endl;
    //std::cout << boot_class_path_.size();
    //std::cout << Runtime::Current()->GetImageLocation() << std::endl;
    //for (const std::string &core_dex_file_name : GetLibCoreDexFileNames()) {
      //std::cout << core_dex_file_name << std::endl;
    //}
    
   
  }
  //std::cout << "compiler options : " << runtime_->GetCompilerOptions().size() << std::endl;
  
}


} //art
