/*
 * Copyright (C) 2016 The Android Open Source Project
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

// Test is in compiler, as it uses compiler related code.
#include "verifier/verifier_deps.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "common_compiler_test.h"
#include "compiler_callbacks.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "dex/verification_results.h"
#include "dex/verified_method.h"
#include "driver/compiler_driver-inl.h"
#include "driver/compiler_options.h"
#include "handle_scope-inl.h"
#include "indenter.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "utils/atomic_dex_ref_map-inl.h"
#include "verifier/method_verifier-inl.h"

namespace art {
namespace verifier {

class VerifierDepsCompilerCallbacks : public CompilerCallbacks {
 public:
  VerifierDepsCompilerCallbacks()
      : CompilerCallbacks(CompilerCallbacks::CallbackMode::kCompileApp),
        deps_(nullptr) {}

  void MethodVerified(verifier::MethodVerifier* verifier ATTRIBUTE_UNUSED) OVERRIDE {}
  void ClassRejected(ClassReference ref ATTRIBUTE_UNUSED) OVERRIDE {}
  bool IsRelocationPossible() OVERRIDE { return false; }

  verifier::VerifierDeps* GetVerifierDeps() const OVERRIDE { return deps_; }
  void SetVerifierDeps(verifier::VerifierDeps* deps) { deps_ = deps; }

 private:
  verifier::VerifierDeps* deps_;
};

class VerifierDepsTest : public CommonCompilerTest {
 public:
  void SetUpRuntimeOptions(RuntimeOptions* options) {
    CommonCompilerTest::SetUpRuntimeOptions(options);
    callbacks_.reset(new VerifierDepsCompilerCallbacks());
  }

  mirror::Class* FindClassByName(const std::string& name, ScopedObjectAccess* soa)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa->Decode<mirror::ClassLoader>(class_loader_)));
    mirror::Class* klass = class_linker_->FindClass(Thread::Current(),
                                                    name.c_str(),
                                                    class_loader_handle);
    if (klass == nullptr) {
      DCHECK(Thread::Current()->IsExceptionPending());
      Thread::Current()->ClearException();
    }
    return klass;
  }

  void SetupCompilerDriver() {
    compiler_options_->boot_image_ = false;
    compiler_driver_->InitializeThreadPools();
  }

  void VerifyWithCompilerDriver(verifier::VerifierDeps* deps) {
    TimingLogger timings("Verify", false, false);
    // The compiler driver handles the verifier deps in the callbacks, so
    // remove what this class did for unit testing.
    if (deps == nullptr) {
      // Create some verifier deps by default if they are not already specified.
      deps = new verifier::VerifierDeps(dex_files_);
      verifier_deps_.reset(deps);
    }
    callbacks_->SetVerifierDeps(deps);
    compiler_driver_->Verify(class_loader_, dex_files_, &timings);
    callbacks_->SetVerifierDeps(nullptr);
    // Clear entries in the verification results to avoid hitting a DCHECK that
    // we always succeed inserting a new entry after verifying.
    AtomicDexRefMap<MethodReference, const VerifiedMethod*>* map =
        &compiler_driver_->GetVerificationResults()->atomic_verified_methods_;
    map->Visit([](const DexFileReference& ref ATTRIBUTE_UNUSED, const VerifiedMethod* method) {
      delete method;
    });
    map->ClearEntries();
  }

  void SetVerifierDeps(const std::vector<const DexFile*>& dex_files) {
    verifier_deps_.reset(new verifier::VerifierDeps(dex_files));
    VerifierDepsCompilerCallbacks* callbacks =
        reinterpret_cast<VerifierDepsCompilerCallbacks*>(callbacks_.get());
    callbacks->SetVerifierDeps(verifier_deps_.get());
  }

  void LoadDexFile(ScopedObjectAccess* soa, const char* name1, const char* name2 = nullptr)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    class_loader_ = (name2 == nullptr) ? LoadDex(name1) : LoadMultiDex(name1, name2);
    dex_files_ = GetDexFiles(class_loader_);
    primary_dex_file_ = dex_files_.front();

    SetVerifierDeps(dex_files_);
    StackHandleScope<1> hs(soa->Self());
    Handle<mirror::ClassLoader> loader =
        hs.NewHandle(soa->Decode<mirror::ClassLoader>(class_loader_));
    for (const DexFile* dex_file : dex_files_) {
      class_linker_->RegisterDexFile(*dex_file, loader.Get());
    }
    for (const DexFile* dex_file : dex_files_) {
      compiler_driver_->GetVerificationResults()->AddDexFile(dex_file);
    }
    compiler_driver_->SetDexFilesForOatFile(dex_files_);
  }

  void LoadDexFile(ScopedObjectAccess* soa) REQUIRES_SHARED(Locks::mutator_lock_) {
    LoadDexFile(soa, "VerifierDeps");
    CHECK_EQ(dex_files_.size(), 1u);
    klass_Main_ = FindClassByName("LMain;", soa);
    CHECK(klass_Main_ != nullptr);
  }

  bool VerifyMethod(const std::string& method_name) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);

    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader_)));
    Handle<mirror::DexCache> dex_cache_handle(hs.NewHandle(klass_Main_->GetDexCache()));

    const DexFile::ClassDef* class_def = klass_Main_->GetClassDef();
    const uint8_t* class_data = primary_dex_file_->GetClassData(*class_def);
    CHECK(class_data != nullptr);

    ClassDataItemIterator it(*primary_dex_file_, class_data);
    it.SkipAllFields();

    ArtMethod* method = nullptr;
    while (it.HasNextDirectMethod()) {
      ArtMethod* resolved_method =
          class_linker_->ResolveMethod<ClassLinker::ResolveMode::kNoChecks>(
              it.GetMemberIndex(),
              dex_cache_handle,
              class_loader_handle,
              /* referrer */ nullptr,
              it.GetMethodInvokeType(*class_def));
      CHECK(resolved_method != nullptr);
      if (method_name == resolved_method->GetName()) {
        method = resolved_method;
        break;
      }
      it.Next();
    }
    CHECK(method != nullptr);

    Thread::Current()->SetVerifierDeps(callbacks_->GetVerifierDeps());
    MethodVerifier verifier(Thread::Current(),
                            primary_dex_file_,
                            dex_cache_handle,
                            class_loader_handle,
                            *class_def,
                            it.GetMethodCodeItem(),
                            it.GetMemberIndex(),
                            method,
                            it.GetMethodAccessFlags(),
                            true /* can_load_classes */,
                            true /* allow_soft_failures */,
                            true /* need_precise_constants */,
                            false /* verify to dump */,
                            true /* allow_thread_suspension */);
    verifier.Verify();
    Thread::Current()->SetVerifierDeps(nullptr);
    return !verifier.HasFailures();
  }

  void VerifyDexFile(const char* multidex = nullptr) {
    {
      ScopedObjectAccess soa(Thread::Current());
      LoadDexFile(&soa, "VerifierDeps", multidex);
    }
    SetupCompilerDriver();
    VerifyWithCompilerDriver(/* verifier_deps */ nullptr);
  }

  bool TestAssignabilityRecording(const std::string& dst,
                                  const std::string& src,
                                  bool is_strict,
                                  bool is_assignable) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);
    mirror::Class* klass_dst = FindClassByName(dst, &soa);
    DCHECK(klass_dst != nullptr) << dst;
    mirror::Class* klass_src = FindClassByName(src, &soa);
    DCHECK(klass_src != nullptr) << src;
    verifier_deps_->AddAssignability(*primary_dex_file_,
                                     klass_dst,
                                     klass_src,
                                     is_strict,
                                     is_assignable);
    return true;
  }

  // Check that the status of classes in `class_loader_` match the
  // expected status in `deps`.
  void VerifyClassStatus(const verifier::VerifierDeps& deps) {
    ScopedObjectAccess soa(Thread::Current());
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader_)));
    MutableHandle<mirror::Class> cls(hs.NewHandle<mirror::Class>(nullptr));
    for (const DexFile* dex_file : dex_files_) {
      const std::set<dex::TypeIndex>& unverified_classes = deps.GetUnverifiedClasses(*dex_file);
      for (uint32_t i = 0; i < dex_file->NumClassDefs(); ++i) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
        const char* descriptor = dex_file->GetClassDescriptor(class_def);
        cls.Assign(class_linker_->FindClass(soa.Self(), descriptor, class_loader_handle));
        if (cls == nullptr) {
          CHECK(soa.Self()->IsExceptionPending());
          soa.Self()->ClearException();
        } else if (unverified_classes.find(class_def.class_idx_) == unverified_classes.end()) {
          ASSERT_EQ(cls->GetStatus(), ClassStatus::kVerified);
        } else {
          ASSERT_LT(cls->GetStatus(), ClassStatus::kVerified);
        }
      }
    }
  }

  bool HasUnverifiedClass(const std::string& cls) {
    return HasUnverifiedClass(cls, *primary_dex_file_);
  }

  bool HasUnverifiedClass(const std::string& cls, const DexFile& dex_file) {
    const DexFile::TypeId* type_id = dex_file.FindTypeId(cls.c_str());
    DCHECK(type_id != nullptr);
    dex::TypeIndex index = dex_file.GetIndexForTypeId(*type_id);
    for (const auto& dex_dep : verifier_deps_->dex_deps_) {
      for (dex::TypeIndex entry : dex_dep.second->unverified_classes_) {
        if (index == entry) {
          return true;
        }
      }
    }
    return false;
  }

  // Iterates over all assignability records and tries to find an entry which
  // matches the expected destination/source pair.
  bool HasAssignable(const std::string& expected_destination,
                     const std::string& expected_source,
                     bool expected_is_assignable) {
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      const DexFile& dex_file = *dex_dep.first;
      auto& storage = expected_is_assignable ? dex_dep.second->assignable_types_
                                             : dex_dep.second->unassignable_types_;
      for (auto& entry : storage) {
        std::string actual_destination =
            verifier_deps_->GetStringFromId(dex_file, entry.GetDestination());
        std::string actual_source = verifier_deps_->GetStringFromId(dex_file, entry.GetSource());
        if ((expected_destination == actual_destination) && (expected_source == actual_source)) {
          return true;
        }
      }
    }
    return false;
  }

  // Iterates over all class resolution records, finds an entry which matches
  // the given class descriptor and tests its properties.
  bool HasClass(const std::string& expected_klass,
                bool expected_resolved,
                const std::string& expected_access_flags = "") {
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      for (auto& entry : dex_dep.second->classes_) {
        if (expected_resolved != entry.IsResolved()) {
          continue;
        }

        std::string actual_klass = dex_dep.first->StringByTypeIdx(entry.GetDexTypeIndex());
        if (expected_klass != actual_klass) {
          continue;
        }

        if (expected_resolved) {
          // Test access flags. Note that PrettyJavaAccessFlags always appends
          // a space after the modifiers. Add it to the expected access flags.
          std::string actual_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
          if (expected_access_flags + " " != actual_access_flags) {
            continue;
          }
        }

        return true;
      }
    }
    return false;
  }

  // Iterates over all field resolution records, finds an entry which matches
  // the given field class+name+type and tests its properties.
  bool HasField(const std::string& expected_klass,
                const std::string& expected_name,
                const std::string& expected_type,
                bool expected_resolved,
                const std::string& expected_access_flags = "",
                const std::string& expected_decl_klass = "") {
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      for (auto& entry : dex_dep.second->fields_) {
        if (expected_resolved != entry.IsResolved()) {
          continue;
        }

        const DexFile::FieldId& field_id = dex_dep.first->GetFieldId(entry.GetDexFieldIndex());

        std::string actual_klass = dex_dep.first->StringByTypeIdx(field_id.class_idx_);
        if (expected_klass != actual_klass) {
          continue;
        }

        std::string actual_name = dex_dep.first->StringDataByIdx(field_id.name_idx_);
        if (expected_name != actual_name) {
          continue;
        }

        std::string actual_type = dex_dep.first->StringByTypeIdx(field_id.type_idx_);
        if (expected_type != actual_type) {
          continue;
        }

        if (expected_resolved) {
          // Test access flags. Note that PrettyJavaAccessFlags always appends
          // a space after the modifiers. Add it to the expected access flags.
          std::string actual_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
          if (expected_access_flags + " " != actual_access_flags) {
            continue;
          }

          std::string actual_decl_klass = verifier_deps_->GetStringFromId(
              *dex_dep.first, entry.GetDeclaringClassIndex());
          if (expected_decl_klass != actual_decl_klass) {
            continue;
          }
        }

        return true;
      }
    }
    return false;
  }

  // Iterates over all method resolution records, finds an entry which matches
  // the given field kind+class+name+signature and tests its properties.
  bool HasMethod(const std::string& expected_klass,
                 const std::string& expected_name,
                 const std::string& expected_signature,
                 bool expected_resolved,
                 const std::string& expected_access_flags = "",
                 const std::string& expected_decl_klass = "") {
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      for (const VerifierDeps::MethodResolution& entry : dex_dep.second->methods_) {
        if (expected_resolved != entry.IsResolved()) {
          continue;
        }

        const DexFile::MethodId& method_id = dex_dep.first->GetMethodId(entry.GetDexMethodIndex());

        std::string actual_klass = dex_dep.first->StringByTypeIdx(method_id.class_idx_);
        if (expected_klass != actual_klass) {
          continue;
        }

        std::string actual_name = dex_dep.first->StringDataByIdx(method_id.name_idx_);
        if (expected_name != actual_name) {
          continue;
        }

        std::string actual_signature = dex_dep.first->GetMethodSignature(method_id).ToString();
        if (expected_signature != actual_signature) {
          continue;
        }

        if (expected_resolved) {
          // Test access flags. Note that PrettyJavaAccessFlags always appends
          // a space after the modifiers. Add it to the expected access flags.
          std::string actual_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
          if (expected_access_flags + " " != actual_access_flags) {
            continue;
          }

          std::string actual_decl_klass = verifier_deps_->GetStringFromId(
              *dex_dep.first, entry.GetDeclaringClassIndex());
          if (expected_decl_klass != actual_decl_klass) {
            continue;
          }
        }

        return true;
      }
    }
    return false;
  }

  size_t NumberOfCompiledDexFiles() {
    return verifier_deps_->dex_deps_.size();
  }

  size_t HasEachKindOfRecord() {
    bool has_strings = false;
    bool has_assignability = false;
    bool has_classes = false;
    bool has_fields = false;
    bool has_methods = false;
    bool has_unverified_classes = false;

    for (auto& entry : verifier_deps_->dex_deps_) {
      has_strings |= !entry.second->strings_.empty();
      has_assignability |= !entry.second->assignable_types_.empty();
      has_assignability |= !entry.second->unassignable_types_.empty();
      has_classes |= !entry.second->classes_.empty();
      has_fields |= !entry.second->fields_.empty();
      has_methods |= !entry.second->methods_.empty();
      has_unverified_classes |= !entry.second->unverified_classes_.empty();
    }

    return has_strings &&
           has_assignability &&
           has_classes &&
           has_fields &&
           has_methods &&
           has_unverified_classes;
  }

  std::unique_ptr<verifier::VerifierDeps> verifier_deps_;
  std::vector<const DexFile*> dex_files_;
  const DexFile* primary_dex_file_;
  jobject class_loader_;
  mirror::Class* klass_Main_;
};

