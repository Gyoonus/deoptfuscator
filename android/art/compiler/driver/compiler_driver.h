/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_DRIVER_COMPILER_DRIVER_H_
#define ART_COMPILER_DRIVER_COMPILER_DRIVER_H_

#include <atomic>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "android-base/strings.h"

#include "arch/instruction_set.h"
#include "base/array_ref.h"
#include "base/bit_utils.h"
#include "base/mutex.h"
#include "base/os.h"
#include "base/quasi_atomic.h"
#include "base/safe_map.h"
#include "base/timing_logger.h"
#include "class_status.h"
#include "compiler.h"
#include "dex/class_reference.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "dex/dex_to_dex_compiler.h"
#include "dex/method_reference.h"
#include "driver/compiled_method_storage.h"
#include "thread_pool.h"
#include "utils/atomic_dex_ref_map.h"
#include "utils/dex_cache_arrays_layout.h"

namespace art {

namespace mirror {
class Class;
class DexCache;
}  // namespace mirror

namespace verifier {
class MethodVerifier;
class VerifierDepsTest;
}  // namespace verifier

class ArtField;
class BitVector;
class CompiledMethod;
class CompilerOptions;
class DexCompilationUnit;
template<class T> class Handle;
struct InlineIGetIPutData;
class InstructionSetFeatures;
class InternTable;
enum InvokeType : uint32_t;
class MemberOffset;
template<class MirrorType> class ObjPtr;
class ParallelCompilationManager;
class ProfileCompilationInfo;
class ScopedObjectAccess;
template <class Allocator> class SrcMap;
class TimingLogger;
class VdexFile;
class VerificationResults;
class VerifiedMethod;

enum EntryPointCallingConvention {
  // ABI of invocations to a method's interpreter entry point.
  kInterpreterAbi,
  // ABI of calls to a method's native code, only used for native methods.
  kJniAbi,
  // ABI of calls to a method's quick code entry point.
  kQuickAbi
};

class CompilerDriver {
 public:
  // Create a compiler targeting the requested "instruction_set".
  // "image" should be true if image specific optimizations should be
  // enabled.  "image_classes" lets the compiler know what classes it
  // can assume will be in the image, with null implying all available
  // classes.
  CompilerDriver(const CompilerOptions* compiler_options,
                 VerificationResults* verification_results,
                 Compiler::Kind compiler_kind,
                 InstructionSet instruction_set,
                 const InstructionSetFeatures* instruction_set_features,
                 std::unordered_set<std::string>* image_classes,
                 std::unordered_set<std::string>* compiled_classes,
                 std::unordered_set<std::string>* compiled_methods,
                 size_t thread_count,
                 int swap_fd,
                 const ProfileCompilationInfo* profile_compilation_info);

  ~CompilerDriver();

  // Set dex files associated with the oat file being compiled.
  void SetDexFilesForOatFile(const std::vector<const DexFile*>& dex_files);

  // Set dex files classpath.
  void SetClasspathDexFiles(const std::vector<const DexFile*>& dex_files);

  // Get dex files associated with the the oat file being compiled.
  ArrayRef<const DexFile* const> GetDexFilesForOatFile() const {
    return ArrayRef<const DexFile* const>(dex_files_for_oat_file_);
  }

