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

#include <gtest/gtest.h>

#include "android-base/strings.h"
#include "art_method-inl.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "common_runtime_test.h"
#include "dex/descriptors_names.h"
#include "exec_utils.h"
#include "jit/profile_compilation_info.h"
#include "linear_alloc.h"
#include "mirror/class-inl.h"
#include "obj_ptr-inl.h"
#include "profile_assistant.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

using Hotness = ProfileCompilationInfo::MethodHotness;

static constexpr size_t kMaxMethodIds = 65535;

class ProfileAssistantTest : public CommonRuntimeTest {
 public:
  void PostRuntimeCreate() OVERRIDE {
    allocator_.reset(new ArenaAllocator(Runtime::Current()->GetArenaPool()));
  }

 protected:
  void SetupProfile(const std::string& id,
                    uint32_t checksum,
                    uint16_t number_of_methods,
                    uint16_t number_of_classes,
                    const ScratchFile& profile,
                    ProfileCompilationInfo* info,
                    uint16_t start_method_index = 0,
                    bool reverse_dex_write_order = false) {
    std::string dex_location1 = "location1" + id;
    uint32_t dex_location_checksum1 = checksum;
    std::string dex_location2 = "location2" + id;
    uint32_t dex_location_checksum2 = 10 * checksum;
    SetupProfile(dex_location1,
                 dex_location_checksum1,
                 dex_location2,
                 dex_location_checksum2,
                 number_of_methods,
                 number_of_classes,
                 profile,
                 info,
                 start_method_index,
                 reverse_dex_write_order);
  }

  void SetupProfile(const std::string& dex_location1,
                    uint32_t dex_location_checksum1,
                    const std::string& dex_location2,
                    uint32_t dex_location_checksum2,
                    uint16_t number_of_methods,
                    uint16_t number_of_classes,
                    const ScratchFile& profile,
                    ProfileCompilationInfo* info,
                    uint16_t start_method_index = 0,
                    bool reverse_dex_write_order = false,
                    uint32_t number_of_methods1 = kMaxMethodIds,
                    uint32_t number_of_methods2 = kMaxMethodIds) {
    for (uint16_t i = start_method_index; i < start_method_index + number_of_methods; i++) {
      // reverse_dex_write_order controls the order in which the dex files will be added to
      // the profile and thus written to disk.
      ProfileCompilationInfo::OfflineProfileMethodInfo pmi =
          GetOfflineProfileMethodInfo(dex_location1, dex_location_checksum1,
                                      dex_location2, dex_location_checksum2,
                                      number_of_methods1, number_of_methods2);
      Hotness::Flag flags = Hotness::kFlagPostStartup;
      if (reverse_dex_write_order) {
        ASSERT_TRUE(info->AddMethod(
            dex_location2, dex_location_checksum2, i, number_of_methods2, pmi, flags));
        ASSERT_TRUE(info->AddMethod(
            dex_location1, dex_location_checksum1, i, number_of_methods1, pmi, flags));
      } else {
        ASSERT_TRUE(info->AddMethod(
            dex_location1, dex_location_checksum1, i, number_of_methods1, pmi, flags));
        ASSERT_TRUE(info->AddMethod(
            dex_location2, dex_location_checksum2, i, number_of_methods2, pmi, flags));
      }
    }
    for (uint16_t i = 0; i < number_of_classes; i++) {
      ASSERT_TRUE(info->AddClassIndex(dex_location1,
                                      dex_location_checksum1,
                                      dex::TypeIndex(i),
                                      number_of_methods1));
    }

    ASSERT_TRUE(info->Save(GetFd(profile)));
    ASSERT_EQ(0, profile.GetFile()->Flush());
    ASSERT_TRUE(profile.GetFile()->ResetOffset());
  }

  void SetupBasicProfile(const std::string& id,
                         uint32_t checksum,
                         uint16_t number_of_methods,
                         const std::vector<uint32_t> hot_methods,
                         const std::vector<uint32_t> startup_methods,
                         const std::vector<uint32_t> post_startup_methods,
                         const ScratchFile& profile,
                         ProfileCompilationInfo* info) {
    std::string dex_location = "location1" + id;
    for (uint32_t idx : hot_methods) {
      info->AddMethodIndex(Hotness::kFlagHot, dex_location, checksum, idx, number_of_methods);
    }
    for (uint32_t idx : startup_methods) {
      info->AddMethodIndex(Hotness::kFlagStartup, dex_location, checksum, idx, number_of_methods);
    }
    for (uint32_t idx : post_startup_methods) {
      info->AddMethodIndex(Hotness::kFlagPostStartup,
                           dex_location,
                           checksum,
                           idx,
                           number_of_methods);
    }
    ASSERT_TRUE(info->Save(GetFd(profile)));
    ASSERT_EQ(0, profile.GetFile()->Flush());
    ASSERT_TRUE(profile.GetFile()->ResetOffset());
  }