TEST_F(VerifierDepsTest, StringToId) {
  ScopedObjectAccess soa(Thread::Current());
  LoadDexFile(&soa);

  dex::StringIndex id_Main1 = verifier_deps_->GetIdFromString(*primary_dex_file_, "LMain;");
  ASSERT_LT(id_Main1.index_, primary_dex_file_->NumStringIds());
  ASSERT_EQ("LMain;", verifier_deps_->GetStringFromId(*primary_dex_file_, id_Main1));

  dex::StringIndex id_Main2 = verifier_deps_->GetIdFromString(*primary_dex_file_, "LMain;");
  ASSERT_LT(id_Main2.index_, primary_dex_file_->NumStringIds());
  ASSERT_EQ("LMain;", verifier_deps_->GetStringFromId(*primary_dex_file_, id_Main2));

  dex::StringIndex id_Lorem1 = verifier_deps_->GetIdFromString(*primary_dex_file_, "Lorem ipsum");
  ASSERT_GE(id_Lorem1.index_, primary_dex_file_->NumStringIds());
  ASSERT_EQ("Lorem ipsum", verifier_deps_->GetStringFromId(*primary_dex_file_, id_Lorem1));

  dex::StringIndex id_Lorem2 = verifier_deps_->GetIdFromString(*primary_dex_file_, "Lorem ipsum");
  ASSERT_GE(id_Lorem2.index_, primary_dex_file_->NumStringIds());
  ASSERT_EQ("Lorem ipsum", verifier_deps_->GetStringFromId(*primary_dex_file_, id_Lorem2));

  ASSERT_EQ(id_Main1, id_Main2);
  ASSERT_EQ(id_Lorem1, id_Lorem2);
  ASSERT_NE(id_Main1, id_Lorem1);
}