  void CompileAll(jobject class_loader,
                  const std::vector<const DexFile*>& dex_files,
                  TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  // Compile a single Method.
  void CompileOne(Thread* self, ArtMethod* method, TimingLogger* timings)
      REQUIRES_SHARED(Locks::mutator_lock_);

  VerificationResults* GetVerificationResults() const;

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  const InstructionSetFeatures* GetInstructionSetFeatures() const {
    return instruction_set_features_;
  }

  const CompilerOptions& GetCompilerOptions() const {
    return *compiler_options_;
  }

  Compiler* GetCompiler() const {
    return compiler_.get();
  }

  const std::unordered_set<std::string>* GetImageClasses() const {
    return image_classes_.get();
  }

  // Generate the trampolines that are invoked by unresolved direct methods.
  std::unique_ptr<const std::vector<uint8_t>> CreateJniDlsymLookup() const;
  std::unique_ptr<const std::vector<uint8_t>> CreateQuickGenericJniTrampoline() const;
  std::unique_ptr<const std::vector<uint8_t>> CreateQuickImtConflictTrampoline() const;
  std::unique_ptr<const std::vector<uint8_t>> CreateQuickResolutionTrampoline() const;
  std::unique_ptr<const std::vector<uint8_t>> CreateQuickToInterpreterBridge() const;

  ClassStatus GetClassStatus(const ClassReference& ref) const;
  bool GetCompiledClass(const ClassReference& ref, ClassStatus* status) const;

  CompiledMethod* GetCompiledMethod(MethodReference ref) const;
  size_t GetNonRelativeLinkerPatchCount() const;
  // Add a compiled method.
  void AddCompiledMethod(const MethodReference& method_ref,
                         CompiledMethod* const compiled_method,
                         size_t non_relative_linker_patch_count);
  CompiledMethod* RemoveCompiledMethod(const MethodReference& method_ref);

  void SetRequiresConstructorBarrier(Thread* self,
                                     const DexFile* dex_file,
                                     uint16_t class_def_index,
                                     bool requires)
      REQUIRES(!requires_constructor_barrier_lock_);

  // Do the <init> methods for this class require a constructor barrier (prior to the return)?
  // The answer is "yes", if and only if this class has any instance final fields.
  // (This must not be called for any non-<init> methods; the answer would be "no").
  //
  // ---
  //
  // JLS 17.5.1 "Semantics of final fields" mandates that all final fields are frozen at the end
  // of the invoked constructor. The constructor barrier is a conservative implementation means of
  // enforcing the freezes happen-before the object being constructed is observable by another
  // thread.
  //
  // Note: This question only makes sense for instance constructors;
  // static constructors (despite possibly having finals) never need
  // a barrier.
  //
  // JLS 12.4.2 "Detailed Initialization Procedure" approximately describes
  // class initialization as:
  //
  //   lock(class.lock)
  //     class.state = initializing
  //   unlock(class.lock)
  //
  //   invoke <clinit>
  //
  //   lock(class.lock)
  //     class.state = initialized
  //   unlock(class.lock)              <-- acts as a release
  //
  // The last operation in the above example acts as an atomic release
  // for any stores in <clinit>, which ends up being stricter
  // than what a constructor barrier needs.
  //
  // See also QuasiAtomic::ThreadFenceForConstructor().
  bool RequiresConstructorBarrier(Thread* self,
                                  const DexFile* dex_file,
                                  uint16_t class_def_index)
      REQUIRES(!requires_constructor_barrier_lock_);

  // Are runtime access checks necessary in the compiled code?
  bool CanAccessTypeWithoutChecks(ObjPtr<mirror::Class> referrer_class,
                                  ObjPtr<mirror::Class> resolved_class)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Are runtime access and instantiable checks necessary in the code?
  // out_is_finalizable is set to whether the type is finalizable.
  bool CanAccessInstantiableTypeWithoutChecks(ObjPtr<mirror::Class> referrer_class,
                                              ObjPtr<mirror::Class> resolved_class,
                                              bool* out_is_finalizable)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Resolve compiling method's class. Returns null on failure.
  ObjPtr<mirror::Class> ResolveCompilingMethodsClass(const ScopedObjectAccess& soa,
                                                     Handle<mirror::DexCache> dex_cache,
                                                     Handle<mirror::ClassLoader> class_loader,
                                                     const DexCompilationUnit* mUnit)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<mirror::Class> ResolveClass(const ScopedObjectAccess& soa,
                                     Handle<mirror::DexCache> dex_cache,
                                     Handle<mirror::ClassLoader> class_loader,
                                     dex::TypeIndex type_index,
                                     const DexCompilationUnit* mUnit)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Resolve a field. Returns null on failure, including incompatible class change.
  // NOTE: Unlike ClassLinker's ResolveField(), this method enforces is_static.
  ArtField* ResolveField(const ScopedObjectAccess& soa,
                         Handle<mirror::DexCache> dex_cache,
                         Handle<mirror::ClassLoader> class_loader,
                         uint32_t field_idx,
                         bool is_static)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can we fast-path an IGET/IPUT access to an instance field? If yes, compute the field offset.
  std::pair<bool, bool> IsFastInstanceField(ObjPtr<mirror::DexCache> dex_cache,
                                            ObjPtr<mirror::Class> referrer_class,
                                            ArtField* resolved_field,
                                            uint16_t field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Resolve a method. Returns null on failure, including incompatible class change.
  ArtMethod* ResolveMethod(
      ScopedObjectAccess& soa,
      Handle<mirror::DexCache> dex_cache,
      Handle<mirror::ClassLoader> class_loader,
      const DexCompilationUnit* mUnit,
      uint32_t method_idx,
      InvokeType invoke_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ProcessedInstanceField(bool resolved);
  void ProcessedStaticField(bool resolved, bool local);

  // Can we fast path instance field access? Computes field's offset and volatility.
  bool ComputeInstanceFieldInfo(uint32_t field_idx, const DexCompilationUnit* mUnit, bool is_put,
                                MemberOffset* field_offset, bool* is_volatile)
      REQUIRES(!Locks::mutator_lock_);

  ArtField* ComputeInstanceFieldInfo(uint32_t field_idx,
                                     const DexCompilationUnit* mUnit,
                                     bool is_put,
                                     const ScopedObjectAccess& soa)
      REQUIRES_SHARED(Locks::mutator_lock_);


  const VerifiedMethod* GetVerifiedMethod(const DexFile* dex_file, uint32_t method_idx) const;
  bool IsSafeCast(const DexCompilationUnit* mUnit, uint32_t dex_pc);

  bool GetSupportBootImageFixup() const {
    return support_boot_image_fixup_;
  }

  void SetSupportBootImageFixup(bool support_boot_image_fixup) {
    support_boot_image_fixup_ = support_boot_image_fixup;
  }

  void SetCompilerContext(void* compiler_context) {
    compiler_context_ = compiler_context;
  }

  void* GetCompilerContext() const {
    return compiler_context_;
  }

  size_t GetThreadCount() const {
    return parallel_thread_count_;
  }

  void SetDedupeEnabled(bool dedupe_enabled) {
    compiled_method_storage_.SetDedupeEnabled(dedupe_enabled);
  }

  bool DedupeEnabled() const {
    return compiled_method_storage_.DedupeEnabled();
  }

  // Checks if class specified by type_idx is one of the image_classes_
  bool IsImageClass(const char* descriptor) const;

  // Checks whether the provided class should be compiled, i.e., is in classes_to_compile_.
  bool IsClassToCompile(const char* descriptor) const;

  // Checks whether the provided method should be compiled, i.e., is in method_to_compile_.
  bool IsMethodToCompile(const MethodReference& method_ref) const;

  // Checks whether profile guided compilation is enabled and if the method should be compiled
  // according to the profile file.
  bool ShouldCompileBasedOnProfile(const MethodReference& method_ref) const;

  // Checks whether profile guided verification is enabled and if the method should be verified
  // according to the profile file.
  bool ShouldVerifyClassBasedOnProfile(const DexFile& dex_file, uint16_t class_idx) const;

  void RecordClassStatus(const ClassReference& ref, ClassStatus status);

  // Checks if the specified method has been verified without failures. Returns
  // false if the method is not in the verification results (GetVerificationResults).
  bool IsMethodVerifiedWithoutFailures(uint32_t method_idx,
                                       uint16_t class_def_idx,
                                       const DexFile& dex_file) const;

  // Get memory usage during compilation.
  std::string GetMemoryUsageString(bool extended) const;

  void SetHadHardVerifierFailure() {
    had_hard_verifier_failure_ = true;
  }
  void AddSoftVerifierFailure() {
    number_of_soft_verifier_failures_++;
  }

  Compiler::Kind GetCompilerKind() {
    return compiler_kind_;
  }

  CompiledMethodStorage* GetCompiledMethodStorage() {
    return &compiled_method_storage_;
  }

  // Can we assume that the klass is loaded?
  bool CanAssumeClassIsLoaded(mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool MayInline(const DexFile* inlined_from, const DexFile* inlined_into) const {
    if (!kIsTargetBuild) {
      return MayInlineInternal(inlined_from, inlined_into);
    }
    return true;
  }

  const ProfileCompilationInfo* GetProfileCompilationInfo() const {
    return profile_compilation_info_;
  }

  // Is `boot_image_filename` the name of a core image (small boot
  // image used for ART testing only)?
  static bool IsCoreImageFilename(const std::string& boot_image_filename) {
    // Look for "core.art" or "core-*.art".
    if (android::base::EndsWith(boot_image_filename, "core.art")) {
      return true;
    }
    if (!android::base::EndsWith(boot_image_filename, ".art")) {
      return false;
    }
    size_t slash_pos = boot_image_filename.rfind('/');
    if (slash_pos == std::string::npos) {
      return android::base::StartsWith(boot_image_filename, "core-");
    }
    return boot_image_filename.compare(slash_pos + 1, 5u, "core-") == 0;
  }

  optimizer::DexToDexCompiler& GetDexToDexCompiler() {
    return dex_to_dex_compiler_;
  }

 private:
  void PreCompile(jobject class_loader,
                  const std::vector<const DexFile*>& dex_files,
                  TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  void LoadImageClasses(TimingLogger* timings) REQUIRES(!Locks::mutator_lock_);

  // Attempt to resolve all type, methods, fields, and strings
  // referenced from code in the dex file following PathClassLoader
  // ordering semantics.
  void Resolve(jobject class_loader,
               const std::vector<const DexFile*>& dex_files,
               TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);
  void ResolveDexFile(jobject class_loader,
                      const DexFile& dex_file,
                      const std::vector<const DexFile*>& dex_files,
                      ThreadPool* thread_pool,
                      size_t thread_count,
                      TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  // Do fast verification through VerifierDeps if possible. Return whether
  // verification was successful.
  bool FastVerify(jobject class_loader,
                  const std::vector<const DexFile*>& dex_files,
                  TimingLogger* timings);

  void Verify(jobject class_loader,
              const std::vector<const DexFile*>& dex_files,
              TimingLogger* timings);

  void VerifyDexFile(jobject class_loader,
                     const DexFile& dex_file,
                     const std::vector<const DexFile*>& dex_files,
                     ThreadPool* thread_pool,
                     size_t thread_count,
                     TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  void SetVerified(jobject class_loader,
                   const std::vector<const DexFile*>& dex_files,
                   TimingLogger* timings);
  void SetVerifiedDexFile(jobject class_loader,
                          const DexFile& dex_file,
                          const std::vector<const DexFile*>& dex_files,
                          ThreadPool* thread_pool,
                          size_t thread_count,
                          TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  void InitializeClasses(jobject class_loader,
                         const std::vector<const DexFile*>& dex_files,
                         TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);
  void InitializeClasses(jobject class_loader,
                         const DexFile& dex_file,
                         const std::vector<const DexFile*>& dex_files,
                         TimingLogger* timings)
      REQUIRES(!Locks::mutator_lock_);

  void UpdateImageClasses(TimingLogger* timings) REQUIRES(!Locks::mutator_lock_);

  void Compile(jobject class_loader,
               const std::vector<const DexFile*>& dex_files,
               TimingLogger* timings);

  bool MayInlineInternal(const DexFile* inlined_from, const DexFile* inlined_into) const;

  void InitializeThreadPools();
  void FreeThreadPools();
  void CheckThreadPools();

  bool RequiresConstructorBarrier(const DexFile& dex_file, uint16_t class_def_idx) const;

  const CompilerOptions* const compiler_options_;
  VerificationResults* const verification_results_;

  std::unique_ptr<Compiler> compiler_;
  Compiler::Kind compiler_kind_;

  const InstructionSet instruction_set_;
  const InstructionSetFeatures* const instruction_set_features_;

  // All class references that require constructor barriers. If the class reference is not in the
  // set then the result has not yet been computed.
  mutable ReaderWriterMutex requires_constructor_barrier_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::map<ClassReference, bool> requires_constructor_barrier_
      GUARDED_BY(requires_constructor_barrier_lock_);

  // All class references that this compiler has compiled. Indexed by class defs.
  using ClassStateTable = AtomicDexRefMap<ClassReference, ClassStatus>;
  ClassStateTable compiled_classes_;
  // All class references that are in the classpath. Indexed by class defs.
  ClassStateTable classpath_classes_;

  typedef AtomicDexRefMap<MethodReference, CompiledMethod*> MethodTable;

 private:
  // All method references that this compiler has compiled.
  MethodTable compiled_methods_;

  // Number of non-relative patches in all compiled methods. These patches need space
  // in the .oat_patches ELF section if requested in the compiler options.
  Atomic<size_t> non_relative_linker_patch_count_;

  // If image_ is true, specifies the classes that will be included in the image.
  // Note if image_classes_ is null, all classes are included in the image.
  std::unique_ptr<std::unordered_set<std::string>> image_classes_;

  // Specifies the classes that will be compiled. Note that if classes_to_compile_ is null,
  // all classes are eligible for compilation (duplication filters etc. will still apply).
  // This option may be restricted to the boot image, depending on a flag in the implementation.
  std::unique_ptr<std::unordered_set<std::string>> classes_to_compile_;

  // Specifies the methods that will be compiled. Note that if methods_to_compile_ is null,
  // all methods are eligible for compilation (compilation filters etc. will still apply).
  // This option may be restricted to the boot image, depending on a flag in the implementation.
  std::unique_ptr<std::unordered_set<std::string>> methods_to_compile_;

  std::atomic<uint32_t> number_of_soft_verifier_failures_;
  bool had_hard_verifier_failure_;

  // A thread pool that can (potentially) run tasks in parallel.
  std::unique_ptr<ThreadPool> parallel_thread_pool_;
  size_t parallel_thread_count_;

  // A thread pool that guarantees running single-threaded on the main thread.
  std::unique_ptr<ThreadPool> single_thread_pool_;

  class AOTCompilationStats;
  std::unique_ptr<AOTCompilationStats> stats_;

  typedef void (*CompilerCallbackFn)(CompilerDriver& driver);
  typedef MutexLock* (*CompilerMutexLockFn)(CompilerDriver& driver);

  void* compiler_context_;

  bool support_boot_image_fixup_;

  // List of dex files associates with the oat file.
  std::vector<const DexFile*> dex_files_for_oat_file_;

  CompiledMethodStorage compiled_method_storage_;

  // Info for profile guided compilation.
  const ProfileCompilationInfo* const profile_compilation_info_;

  size_t max_arena_alloc_;

  // Compiler for dex to dex (quickening).
  optimizer::DexToDexCompiler dex_to_dex_compiler_;

  friend class CompileClassVisitor;
  friend class DexToDexDecompilerTest;
  friend class verifier::VerifierDepsTest;
  DISALLOW_COPY_AND_ASSIGN(CompilerDriver);
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_DRIVER_H_