  // Creates an inline cache which will be destructed at the end of the test.
  ProfileCompilationInfo::InlineCacheMap* CreateInlineCacheMap() {
    used_inline_caches.emplace_back(new ProfileCompilationInfo::InlineCacheMap(
        std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)));
    return used_inline_caches.back().get();
  }

  ProfileCompilationInfo::OfflineProfileMethodInfo GetOfflineProfileMethodInfo(
        const std::string& dex_location1, uint32_t dex_checksum1,
        const std::string& dex_location2, uint32_t dex_checksum2,
        uint32_t number_of_methods1 = kMaxMethodIds, uint32_t number_of_methods2 = kMaxMethodIds) {
    ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();
    ProfileCompilationInfo::OfflineProfileMethodInfo pmi(ic_map);
    pmi.dex_references.emplace_back(dex_location1, dex_checksum1, number_of_methods1);
    pmi.dex_references.emplace_back(dex_location2, dex_checksum2, number_of_methods2);

    // Monomorphic
    for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Polymorphic
    for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      dex_pc_data.AddClass(1, dex::TypeIndex(1));

      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Megamorphic
    for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMegamorphic();
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Missing types
    for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    return pmi;
  }

  int GetFd(const ScratchFile& file) const {
    return static_cast<int>(file.GetFd());
  }

  void CheckProfileInfo(ScratchFile& file, const ProfileCompilationInfo& info) {
    ProfileCompilationInfo file_info;
    ASSERT_TRUE(file.GetFile()->ResetOffset());
    ASSERT_TRUE(file_info.Load(GetFd(file)));
    ASSERT_TRUE(file_info.Equals(info));
  }

  std::string GetProfmanCmd() {
    std::string file_path = GetTestAndroidRoot();
    file_path += "/bin/profman";
    if (kIsDebugBuild) {
      file_path += "d";
    }
    EXPECT_TRUE(OS::FileExists(file_path.c_str()))
        << file_path << " should be a valid file path";
    return file_path;
  }

  // Runs test with given arguments.
  int ProcessProfiles(const std::vector<int>& profiles_fd, int reference_profile_fd) {
    std::string profman_cmd = GetProfmanCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(profman_cmd);
    for (size_t k = 0; k < profiles_fd.size(); k++) {
      argv_str.push_back("--profile-file-fd=" + std::to_string(profiles_fd[k]));
    }
    argv_str.push_back("--reference-profile-file-fd=" + std::to_string(reference_profile_fd));

    std::string error;
    return ExecAndReturnCode(argv_str, &error);
  }

  bool GenerateTestProfile(const std::string& filename) {
    std::string profman_cmd = GetProfmanCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(profman_cmd);
    argv_str.push_back("--generate-test-profile=" + filename);
    std::string error;
    return ExecAndReturnCode(argv_str, &error);
  }

  bool GenerateTestProfileWithInputDex(const std::string& filename) {
    std::string profman_cmd = GetProfmanCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(profman_cmd);
    argv_str.push_back("--generate-test-profile=" + filename);
    argv_str.push_back("--generate-test-profile-seed=0");
    argv_str.push_back("--apk=" + GetLibCoreDexFileNames()[0]);
    argv_str.push_back("--dex-location=" + GetLibCoreDexFileNames()[0]);
    std::string error;
    return ExecAndReturnCode(argv_str, &error);
  }

  bool CreateProfile(const std::string& profile_file_contents,
                     const std::string& filename,
                     const std::string& dex_location) {
    ScratchFile class_names_file;
    File* file = class_names_file.GetFile();
    EXPECT_TRUE(file->WriteFully(profile_file_contents.c_str(), profile_file_contents.length()));
    EXPECT_EQ(0, file->Flush());
    EXPECT_TRUE(file->ResetOffset());
    std::string profman_cmd = GetProfmanCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(profman_cmd);
    argv_str.push_back("--create-profile-from=" + class_names_file.GetFilename());
    argv_str.push_back("--reference-profile-file=" + filename);
    argv_str.push_back("--apk=" + dex_location);
    argv_str.push_back("--dex-location=" + dex_location);
    std::string error;
    EXPECT_EQ(ExecAndReturnCode(argv_str, &error), 0);
    return true;
  }

  bool RunProfman(const std::string& filename,
                  std::vector<std::string>& extra_args,
                  std::string* output) {
    ScratchFile output_file;
    std::string profman_cmd = GetProfmanCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(profman_cmd);
    argv_str.insert(argv_str.end(), extra_args.begin(), extra_args.end());
    argv_str.push_back("--profile-file=" + filename);
    argv_str.push_back("--apk=" + GetLibCoreDexFileNames()[0]);
    argv_str.push_back("--dex-location=" + GetLibCoreDexFileNames()[0]);
    argv_str.push_back("--dump-output-to-fd=" + std::to_string(GetFd(output_file)));
    std::string error;
    EXPECT_EQ(ExecAndReturnCode(argv_str, &error), 0);
    File* file = output_file.GetFile();
    EXPECT_EQ(0, file->Flush());
    EXPECT_TRUE(file->ResetOffset());
    int64_t length = file->GetLength();
    std::unique_ptr<char[]> buf(new char[length]);
    EXPECT_EQ(file->Read(buf.get(), length, 0), length);
    *output = std::string(buf.get(), length);
    return true;
  }

  bool DumpClassesAndMethods(const std::string& filename, std::string* file_contents) {
    std::vector<std::string> extra_args;
    extra_args.push_back("--dump-classes-and-methods");
    return RunProfman(filename, extra_args, file_contents);
  }

  bool DumpOnly(const std::string& filename, std::string* file_contents) {
    std::vector<std::string> extra_args;
    extra_args.push_back("--dump-only");
    return RunProfman(filename, extra_args, file_contents);
  }

  bool CreateAndDump(const std::string& input_file_contents,
                     std::string* output_file_contents) {
    ScratchFile profile_file;
    EXPECT_TRUE(CreateProfile(input_file_contents,
                              profile_file.GetFilename(),
                              GetLibCoreDexFileNames()[0]));
    profile_file.GetFile()->ResetOffset();
    EXPECT_TRUE(DumpClassesAndMethods(profile_file.GetFilename(), output_file_contents));
    return true;
  }

  mirror::Class* GetClass(jobject class_loader, const std::string& clazz) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);
    Handle<mirror::ClassLoader> h_loader(
        hs.NewHandle(ObjPtr<mirror::ClassLoader>::DownCast(self->DecodeJObject(class_loader))));
    return class_linker->FindClass(self, clazz.c_str(), h_loader);
  }

  ArtMethod* GetVirtualMethod(jobject class_loader,
                              const std::string& clazz,
                              const std::string& name) {
    mirror::Class* klass = GetClass(class_loader, clazz);
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    const auto pointer_size = class_linker->GetImagePointerSize();
    ArtMethod* method = nullptr;
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    for (auto& m : klass->GetVirtualMethods(pointer_size)) {
      if (name == m.GetName()) {
        EXPECT_TRUE(method == nullptr);
        method = &m;
      }
    }
    return method;
  }

  // Verify that given method has the expected inline caches and nothing else.
  void AssertInlineCaches(ArtMethod* method,
                          const std::set<mirror::Class*>& expected_clases,
                          const ProfileCompilationInfo& info,
                          bool is_megamorphic,
                          bool is_missing_types)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> pmi =
        info.GetMethod(method->GetDexFile()->GetLocation(),
                       method->GetDexFile()->GetLocationChecksum(),
                       method->GetDexMethodIndex());
    ASSERT_TRUE(pmi != nullptr);
    ASSERT_EQ(pmi->inline_caches->size(), 1u);
    const ProfileCompilationInfo::DexPcData& dex_pc_data = pmi->inline_caches->begin()->second;

    ASSERT_EQ(dex_pc_data.is_megamorphic, is_megamorphic);
    ASSERT_EQ(dex_pc_data.is_missing_types, is_missing_types);
    ASSERT_EQ(expected_clases.size(), dex_pc_data.classes.size());
    size_t found = 0;
    for (mirror::Class* it : expected_clases) {
      for (const auto& class_ref : dex_pc_data.classes) {
        ProfileCompilationInfo::DexReference dex_ref =
            pmi->dex_references[class_ref.dex_profile_index];
        if (dex_ref.MatchesDex(&(it->GetDexFile())) &&
            class_ref.type_index == it->GetDexTypeIndex()) {
          found++;
        }
      }
    }

    ASSERT_EQ(expected_clases.size(), found);
  }

  int CheckCompilationMethodPercentChange(uint16_t methods_in_cur_profile,
                                          uint16_t methods_in_ref_profile) {
    ScratchFile profile;
    ScratchFile reference_profile;
    std::vector<int> profile_fds({ GetFd(profile)});
    int reference_profile_fd = GetFd(reference_profile);
    std::vector<uint32_t> hot_methods_cur;
    std::vector<uint32_t> hot_methods_ref;
    std::vector<uint32_t> empty_vector;
    for (size_t i = 0; i < methods_in_cur_profile; ++i) {
      hot_methods_cur.push_back(i);
    }
    for (size_t i = 0; i < methods_in_ref_profile; ++i) {
      hot_methods_ref.push_back(i);
    }
    ProfileCompilationInfo info1;
    uint16_t methods_in_profile = std::max(methods_in_cur_profile, methods_in_ref_profile);
    SetupBasicProfile("p1", 1, methods_in_profile, hot_methods_cur, empty_vector, empty_vector,
        profile,  &info1);
    ProfileCompilationInfo info2;
    SetupBasicProfile("p1", 1, methods_in_profile, hot_methods_ref, empty_vector, empty_vector,
        reference_profile,  &info2);
    return ProcessProfiles(profile_fds, reference_profile_fd);
  }

  int CheckCompilationClassPercentChange(uint16_t classes_in_cur_profile,
                                         uint16_t classes_in_ref_profile) {
    ScratchFile profile;
    ScratchFile reference_profile;

    std::vector<int> profile_fds({ GetFd(profile)});
    int reference_profile_fd = GetFd(reference_profile);

    ProfileCompilationInfo info1;
    SetupProfile("p1", 1, 0, classes_in_cur_profile, profile,  &info1);
    ProfileCompilationInfo info2;
    SetupProfile("p1", 1, 0, classes_in_ref_profile, reference_profile, &info2);
    return ProcessProfiles(profile_fds, reference_profile_fd);
  }

  std::unique_ptr<ArenaAllocator> allocator_;

  // Cache of inline caches generated during tests.
  // This makes it easier to pass data between different utilities and ensure that
  // caches are destructed at the end of the test.
  std::vector<std::unique_ptr<ProfileCompilationInfo::InlineCacheMap>> used_inline_caches;
};