TEST_F(VerifierDepsTest, Assignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/net/Socket;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/net/Socket;", "Ljavax/net/ssl/SSLSocket;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot3) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/Collection;",
                                         /* src */ "LMyThreadSet;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/Collection;", "Ljava/util/Set;", true));
}

TEST_F(VerifierDepsTest, Assignable_BothArrays_Resolved) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[[Ljava/util/TimeZone;",
                                         /* src */ "[[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  // If the component types of both arrays are resolved, we optimize the list of
  // dependencies by recording a dependency on the component types.
  ASSERT_FALSE(HasAssignable("[[Ljava/util/TimeZone;", "[[Ljava/util/SimpleTimeZone;", true));
  ASSERT_FALSE(HasAssignable("[Ljava/util/TimeZone;", "[Ljava/util/SimpleTimeZone;", true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, NotAssignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/util/SimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljavax/net/ssl/SSLSocket;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/util/SimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_BothArrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[Ljava/lang/Exception;",
                                         /* src */ "[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/util/SimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, ArgumentType_ResolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedClass"));
  ASSERT_TRUE(HasClass("Ljava/lang/Thread;", true, "public"));
}

TEST_F(VerifierDepsTest, ArgumentType_UnresolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedClass"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, ArgumentType_UnresolvedSuper) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedSuper"));
  ASSERT_TRUE(HasClass("LMySetWithUnresolvedSuper;", false));
}