TEST_F(ProfileAssistantTest, AdviseCompilationEmptyReferences) {
  ScratchFile profile1;
  ScratchFile profile2;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1),
      GetFd(profile2)});
  int reference_profile_fd = GetFd(reference_profile);

  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToEnableCompilation, 0, profile1, &info1);
  ProfileCompilationInfo info2;
  SetupProfile("p2", 2, kNumberOfMethodsToEnableCompilation, 0, profile2, &info2);

  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            ProcessProfiles(profile_fds, reference_profile_fd));
  // The resulting compilation info must be equal to the merge of the inputs.
  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile_fd));

  ProfileCompilationInfo expected;
  ASSERT_TRUE(expected.MergeWith(info1));
  ASSERT_TRUE(expected.MergeWith(info2));
  ASSERT_TRUE(expected.Equals(result));

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
  CheckProfileInfo(profile2, info2);
}

// TODO(calin): Add more tests for classes.
TEST_F(ProfileAssistantTest, AdviseCompilationEmptyReferencesBecauseOfClasses) {
  ScratchFile profile1;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1)});
  int reference_profile_fd = GetFd(reference_profile);

  const uint16_t kNumberOfClassesToEnableCompilation = 100;
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, 0, kNumberOfClassesToEnableCompilation, profile1, &info1);

  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            ProcessProfiles(profile_fds, reference_profile_fd));
  // The resulting compilation info must be equal to the merge of the inputs.
  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile_fd));

  ProfileCompilationInfo expected;
  ASSERT_TRUE(expected.MergeWith(info1));
  ASSERT_TRUE(expected.Equals(result));

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
}

TEST_F(ProfileAssistantTest, AdviseCompilationNonEmptyReferences) {
  ScratchFile profile1;
  ScratchFile profile2;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1),
      GetFd(profile2)});
  int reference_profile_fd = GetFd(reference_profile);

  // The new profile info will contain the methods with indices 0-100.
  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToEnableCompilation, 0, profile1, &info1);
  ProfileCompilationInfo info2;
  SetupProfile("p2", 2, kNumberOfMethodsToEnableCompilation, 0, profile2, &info2);


  // The reference profile info will contain the methods with indices 50-150.
  const uint16_t kNumberOfMethodsAlreadyCompiled = 100;
  ProfileCompilationInfo reference_info;
  SetupProfile("p1", 1, kNumberOfMethodsAlreadyCompiled, 0, reference_profile,
      &reference_info, kNumberOfMethodsToEnableCompilation / 2);

  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            ProcessProfiles(profile_fds, reference_profile_fd));

  // The resulting compilation info must be equal to the merge of the inputs
  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile_fd));

  ProfileCompilationInfo expected;
  ASSERT_TRUE(expected.MergeWith(info1));
  ASSERT_TRUE(expected.MergeWith(info2));
  ASSERT_TRUE(expected.MergeWith(reference_info));
  ASSERT_TRUE(expected.Equals(result));

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
  CheckProfileInfo(profile2, info2);
}

TEST_F(ProfileAssistantTest, DoNotAdviseCompilation) {
  ScratchFile profile1;
  ScratchFile profile2;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1),
      GetFd(profile2)});
  int reference_profile_fd = GetFd(reference_profile);

  const uint16_t kNumberOfMethodsToSkipCompilation = 24;  // Threshold is 100.
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToSkipCompilation, 0, profile1, &info1);
  ProfileCompilationInfo info2;
  SetupProfile("p2", 2, kNumberOfMethodsToSkipCompilation, 0, profile2, &info2);

  // We should not advise compilation.
  ASSERT_EQ(ProfileAssistant::kSkipCompilation,
            ProcessProfiles(profile_fds, reference_profile_fd));

  // The information from profiles must remain the same.
  ProfileCompilationInfo file_info1;
  ASSERT_TRUE(profile1.GetFile()->ResetOffset());
  ASSERT_TRUE(file_info1.Load(GetFd(profile1)));
  ASSERT_TRUE(file_info1.Equals(info1));

  ProfileCompilationInfo file_info2;
  ASSERT_TRUE(profile2.GetFile()->ResetOffset());
  ASSERT_TRUE(file_info2.Load(GetFd(profile2)));
  ASSERT_TRUE(file_info2.Equals(info2));

  // Reference profile files must remain empty.
  ASSERT_EQ(0, reference_profile.GetFile()->GetLength());

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
  CheckProfileInfo(profile2, info2);
}

TEST_F(ProfileAssistantTest, DoNotAdviseCompilationMethodPercentage) {
  const uint16_t kNumberOfMethodsInRefProfile = 6000;
  const uint16_t kNumberOfMethodsInCurProfile = 6100;  // Threshold is 2%.
  // We should not advise compilation.
  ASSERT_EQ(ProfileAssistant::kSkipCompilation,
            CheckCompilationMethodPercentChange(kNumberOfMethodsInCurProfile,
                                                kNumberOfMethodsInRefProfile));
}

TEST_F(ProfileAssistantTest, ShouldAdviseCompilationMethodPercentage) {
  const uint16_t kNumberOfMethodsInRefProfile = 6000;
  const uint16_t kNumberOfMethodsInCurProfile = 6200;  // Threshold is 2%.
  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            CheckCompilationMethodPercentChange(kNumberOfMethodsInCurProfile,
                                                kNumberOfMethodsInRefProfile));
}

TEST_F(ProfileAssistantTest, DoNotdviseCompilationClassPercentage) {
  const uint16_t kNumberOfClassesInRefProfile = 6000;
  const uint16_t kNumberOfClassesInCurProfile = 6110;  // Threshold is 2%.
  // We should not advise compilation.
  ASSERT_EQ(ProfileAssistant::kSkipCompilation,
            CheckCompilationClassPercentChange(kNumberOfClassesInCurProfile,
                                               kNumberOfClassesInRefProfile));
}

TEST_F(ProfileAssistantTest, ShouldAdviseCompilationClassPercentage) {
  const uint16_t kNumberOfClassesInRefProfile = 6000;
  const uint16_t kNumberOfClassesInCurProfile = 6120;  // Threshold is 2%.
  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            CheckCompilationClassPercentChange(kNumberOfClassesInCurProfile,
                                               kNumberOfClassesInRefProfile));
}

TEST_F(ProfileAssistantTest, FailProcessingBecauseOfProfiles) {
  ScratchFile profile1;
  ScratchFile profile2;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1),
      GetFd(profile2)});
  int reference_profile_fd = GetFd(reference_profile);

  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  // Assign different hashes for the same dex file. This will make merging of information to fail.
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToEnableCompilation, 0, profile1, &info1);
  ProfileCompilationInfo info2;
  SetupProfile("p1", 2, kNumberOfMethodsToEnableCompilation, 0, profile2, &info2);

  // We should fail processing.
  ASSERT_EQ(ProfileAssistant::kErrorBadProfiles,
            ProcessProfiles(profile_fds, reference_profile_fd));

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
  CheckProfileInfo(profile2, info2);

  // Reference profile files must still remain empty.
  ASSERT_EQ(0, reference_profile.GetFile()->GetLength());
}

TEST_F(ProfileAssistantTest, FailProcessingBecauseOfReferenceProfiles) {
  ScratchFile profile1;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1)});
  int reference_profile_fd = GetFd(reference_profile);

  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  // Assign different hashes for the same dex file. This will make merging of information to fail.
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToEnableCompilation, 0, profile1, &info1);
  ProfileCompilationInfo reference_info;
  SetupProfile("p1", 2, kNumberOfMethodsToEnableCompilation, 0, reference_profile, &reference_info);

  // We should not advise compilation.
  ASSERT_TRUE(profile1.GetFile()->ResetOffset());
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_EQ(ProfileAssistant::kErrorBadProfiles,
            ProcessProfiles(profile_fds, reference_profile_fd));

  // The information from profiles must remain the same.
  CheckProfileInfo(profile1, info1);
}

TEST_F(ProfileAssistantTest, TestProfileGeneration) {
  ScratchFile profile;
  // Generate a test profile.
  GenerateTestProfile(profile.GetFilename());

  // Verify that the generated profile is valid and can be loaded.
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ProfileCompilationInfo info;
  ASSERT_TRUE(info.Load(GetFd(profile)));
}

TEST_F(ProfileAssistantTest, TestProfileGenerationWithIndexDex) {
  ScratchFile profile;
  // Generate a test profile passing in a dex file as reference.
  GenerateTestProfileWithInputDex(profile.GetFilename());

  // Verify that the generated profile is valid and can be loaded.
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ProfileCompilationInfo info;
  ASSERT_TRUE(info.Load(GetFd(profile)));
}

TEST_F(ProfileAssistantTest, TestProfileCreationAllMatch) {
  // Class names put here need to be in sorted order.
  std::vector<std::string> class_names = {
    "HLjava/lang/Object;-><init>()V",
    "Ljava/lang/Comparable;",
    "Ljava/lang/Math;",
    "Ljava/lang/Object;",
    "SPLjava/lang/Comparable;->compareTo(Ljava/lang/Object;)I",
  };
  std::string file_contents;
  for (std::string& class_name : class_names) {
    file_contents += class_name + std::string("\n");
  }
  std::string output_file_contents;
  ASSERT_TRUE(CreateAndDump(file_contents, &output_file_contents));
  ASSERT_EQ(output_file_contents, file_contents);
}