TEST_F(VerifierDepsTest, ReturnType_Reference) {
  ASSERT_TRUE(VerifyMethod("ReturnType_Reference"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/lang/IllegalStateException;", true));
}

TEST_F(VerifierDepsTest, ReturnType_Array) {
  ASSERT_FALSE(VerifyMethod("ReturnType_Array"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Integer;", "Ljava/lang/IllegalStateException;", false));
}

TEST_F(VerifierDepsTest, InvokeArgumentType) {
  ASSERT_TRUE(VerifyMethod("InvokeArgumentType"));
  ASSERT_TRUE(HasClass("Ljava/text/SimpleDateFormat;", true, "public"));
  ASSERT_TRUE(HasClass("Ljava/util/SimpleTimeZone;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/text/SimpleDateFormat;",
                        "setTimeZone",
                        "(Ljava/util/TimeZone;)V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/text/DateFormat;"));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, MergeTypes_RegisterLines) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_RegisterLines"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
}

TEST_F(VerifierDepsTest, MergeTypes_IfInstanceOf) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_IfInstanceOf"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/net/SocketTimeoutException;", "Ljava/lang/Exception;", false));
}

TEST_F(VerifierDepsTest, MergeTypes_Unresolved) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_Unresolved"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
}

TEST_F(VerifierDepsTest, ConstClass_Resolved) {
  ASSERT_TRUE(VerifyMethod("ConstClass_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", true, "public"));
}