TEST_F(ProfileAssistantTest, TestProfileCreationGenerateMethods) {
  // Class names put here need to be in sorted order.
  std::vector<std::string> class_names = {
    "Ljava/lang/Math;->*",
  };
  std::string input_file_contents;
  std::string expected_contents;
  for (std::string& class_name : class_names) {
    input_file_contents += class_name + std::string("\n");
    expected_contents += DescriptorToDot(class_name.c_str()) +
        std::string("\n");
  }
  std::string output_file_contents;
  ScratchFile profile_file;
  EXPECT_TRUE(CreateProfile(input_file_contents,
                            profile_file.GetFilename(),
                            GetLibCoreDexFileNames()[0]));
  ProfileCompilationInfo info;
  profile_file.GetFile()->ResetOffset();
  ASSERT_TRUE(info.Load(GetFd(profile_file)));
  // Verify that the profile has matching methods.
  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::Class> klass = GetClass(nullptr, "Ljava/lang/Math;");
  ASSERT_TRUE(klass != nullptr);
  size_t method_count = 0;
  for (ArtMethod& method : klass->GetMethods(kRuntimePointerSize)) {
    if (!method.IsCopied() && method.GetCodeItem() != nullptr) {
      ++method_count;
      std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> pmi =
          info.GetMethod(method.GetDexFile()->GetLocation(),
                         method.GetDexFile()->GetLocationChecksum(),
                         method.GetDexMethodIndex());
      ASSERT_TRUE(pmi != nullptr) << method.PrettyMethod();
    }
  }
  EXPECT_GT(method_count, 0u);
}

TEST_F(ProfileAssistantTest, TestBootImageProfile) {
  const std::string core_dex = GetLibCoreDexFileNames()[0];

  std::vector<ScratchFile> profiles;

  // In image with enough clean occurrences.
  const std::string kCleanClass = "Ljava/lang/CharSequence;";
  // In image with enough dirty occurrences.
  const std::string kDirtyClass = "Ljava/lang/Object;";
  // Not in image becauseof not enough occurrences.
  const std::string kUncommonCleanClass = "Ljava/lang/Process;";
  const std::string kUncommonDirtyClass = "Ljava/lang/Package;";
  // Method that is hot.
  // Also adds the class through inference since it is in each dex.
  const std::string kHotMethod = "Ljava/lang/Comparable;->compareTo(Ljava/lang/Object;)I";
  // Method that doesn't add the class since its only in one profile. Should still show up in the
  // boot profile.
  const std::string kOtherMethod = "Ljava/util/HashMap;-><init>()V";
  // Method that gets marked as hot since it's in multiple profiles.
  const std::string kMultiMethod = "Ljava/util/ArrayList;->clear()V";

  // Thresholds for this test.
  static const size_t kDirtyThreshold = 3;
  static const size_t kCleanThreshold = 2;
  static const size_t kMethodThreshold = 2;

  // Create a bunch of boot profiles.
  std::string dex1 =
      kCleanClass + "\n" +
      kDirtyClass + "\n" +
      kUncommonCleanClass + "\n" +
      "H" + kHotMethod + "\n" +
      kUncommonDirtyClass;
  profiles.emplace_back(ScratchFile());
  EXPECT_TRUE(CreateProfile(
      dex1, profiles.back().GetFilename(), core_dex));

  // Create a bunch of boot profiles.
  std::string dex2 =
      kCleanClass + "\n" +
      kDirtyClass + "\n" +
      "P" + kHotMethod + "\n" +
      "P" + kMultiMethod + "\n" +
      kUncommonDirtyClass;
  profiles.emplace_back(ScratchFile());
  EXPECT_TRUE(CreateProfile(
      dex2, profiles.back().GetFilename(), core_dex));

  // Create a bunch of boot profiles.
  std::string dex3 =
      "S" + kHotMethod + "\n" +
      "P" + kOtherMethod + "\n" +
      "P" + kMultiMethod + "\n" +
      kDirtyClass + "\n";
  profiles.emplace_back(ScratchFile());
  EXPECT_TRUE(CreateProfile(
      dex3, profiles.back().GetFilename(), core_dex));

  // Generate the boot profile.
  ScratchFile out_profile;
  std::vector<std::string> args;
  args.push_back(GetProfmanCmd());
  args.push_back("--generate-boot-image-profile");
  args.push_back("--boot-image-class-threshold=" + std::to_string(kDirtyThreshold));
  args.push_back("--boot-image-clean-class-threshold=" + std::to_string(kCleanThreshold));
  args.push_back("--boot-image-sampled-method-threshold=" + std::to_string(kMethodThreshold));
  args.push_back("--reference-profile-file=" + out_profile.GetFilename());
  args.push_back("--apk=" + core_dex);
  args.push_back("--dex-location=" + core_dex);
  for (const ScratchFile& profile : profiles) {
    args.push_back("--profile-file=" + profile.GetFilename());
  }
  std::string error;
  EXPECT_EQ(ExecAndReturnCode(args, &error), 0) << error;
  ASSERT_EQ(0, out_profile.GetFile()->Flush());
  ASSERT_TRUE(out_profile.GetFile()->ResetOffset());

  // Verify the boot profile contents.
  std::string output_file_contents;
  EXPECT_TRUE(DumpClassesAndMethods(out_profile.GetFilename(), &output_file_contents));
  // Common classes, should be in the classes of the profile.
  EXPECT_NE(output_file_contents.find(kCleanClass + "\n"), std::string::npos)
      << output_file_contents;
  EXPECT_NE(output_file_contents.find(kDirtyClass + "\n"), std::string::npos)
      << output_file_contents;
  // Uncommon classes, should not fit preloaded class criteria and should not be in the profile.
  EXPECT_EQ(output_file_contents.find(kUncommonCleanClass + "\n"), std::string::npos)
      << output_file_contents;
  EXPECT_EQ(output_file_contents.find(kUncommonDirtyClass + "\n"), std::string::npos)
      << output_file_contents;
  // Inferred class from a method common to all three profiles.
  EXPECT_NE(output_file_contents.find("Ljava/lang/Comparable;\n"), std::string::npos)
      << output_file_contents;
  // Aggregated methods hotness information.
  EXPECT_NE(output_file_contents.find("HSP" + kHotMethod), std::string::npos)
      << output_file_contents;
  EXPECT_NE(output_file_contents.find("P" + kOtherMethod), std::string::npos)
      << output_file_contents;
  // Not inferred class, method is only in one profile.
  EXPECT_EQ(output_file_contents.find("Ljava/util/HashMap;\n"), std::string::npos)
      << output_file_contents;
  // Test the sampled methods that became hot.
  // Other method is in only one profile, it should not become hot.
  EXPECT_EQ(output_file_contents.find("HP" + kOtherMethod), std::string::npos)
      << output_file_contents;
  // Multi method is in at least two profiles, it should become hot.
  EXPECT_NE(output_file_contents.find("HP" + kMultiMethod), std::string::npos)
      << output_file_contents;
}

TEST_F(ProfileAssistantTest, TestProfileCreationOneNotMatched) {
  // Class names put here need to be in sorted order.
  std::vector<std::string> class_names = {
    "Ldoesnt/match/this/one;",
    "Ljava/lang/Comparable;",
    "Ljava/lang/Object;"
  };
  std::string input_file_contents;
  for (std::string& class_name : class_names) {
    input_file_contents += class_name + std::string("\n");
  }
  std::string output_file_contents;
  ASSERT_TRUE(CreateAndDump(input_file_contents, &output_file_contents));
  std::string expected_contents =
      class_names[1] + std::string("\n") +
      class_names[2] + std::string("\n");
  ASSERT_EQ(output_file_contents, expected_contents);
}

TEST_F(ProfileAssistantTest, TestProfileCreationNoneMatched) {
  // Class names put here need to be in sorted order.
  std::vector<std::string> class_names = {
    "Ldoesnt/match/this/one;",
    "Ldoesnt/match/this/one/either;",
    "Lnor/this/one;"
  };
  std::string input_file_contents;
  for (std::string& class_name : class_names) {
    input_file_contents += class_name + std::string("\n");
  }
  std::string output_file_contents;
  ASSERT_TRUE(CreateAndDump(input_file_contents, &output_file_contents));
  std::string expected_contents("");
  ASSERT_EQ(output_file_contents, expected_contents);
}

TEST_F(ProfileAssistantTest, TestProfileCreateInlineCache) {
  // Create the profile content.
  std::vector<std::string> methods = {
    "LTestInline;->inlineMonomorphic(LSuper;)I+LSubA;",
    "LTestInline;->inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,LSubC;",
    "LTestInline;->inlineMegamorphic(LSuper;)I+LSubA;,LSubB;,LSubC;,LSubD;,LSubE;",
    "LTestInline;->inlineMissingTypes(LSuper;)I+missing_types",
    "LTestInline;->noInlineCache(LSuper;)I"
  };
  std::string input_file_contents;
  for (std::string& m : methods) {
    input_file_contents += m + std::string("\n");
  }

  // Create the profile and save it to disk.
  ScratchFile profile_file;
  ASSERT_TRUE(CreateProfile(input_file_contents,
                            profile_file.GetFilename(),
                            GetTestDexFileName("ProfileTestMultiDex")));

  // Load the profile from disk.
  ProfileCompilationInfo info;
  profile_file.GetFile()->ResetOffset();
  ASSERT_TRUE(info.Load(GetFd(profile_file)));

  // Load the dex files and verify that the profile contains the expected methods info.
  ScopedObjectAccess soa(Thread::Current());
  jobject class_loader = LoadDex("ProfileTestMultiDex");
  ASSERT_NE(class_loader, nullptr);

  mirror::Class* sub_a = GetClass(class_loader, "LSubA;");
  mirror::Class* sub_b = GetClass(class_loader, "LSubB;");
  mirror::Class* sub_c = GetClass(class_loader, "LSubC;");

  ASSERT_TRUE(sub_a != nullptr);
  ASSERT_TRUE(sub_b != nullptr);
  ASSERT_TRUE(sub_c != nullptr);

  {
    // Verify that method inlineMonomorphic has the expected inline caches and nothing else.
    ArtMethod* inline_monomorphic = GetVirtualMethod(class_loader,
                                                     "LTestInline;",
                                                     "inlineMonomorphic");
    ASSERT_TRUE(inline_monomorphic != nullptr);
    std::set<mirror::Class*> expected_monomorphic;
    expected_monomorphic.insert(sub_a);
    AssertInlineCaches(inline_monomorphic,
                       expected_monomorphic,
                       info,
                       /*megamorphic*/false,
                       /*missing_types*/false);
  }

  {
    // Verify that method inlinePolymorphic has the expected inline caches and nothing else.
    ArtMethod* inline_polymorhic = GetVirtualMethod(class_loader,
                                                    "LTestInline;",
                                                    "inlinePolymorphic");
    ASSERT_TRUE(inline_polymorhic != nullptr);
    std::set<mirror::Class*> expected_polymorphic;
    expected_polymorphic.insert(sub_a);
    expected_polymorphic.insert(sub_b);
    expected_polymorphic.insert(sub_c);
    AssertInlineCaches(inline_polymorhic,
                       expected_polymorphic,
                       info,
                       /*megamorphic*/false,
                       /*missing_types*/false);
  }

  {
    // Verify that method inlineMegamorphic has the expected inline caches and nothing else.
    ArtMethod* inline_megamorphic = GetVirtualMethod(class_loader,
                                                     "LTestInline;",
                                                     "inlineMegamorphic");
    ASSERT_TRUE(inline_megamorphic != nullptr);
    std::set<mirror::Class*> expected_megamorphic;
    AssertInlineCaches(inline_megamorphic,
                       expected_megamorphic,
                       info,
                       /*megamorphic*/true,
                       /*missing_types*/false);
  }

  {
    // Verify that method inlineMegamorphic has the expected inline caches and nothing else.
    ArtMethod* inline_missing_types = GetVirtualMethod(class_loader,
                                                       "LTestInline;",
                                                       "inlineMissingTypes");
    ASSERT_TRUE(inline_missing_types != nullptr);
    std::set<mirror::Class*> expected_missing_Types;
    AssertInlineCaches(inline_missing_types,
                       expected_missing_Types,
                       info,
                       /*megamorphic*/false,
                       /*missing_types*/true);
  }

  {
    // Verify that method noInlineCache has no inline caches in the profile.
    ArtMethod* no_inline_cache = GetVirtualMethod(class_loader, "LTestInline;", "noInlineCache");
    ASSERT_TRUE(no_inline_cache != nullptr);
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> pmi_no_inline_cache =
        info.GetMethod(no_inline_cache->GetDexFile()->GetLocation(),
                       no_inline_cache->GetDexFile()->GetLocationChecksum(),
                       no_inline_cache->GetDexMethodIndex());
    ASSERT_TRUE(pmi_no_inline_cache != nullptr);
    ASSERT_TRUE(pmi_no_inline_cache->inline_caches->empty());
  }
}