TEST_F(VerifierDepsTest, ConstClass_Unresolved) {
  ASSERT_FALSE(VerifyMethod("ConstClass_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, CheckCast_Resolved) {
  ASSERT_TRUE(VerifyMethod("CheckCast_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", true, "public"));
}

TEST_F(VerifierDepsTest, CheckCast_Unresolved) {
  ASSERT_FALSE(VerifyMethod("CheckCast_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, InstanceOf_Resolved) {
  ASSERT_TRUE(VerifyMethod("InstanceOf_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", true, "public"));
}

TEST_F(VerifierDepsTest, InstanceOf_Unresolved) {
  ASSERT_FALSE(VerifyMethod("InstanceOf_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, NewInstance_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewInstance_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", true, "public"));
}

TEST_F(VerifierDepsTest, NewInstance_Unresolved) {
  ASSERT_FALSE(VerifyMethod("NewInstance_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, NewArray_Unresolved) {
  ASSERT_FALSE(VerifyMethod("NewArray_Unresolved"));
  ASSERT_TRUE(HasClass("[LUnresolvedClass;", false));
}

TEST_F(VerifierDepsTest, Throw) {
  ASSERT_TRUE(VerifyMethod("Throw"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/lang/IllegalStateException;", true));
}

TEST_F(VerifierDepsTest, MoveException_Resolved) {
  ASSERT_TRUE(VerifyMethod("MoveException_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", true, "public"));
  ASSERT_TRUE(HasClass("Ljava/net/SocketTimeoutException;", true, "public"));
  ASSERT_TRUE(HasClass("Ljava/util/zip/ZipException;", true, "public"));

  // Testing that all exception types are assignable to Throwable.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/io/InterruptedIOException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/util/zip/ZipException;", true));

  // Testing that the merge type is assignable to Throwable.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/io/IOException;", true));

  // Merging of exception types.
  ASSERT_TRUE(HasAssignable("Ljava/io/IOException;", "Ljava/io/InterruptedIOException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/io/IOException;", "Ljava/util/zip/ZipException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, MoveException_Unresolved) {
  ASSERT_FALSE(VerifyMethod("MoveException_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedException;", false));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/System;", true, "public"));
  ASSERT_TRUE(HasField("Ljava/lang/System;",
                       "out",
                       "Ljava/io/PrintStream;",
                       true,
                       "public static",
                       "Ljava/lang/System;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/util/SimpleTimeZone;", true, "public"));
  ASSERT_TRUE(HasField(
      "Ljava/util/SimpleTimeZone;", "LONG", "I", true, "public static", "Ljava/util/TimeZone;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasField(
      "LMySimpleTimeZone;", "SHORT", "I", true, "public static", "Ljava/util/TimeZone;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface1"));
  ASSERT_TRUE(HasClass("Ljavax/xml/transform/dom/DOMResult;", true, "public"));
  ASSERT_TRUE(HasField("Ljavax/xml/transform/dom/DOMResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       true,
                       "public static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface2"));
  ASSERT_TRUE(HasField("LMyDOMResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       true,
                       "public static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface3) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface3"));
  ASSERT_TRUE(HasField("LMyResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       true,
                       "public static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface4) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface4"));
  ASSERT_TRUE(HasField("LMyDocument;",
                       "ELEMENT_NODE",
                       "S",
                       true,
                       "public static",
                       "Lorg/w3c/dom/Node;"));
}

TEST_F(VerifierDepsTest, StaticField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasClass("Ljava/util/TimeZone;", true, "public"));
  ASSERT_TRUE(HasField("Ljava/util/TimeZone;", "x", "I", false));
}

TEST_F(VerifierDepsTest, StaticField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasField("LMyThreadSet;", "x", "I", false));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", true, "public"));
  ASSERT_TRUE(HasField("Ljava/io/InterruptedIOException;",
                       "bytesTransferred",
                       "I",
                       true,
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/net/SocketTimeoutException;", true, "public"));
  ASSERT_TRUE(HasField("Ljava/net/SocketTimeoutException;",
                       "bytesTransferred",
                       "I",
                       true,
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasField("LMySocketTimeoutException;",
                       "bytesTransferred",
                       "I",
                       true,
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", true, "public"));
  ASSERT_TRUE(HasField("Ljava/io/InterruptedIOException;", "x", "I", false));
}

TEST_F(VerifierDepsTest, InstanceField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasField("LMyThreadSet;", "x", "I", false));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/net/Socket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/net/Socket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        /* expect_resolved */ true,
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljavax/net/ssl/SSLSocket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        /* expect_resolved */ true,
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod("LMySSLSocket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        /* expect_resolved */ true,
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_DeclaredInInterface1"));
  ASSERT_TRUE(HasClass("Ljava/util/Map$Entry;", true, "public interface"));
  ASSERT_TRUE(HasMethod("Ljava/util/Map$Entry;",
                        "comparingByKey",
                        "()Ljava/util/Comparator;",
                        /* expect_resolved */ true,
                        "public static",
                        "Ljava/util/Map$Entry;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_DeclaredInInterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_DeclaredInInterface2"));
  ASSERT_TRUE(HasClass("Ljava/util/AbstractMap$SimpleEntry;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/util/AbstractMap$SimpleEntry;",
                        "comparingByKey",
                        "()Ljava/util/Comparator;",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeStatic_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljavax/net/ssl/SSLSocket;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeStatic_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved2"));
  ASSERT_TRUE(HasMethod("LMySSLSocket;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeDirect_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/net/Socket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/net/Socket;",
                        "<init>",
                        "()V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInSuperclass1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljavax/net/ssl/SSLSocket;",
                        "checkOldImpl",
                        "()V",
                        /* expect_resolved */ true,
                        "private",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInSuperclass2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod("LMySSLSocket;",
                        "checkOldImpl",
                        "()V",
                        /* expect_resolved */ true,
                        "private",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljavax/net/ssl/SSLSocket;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeDirect_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved2"));
  ASSERT_TRUE(HasMethod("LMySSLSocket;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/Throwable;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/lang/Throwable;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/io/InterruptedIOException;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod("LMySocketTimeoutException;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Throwable;"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperinterface) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperinterface"));
  ASSERT_TRUE(HasMethod("LMyThreadSet;",
                        "size",
                        "()I",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/util/Set;"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", true, "public"));
  ASSERT_TRUE(HasMethod("Ljava/io/InterruptedIOException;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved2"));
  ASSERT_TRUE(HasMethod("LMySocketTimeoutException;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeInterface_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", true, "public interface"));
  ASSERT_TRUE(HasMethod("Ljava/lang/Runnable;",
                        "run",
                        "()V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Runnable;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperclass) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperclass"));
  // TODO: Maybe we should not record dependency if the invoke type does not match the lookup type.
  ASSERT_TRUE(HasMethod("LMyThread;",
                        "join",
                        "()V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Thread;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperinterface1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface1"));
  // TODO: Maybe we should not record dependency if the invoke type does not match the lookup type.
  ASSERT_TRUE(HasMethod("LMyThreadSet;",
                        "run",
                        "()V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Thread;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperinterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface2"));
  ASSERT_TRUE(HasMethod("LMyThreadSet;",
                        "isEmpty",
                        "()Z",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/util/Set;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", true, "public interface"));
  ASSERT_TRUE(HasMethod("Ljava/lang/Runnable;",
                        "x",
                        "()V",
                        /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeInterface_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved2"));
  ASSERT_TRUE(HasMethod("LMyThreadSet;", "x", "()V", /* expect_resolved */ false));
}

TEST_F(VerifierDepsTest, InvokeSuper_ThisAssignable) {
  ASSERT_TRUE(VerifyMethod("InvokeSuper_ThisAssignable"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", true, "public interface"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Runnable;", "Ljava/lang/Thread;", true));
  ASSERT_TRUE(HasMethod("Ljava/lang/Runnable;",
                        "run",
                        "()V",
                        /* expect_resolved */ true,
                        "public",
                        "Ljava/lang/Runnable;"));
}

TEST_F(VerifierDepsTest, InvokeSuper_ThisNotAssignable) {
  ASSERT_FALSE(VerifyMethod("InvokeSuper_ThisNotAssignable"));
  ASSERT_TRUE(HasClass("Ljava/lang/Integer;", true, "public"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Integer;", "Ljava/lang/Thread;", false));
  ASSERT_TRUE(HasMethod("Ljava/lang/Integer;",
                        "intValue", "()I",
                        /* expect_resolved */ true,
                        "public", "Ljava/lang/Integer;"));
}

TEST_F(VerifierDepsTest, ArgumentType_ResolvedReferenceArray) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedReferenceArray"));
  ASSERT_TRUE(HasClass("[Ljava/lang/Thread;", true, "public"));
}

TEST_F(VerifierDepsTest, NewArray_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewArray_Resolved"));
  ASSERT_TRUE(HasClass("[Ljava/lang/IllegalStateException;", true, "public"));
}

TEST_F(VerifierDepsTest, EncodeDecode) {
  VerifyDexFile();

  ASSERT_EQ(1u, NumberOfCompiledDexFiles());
  ASSERT_TRUE(HasEachKindOfRecord());

  std::vector<uint8_t> buffer;
  verifier_deps_->Encode(dex_files_, &buffer);
  ASSERT_FALSE(buffer.empty());

  VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
  ASSERT_TRUE(verifier_deps_->Equals(decoded_deps));
}

TEST_F(VerifierDepsTest, EncodeDecodeMulti) {
  VerifyDexFile("MultiDex");

  ASSERT_GT(NumberOfCompiledDexFiles(), 1u);
  std::vector<uint8_t> buffer;
  verifier_deps_->Encode(dex_files_, &buffer);
  ASSERT_FALSE(buffer.empty());

  // Create new DexFile, to mess with std::map order: the verifier deps used
  // to iterate over the map, which doesn't guarantee insertion order. We fixed
  // this by passing the expected order when encoding/decoding.
  std::vector<std::unique_ptr<const DexFile>> first_dex_files = OpenTestDexFiles("VerifierDeps");
  std::vector<std::unique_ptr<const DexFile>> second_dex_files = OpenTestDexFiles("MultiDex");
  std::vector<const DexFile*> dex_files;
  for (auto& dex_file : first_dex_files) {
    dex_files.push_back(dex_file.get());
  }
  for (auto& dex_file : second_dex_files) {
    dex_files.push_back(dex_file.get());
  }

  // Dump the new verifier deps to ensure it can properly read the data.
  VerifierDeps decoded_deps(dex_files, ArrayRef<const uint8_t>(buffer));
  std::ostringstream stream;
  VariableIndentationOutputStream os(&stream);
  decoded_deps.Dump(&os);
}

TEST_F(VerifierDepsTest, UnverifiedClasses) {
  VerifyDexFile();
  ASSERT_FALSE(HasUnverifiedClass("LMyThread;"));
  // Test that a class with a soft failure is recorded.
  ASSERT_TRUE(HasUnverifiedClass("LMain;"));
  // Test that a class with hard failure is recorded.
  ASSERT_TRUE(HasUnverifiedClass("LMyVerificationFailure;"));
  // Test that a class with unresolved super is recorded.
  ASSERT_TRUE(HasUnverifiedClass("LMyClassWithNoSuper;"));
  // Test that a class with unresolved super and hard failure is recorded.
  ASSERT_TRUE(HasUnverifiedClass("LMyClassWithNoSuperButFailures;"));
}

TEST_F(VerifierDepsTest, UnverifiedOrder) {
  ScopedObjectAccess soa(Thread::Current());
  jobject loader = LoadDex("VerifierDeps");
  std::vector<const DexFile*> dex_files = GetDexFiles(loader);
  ASSERT_GT(dex_files.size(), 0u);
  const DexFile* dex_file = dex_files[0];
  VerifierDeps deps1(dex_files);
  Thread* const self = Thread::Current();
  ASSERT_TRUE(self->GetVerifierDeps() == nullptr);
  self->SetVerifierDeps(&deps1);
  deps1.MaybeRecordVerificationStatus(*dex_file,
                                      dex::TypeIndex(0),
                                      verifier::FailureKind::kHardFailure);
  deps1.MaybeRecordVerificationStatus(*dex_file,
                                      dex::TypeIndex(1),
                                      verifier::FailureKind::kHardFailure);
  VerifierDeps deps2(dex_files);
  self->SetVerifierDeps(nullptr);
  self->SetVerifierDeps(&deps2);
  deps2.MaybeRecordVerificationStatus(*dex_file,
                                      dex::TypeIndex(1),
                                      verifier::FailureKind::kHardFailure);
  deps2.MaybeRecordVerificationStatus(*dex_file,
                                      dex::TypeIndex(0),
                                      verifier::FailureKind::kHardFailure);
  self->SetVerifierDeps(nullptr);
  std::vector<uint8_t> buffer1;
  deps1.Encode(dex_files, &buffer1);
  std::vector<uint8_t> buffer2;
  deps2.Encode(dex_files, &buffer2);
  EXPECT_EQ(buffer1, buffer2);
}

TEST_F(VerifierDepsTest, VerifyDeps) {
  VerifyDexFile();

  ASSERT_EQ(1u, NumberOfCompiledDexFiles());
  ASSERT_TRUE(HasEachKindOfRecord());

  // When validating, we create a new class loader, as
  // the existing `class_loader_` may contain erroneous classes,
  // that ClassLinker::FindClass won't return.

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  MutableHandle<mirror::ClassLoader> new_class_loader(hs.NewHandle<mirror::ClassLoader>(nullptr));
  {
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_TRUE(verifier_deps_->ValidateDependencies(new_class_loader, soa.Self()));
  }

  std::vector<uint8_t> buffer;
  verifier_deps_->Encode(dex_files_, &buffer);
  ASSERT_FALSE(buffer.empty());

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_TRUE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  // Fiddle with the dependencies to make sure we catch any change and fail to verify.

  {
    // Mess up with the assignable_types.
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    deps->assignable_types_.insert(*deps->unassignable_types_.begin());
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    // Mess up with the unassignable_types.
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    deps->unassignable_types_.insert(*deps->assignable_types_.begin());
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  // Mess up with classes.
  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->classes_) {
      if (entry.IsResolved()) {
        deps->classes_.insert(VerifierDeps::ClassResolution(
            entry.GetDexTypeIndex(), VerifierDeps::kUnresolvedMarker));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->classes_) {
      if (!entry.IsResolved()) {
        deps->classes_.insert(VerifierDeps::ClassResolution(
            entry.GetDexTypeIndex(), VerifierDeps::kUnresolvedMarker - 1));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->classes_) {
      if (entry.IsResolved()) {
        deps->classes_.insert(VerifierDeps::ClassResolution(
            entry.GetDexTypeIndex(), entry.GetAccessFlags() - 1));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  // Mess up with fields.
  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->fields_) {
      if (entry.IsResolved()) {
        deps->fields_.insert(VerifierDeps::FieldResolution(entry.GetDexFieldIndex(),
                                                           VerifierDeps::kUnresolvedMarker,
                                                           entry.GetDeclaringClassIndex()));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->fields_) {
      if (!entry.IsResolved()) {
        constexpr dex::StringIndex kStringIndexZero(0);  // We know there is a class there.
        deps->fields_.insert(VerifierDeps::FieldResolution(0 /* we know there is a field there */,
                                                           VerifierDeps::kUnresolvedMarker - 1,
                                                           kStringIndexZero));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->fields_) {
      if (entry.IsResolved()) {
        deps->fields_.insert(VerifierDeps::FieldResolution(entry.GetDexFieldIndex(),
                                                           entry.GetAccessFlags() - 1,
                                                           entry.GetDeclaringClassIndex()));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    for (const auto& entry : deps->fields_) {
      constexpr dex::StringIndex kNewTypeIndex(0);
      if (entry.GetDeclaringClassIndex() != kNewTypeIndex) {
        deps->fields_.insert(VerifierDeps::FieldResolution(entry.GetDexFieldIndex(),
                                                           entry.GetAccessFlags(),
                                                           kNewTypeIndex));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  // Mess up with methods.
  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    std::set<VerifierDeps::MethodResolution>* methods = &deps->methods_;
    for (const auto& entry : *methods) {
      if (entry.IsResolved()) {
        methods->insert(VerifierDeps::MethodResolution(entry.GetDexMethodIndex(),
                                                       VerifierDeps::kUnresolvedMarker,
                                                       entry.GetDeclaringClassIndex()));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    std::set<VerifierDeps::MethodResolution>* methods = &deps->methods_;
    for (const auto& entry : *methods) {
      if (!entry.IsResolved()) {
        constexpr dex::StringIndex kStringIndexZero(0);  // We know there is a class there.
        methods->insert(VerifierDeps::MethodResolution(0 /* we know there is a method there */,
                                                       VerifierDeps::kUnresolvedMarker - 1,
                                                       kStringIndexZero));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    std::set<VerifierDeps::MethodResolution>* methods = &deps->methods_;
    for (const auto& entry : *methods) {
      if (entry.IsResolved()) {
        methods->insert(VerifierDeps::MethodResolution(entry.GetDexMethodIndex(),
                                                       entry.GetAccessFlags() - 1,
                                                       entry.GetDeclaringClassIndex()));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }

  {
    VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
    VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
    bool found = false;
    std::set<VerifierDeps::MethodResolution>* methods = &deps->methods_;
    for (const auto& entry : *methods) {
      constexpr dex::StringIndex kNewTypeIndex(0);
      if (entry.IsResolved() && entry.GetDeclaringClassIndex() != kNewTypeIndex) {
        methods->insert(VerifierDeps::MethodResolution(entry.GetDexMethodIndex(),
                                                       entry.GetAccessFlags(),
                                                       kNewTypeIndex));
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    new_class_loader.Assign(soa.Decode<mirror::ClassLoader>(LoadDex("VerifierDeps")));
    ASSERT_FALSE(decoded_deps.ValidateDependencies(new_class_loader, soa.Self()));
  }
}

TEST_F(VerifierDepsTest, CompilerDriver) {
  SetupCompilerDriver();

  // Test both multi-dex and single-dex configuration.
  for (const char* multi : { "MultiDex", static_cast<const char*>(nullptr) }) {
    // Test that the compiler driver behaves as expected when the dependencies
    // verify and when they don't verify.
    for (bool verify_failure : { false, true }) {
      {
        ScopedObjectAccess soa(Thread::Current());
        LoadDexFile(&soa, "VerifierDeps", multi);
      }
      VerifyWithCompilerDriver(/* verifier_deps */ nullptr);

      std::vector<uint8_t> buffer;
      verifier_deps_->Encode(dex_files_, &buffer);

      {
        ScopedObjectAccess soa(Thread::Current());
        LoadDexFile(&soa, "VerifierDeps", multi);
      }
      verifier::VerifierDeps decoded_deps(dex_files_, ArrayRef<const uint8_t>(buffer));
      if (verify_failure) {
        // Just taint the decoded VerifierDeps with one invalid entry.
        VerifierDeps::DexFileDeps* deps = decoded_deps.GetDexFileDeps(*primary_dex_file_);
        bool found = false;
        for (const auto& entry : deps->classes_) {
          if (entry.IsResolved()) {
            deps->classes_.insert(VerifierDeps::ClassResolution(
                entry.GetDexTypeIndex(), VerifierDeps::kUnresolvedMarker));
            found = true;
            break;
          }
        }
        ASSERT_TRUE(found);
      }
      VerifyWithCompilerDriver(&decoded_deps);

      if (verify_failure) {
        ASSERT_FALSE(verifier_deps_ == nullptr);
        ASSERT_FALSE(verifier_deps_->Equals(decoded_deps));
      } else {
        VerifyClassStatus(decoded_deps);
      }
    }
  }
}

TEST_F(VerifierDepsTest, MultiDexVerification) {
  VerifyDexFile("VerifierDepsMulti");
  ASSERT_EQ(NumberOfCompiledDexFiles(), 2u);

  ASSERT_TRUE(HasUnverifiedClass("LMySoftVerificationFailure;", *dex_files_[1]));
  ASSERT_TRUE(HasUnverifiedClass("LMySub1SoftVerificationFailure;", *dex_files_[0]));
  ASSERT_TRUE(HasUnverifiedClass("LMySub2SoftVerificationFailure;", *dex_files_[0]));

  std::vector<uint8_t> buffer;
  verifier_deps_->Encode(dex_files_, &buffer);
  ASSERT_FALSE(buffer.empty());
}

TEST_F(VerifierDepsTest, NotAssignable_InterfaceWithClassInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LIface;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "LIface;", false));
}

TEST_F(VerifierDepsTest, Assignable_Arrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[LIface;",
                                         /* src */ "[LMyClassExtendingInterface;",
                                         /* is_strict */ false,
                                         /* is_assignable */ true));
  ASSERT_FALSE(HasAssignable(
      "LIface;", "LMyClassExtendingInterface;", /* expected_is_assignable */ true));
  ASSERT_FALSE(HasAssignable(
      "LIface;", "LMyClassExtendingInterface;", /* expected_is_assignable */ false));
}

}  // namespace verifier
}  // namespace art