TEST_F(ProfileAssistantTest, MergeProfilesWithDifferentDexOrder) {
  ScratchFile profile1;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({GetFd(profile1)});
  int reference_profile_fd = GetFd(reference_profile);

  // The new profile info will contain the methods with indices 0-100.
  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  ProfileCompilationInfo info1;
  SetupProfile("p1", 1, kNumberOfMethodsToEnableCompilation, 0, profile1, &info1,
      /*start_method_index*/0, /*reverse_dex_write_order*/false);

  // The reference profile info will contain the methods with indices 50-150.
  // When setting up the profile reverse the order in which the dex files
  // are added to the profile. This will verify that profman merges profiles
  // with a different dex order correctly.
  const uint16_t kNumberOfMethodsAlreadyCompiled = 100;
  ProfileCompilationInfo reference_info;
  SetupProfile("p1", 1, kNumberOfMethodsAlreadyCompiled, 0, reference_profile,
      &reference_info, kNumberOfMethodsToEnableCompilation / 2, /*reverse_dex_write_order*/true);

  // We should advise compilation.
  ASSERT_EQ(ProfileAssistant::kCompile,
            ProcessProfiles(profile_fds, reference_profile_fd));

  // The resulting compilation info must be equal to the merge of the inputs.
  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile_fd));

  ProfileCompilationInfo expected;
  ASSERT_TRUE(expected.MergeWith(reference_info));
  ASSERT_TRUE(expected.MergeWith(info1));
  ASSERT_TRUE(expected.Equals(result));

  // The information from profile must remain the same.
  CheckProfileInfo(profile1, info1);
}

TEST_F(ProfileAssistantTest, TestProfileCreateWithInvalidData) {
  // Create the profile content.
  std::vector<std::string> profile_methods = {
    "LTestInline;->inlineMonomorphic(LSuper;)I+invalid_class",
    "LTestInline;->invalid_method",
    "invalid_class"
  };
  std::string input_file_contents;
  for (std::string& m : profile_methods) {
    input_file_contents += m + std::string("\n");
  }

  // Create the profile and save it to disk.
  ScratchFile profile_file;
  std::string dex_filename = GetTestDexFileName("ProfileTestMultiDex");
  ASSERT_TRUE(CreateProfile(input_file_contents,
                            profile_file.GetFilename(),
                            dex_filename));

  // Load the profile from disk.
  ProfileCompilationInfo info;
  profile_file.GetFile()->ResetOffset();
  ASSERT_TRUE(info.Load(GetFd(profile_file)));

  // Load the dex files and verify that the profile contains the expected methods info.
  ScopedObjectAccess soa(Thread::Current());
  jobject class_loader = LoadDex("ProfileTestMultiDex");
  ASSERT_NE(class_loader, nullptr);

  ArtMethod* inline_monomorphic = GetVirtualMethod(class_loader,
                                                   "LTestInline;",
                                                   "inlineMonomorphic");
  const DexFile* dex_file = inline_monomorphic->GetDexFile();

  // Verify that the inline cache contains the invalid type.
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> pmi =
      info.GetMethod(dex_file->GetLocation(),
                     dex_file->GetLocationChecksum(),
                     inline_monomorphic->GetDexMethodIndex());
  ASSERT_TRUE(pmi != nullptr);
  ASSERT_EQ(pmi->inline_caches->size(), 1u);
  const ProfileCompilationInfo::DexPcData& dex_pc_data = pmi->inline_caches->begin()->second;
  dex::TypeIndex invalid_class_index(std::numeric_limits<uint16_t>::max() - 1);
  ASSERT_EQ(1u, dex_pc_data.classes.size());
  ASSERT_EQ(invalid_class_index, dex_pc_data.classes.begin()->type_index);

  // Verify that the start-up classes contain the invalid class.
  std::set<dex::TypeIndex> classes;
  std::set<uint16_t> hot_methods;
  std::set<uint16_t> startup_methods;
  std::set<uint16_t> post_start_methods;
  ASSERT_TRUE(info.GetClassesAndMethods(*dex_file,
                                        &classes,
                                        &hot_methods,
                                        &startup_methods,
                                        &post_start_methods));
  ASSERT_EQ(1u, classes.size());
  ASSERT_TRUE(classes.find(invalid_class_index) != classes.end());

  // Verify that the invalid method did not get in the profile.
  ASSERT_EQ(1u, hot_methods.size());
  uint16_t invalid_method_index = std::numeric_limits<uint16_t>::max() - 1;
  ASSERT_FALSE(hot_methods.find(invalid_method_index) != hot_methods.end());
}

TEST_F(ProfileAssistantTest, DumpOnly) {
  ScratchFile profile;

  const uint32_t kNumberOfMethods = 64;
  std::vector<uint32_t> hot_methods;
  std::vector<uint32_t> startup_methods;
  std::vector<uint32_t> post_startup_methods;
  for (size_t i = 0; i < kNumberOfMethods; ++i) {
    if (i % 2 == 0) {
      hot_methods.push_back(i);
    }
    if (i % 3 == 1) {
      startup_methods.push_back(i);
    }
    if (i % 4 == 2) {
      post_startup_methods.push_back(i);
    }
  }
  EXPECT_GT(hot_methods.size(), 0u);
  EXPECT_GT(startup_methods.size(), 0u);
  EXPECT_GT(post_startup_methods.size(), 0u);
  ProfileCompilationInfo info1;
  SetupBasicProfile("p1",
                    1,
                    kNumberOfMethods,
                    hot_methods,
                    startup_methods,
                    post_startup_methods,
                    profile,
                    &info1);
  std::string output;
  DumpOnly(profile.GetFilename(), &output);
  const size_t hot_offset = output.find("hot methods:");
  const size_t startup_offset = output.find("startup methods:");
  const size_t post_startup_offset = output.find("post startup methods:");
  const size_t classes_offset = output.find("classes:");
  ASSERT_NE(hot_offset, std::string::npos);
  ASSERT_NE(startup_offset, std::string::npos);
  ASSERT_NE(post_startup_offset, std::string::npos);
  ASSERT_LT(hot_offset, startup_offset);
  ASSERT_LT(startup_offset, post_startup_offset);
  // Check the actual contents of the dump by looking at the offsets of the methods.
  for (uint32_t m : hot_methods) {
    const size_t pos = output.find(std::to_string(m) + "[],", hot_offset);
    ASSERT_NE(pos, std::string::npos);
    EXPECT_LT(pos, startup_offset);
  }
  for (uint32_t m : startup_methods) {
    const size_t pos = output.find(std::to_string(m) + ",", startup_offset);
    ASSERT_NE(pos, std::string::npos);
    EXPECT_LT(pos, post_startup_offset);
  }
  for (uint32_t m : post_startup_methods) {
    const size_t pos = output.find(std::to_string(m) + ",", post_startup_offset);
    ASSERT_NE(pos, std::string::npos);
    EXPECT_LT(pos, classes_offset);
  }
}

TEST_F(ProfileAssistantTest, MergeProfilesWithFilter) {
  ScratchFile profile1;
  ScratchFile profile2;
  ScratchFile reference_profile;

  std::vector<int> profile_fds({
      GetFd(profile1),
      GetFd(profile2)});
  int reference_profile_fd = GetFd(reference_profile);

  // Use a real dex file to generate profile test data.
  // The file will be used during merging to filter unwanted data.
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("ProfileTestMultiDex");
  const DexFile& d1 = *dex_files[0];
  const DexFile& d2 = *dex_files[1];
  // The new profile info will contain the methods with indices 0-100.
  const uint16_t kNumberOfMethodsToEnableCompilation = 100;
  ProfileCompilationInfo info1;
  SetupProfile(d1.GetLocation(), d1.GetLocationChecksum(), "p1", 1,
      kNumberOfMethodsToEnableCompilation, 0, profile1, &info1);
  ProfileCompilationInfo info2;
  SetupProfile(d2.GetLocation(), d2.GetLocationChecksum(), "p2", 2,
      kNumberOfMethodsToEnableCompilation, 0, profile2, &info2);


  // The reference profile info will contain the methods with indices 50-150.
  const uint16_t kNumberOfMethodsAlreadyCompiled = 100;
  ProfileCompilationInfo reference_info;
  SetupProfile(d1.GetLocation(), d1.GetLocationChecksum(), "p1", 1,
      kNumberOfMethodsAlreadyCompiled, 0, reference_profile,
      &reference_info, kNumberOfMethodsToEnableCompilation / 2);

  // Run profman and pass the dex file with --apk-fd.
  android::base::unique_fd apk_fd(
      open(GetTestDexFileName("ProfileTestMultiDex").c_str(), O_RDONLY));
  ASSERT_GE(apk_fd.get(), 0);

  std::string profman_cmd = GetProfmanCmd();
  std::vector<std::string> argv_str;
  argv_str.push_back(profman_cmd);
  argv_str.push_back("--profile-file-fd=" + std::to_string(profile1.GetFd()));
  argv_str.push_back("--profile-file-fd=" + std::to_string(profile2.GetFd()));
  argv_str.push_back("--reference-profile-file-fd=" + std::to_string(reference_profile.GetFd()));
  argv_str.push_back("--apk-fd=" + std::to_string(apk_fd.get()));
  std::string error;

  EXPECT_EQ(ExecAndReturnCode(argv_str, &error), 0) << error;

  // Verify that we can load the result.

  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile_fd));


  ASSERT_TRUE(profile1.GetFile()->ResetOffset());
  ASSERT_TRUE(profile2.GetFile()->ResetOffset());
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());

  // Verify that the result filtered out data not belonging to the dex file.
  // This is equivalent to checking that the result is equal to the merging of
  // all profiles while filtering out data not belonging to the dex file.

  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [&d1, &d2](const std::string& dex_location, uint32_t checksum) -> bool {
          return (dex_location == ProfileCompilationInfo::GetProfileDexFileKey(d1.GetLocation())
              && checksum == d1.GetLocationChecksum())
              || (dex_location == ProfileCompilationInfo::GetProfileDexFileKey(d2.GetLocation())
              && checksum == d2.GetLocationChecksum());
        };

  ProfileCompilationInfo info1_filter;
  ProfileCompilationInfo info2_filter;
  ProfileCompilationInfo expected;

  info2_filter.Load(profile1.GetFd(), /*merge_classes*/ true, filter_fn);
  info2_filter.Load(profile2.GetFd(), /*merge_classes*/ true, filter_fn);
  expected.Load(reference_profile.GetFd(), /*merge_classes*/ true, filter_fn);

  ASSERT_TRUE(expected.MergeWith(info1_filter));
  ASSERT_TRUE(expected.MergeWith(info2_filter));

  ASSERT_TRUE(expected.Equals(result));
}

TEST_F(ProfileAssistantTest, CopyAndUpdateProfileKey) {
  ScratchFile profile1;
  ScratchFile reference_profile;

  // Use a real dex file to generate profile test data. During the copy-and-update the
  // matching is done based on checksum so we have to match with the real thing.
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("ProfileTestMultiDex");
  const DexFile& d1 = *dex_files[0];
  const DexFile& d2 = *dex_files[1];

  ProfileCompilationInfo info1;
  uint16_t num_methods_to_add = std::min(d1.NumMethodIds(), d2.NumMethodIds());
  SetupProfile("fake-location1",
               d1.GetLocationChecksum(),
               "fake-location2",
               d2.GetLocationChecksum(),
               num_methods_to_add,
               /*num_classes*/ 0,
               profile1,
               &info1,
               /*start_method_index*/ 0,
               /*reverse_dex_write_order*/ false,
               /*number_of_methods1*/ d1.NumMethodIds(),
               /*number_of_methods2*/ d2.NumMethodIds());

  // Run profman and pass the dex file with --apk-fd.
  android::base::unique_fd apk_fd(
      open(GetTestDexFileName("ProfileTestMultiDex").c_str(), O_RDONLY));
  ASSERT_GE(apk_fd.get(), 0);

  std::string profman_cmd = GetProfmanCmd();
  std::vector<std::string> argv_str;
  argv_str.push_back(profman_cmd);
  argv_str.push_back("--profile-file-fd=" + std::to_string(profile1.GetFd()));
  argv_str.push_back("--reference-profile-file-fd=" + std::to_string(reference_profile.GetFd()));
  argv_str.push_back("--apk-fd=" + std::to_string(apk_fd.get()));
  argv_str.push_back("--copy-and-update-profile-key");
  std::string error;

  ASSERT_EQ(ExecAndReturnCode(argv_str, &error), 0) << error;

  // Verify that we can load the result.
  ProfileCompilationInfo result;
  ASSERT_TRUE(reference_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(result.Load(reference_profile.GetFd()));

  // Verify that the renaming was done.
  for (uint16_t i = 0; i < num_methods_to_add; i ++) {
      std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> pmi;
      ASSERT_TRUE(result.GetMethod(d1.GetLocation(), d1.GetLocationChecksum(), i) != nullptr) << i;
      ASSERT_TRUE(result.GetMethod(d2.GetLocation(), d2.GetLocationChecksum(), i) != nullptr) << i;

      ASSERT_TRUE(result.GetMethod("fake-location1", d1.GetLocationChecksum(), i) == nullptr);
      ASSERT_TRUE(result.GetMethod("fake-location2", d2.GetLocationChecksum(), i) == nullptr);
  }
}

}  // namespace art
