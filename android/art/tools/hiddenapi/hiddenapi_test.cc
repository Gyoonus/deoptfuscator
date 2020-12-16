/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <fstream>

#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "exec_utils.h"
#include "zip_archive.h"

namespace art {

class HiddenApiTest : public CommonRuntimeTest {
 protected:
  std::string GetHiddenApiCmd() {
    std::string file_path = GetTestAndroidRoot();
    file_path += "/bin/hiddenapi";
    if (kIsDebugBuild) {
      file_path += "d";
    }
    if (!OS::FileExists(file_path.c_str())) {
      LOG(FATAL) << "Could not find binary " << file_path;
      UNREACHABLE();
    }
    return file_path;
  }

  std::unique_ptr<const DexFile> RunHiddenApi(const ScratchFile& light_greylist,
                                              const ScratchFile& dark_greylist,
                                              const ScratchFile& blacklist,
                                              const std::vector<std::string>& extra_args,
                                              ScratchFile* out_dex) {
    std::string error;
    std::unique_ptr<ZipArchive> jar(
        ZipArchive::Open(GetTestDexFileName("HiddenApi").c_str(), &error));
    if (jar == nullptr) {
      LOG(FATAL) << "Could not open test file " << GetTestDexFileName("HiddenApi") << ": " << error;
      UNREACHABLE();
    }
    std::unique_ptr<ZipEntry> jar_classes_dex(jar->Find("classes.dex", &error));
    if (jar_classes_dex == nullptr) {
      LOG(FATAL) << "Could not find classes.dex in test file " << GetTestDexFileName("HiddenApi")
                 << ": " << error;
      UNREACHABLE();
    } else if (!jar_classes_dex->ExtractToFile(*out_dex->GetFile(), &error)) {
      LOG(FATAL) << "Could not extract classes.dex from test file "
                 << GetTestDexFileName("HiddenApi") << ": " << error;
      UNREACHABLE();
    }

    std::vector<std::string> argv_str;
    argv_str.push_back(GetHiddenApiCmd());
    argv_str.insert(argv_str.end(), extra_args.begin(), extra_args.end());
    argv_str.push_back("--dex=" + out_dex->GetFilename());
    argv_str.push_back("--light-greylist=" + light_greylist.GetFilename());
    argv_str.push_back("--dark-greylist=" + dark_greylist.GetFilename());
    argv_str.push_back("--blacklist=" + blacklist.GetFilename());
    int return_code = ExecAndReturnCode(argv_str, &error);
    if (return_code != 0) {
      LOG(FATAL) << "HiddenApi binary exited with unexpected return code " << return_code;
    }
    return OpenDex(*out_dex);
  }

  std::unique_ptr<const DexFile> OpenDex(const ScratchFile& file) {
    ArtDexFileLoader dex_loader;
    std::string error_msg;

    File fd(file.GetFilename(), O_RDONLY, /* check_usage */ false);
    if (fd.Fd() == -1) {
      LOG(FATAL) << "Unable to open file '" << file.GetFilename() << "': " << strerror(errno);
      UNREACHABLE();
    }

    std::unique_ptr<const DexFile> dex_file(dex_loader.OpenDex(
        fd.Release(), /* location */ file.GetFilename(), /* verify */ false,
        /* verify_checksum */ true, /* mmap_shared */ false, &error_msg));
    if (dex_file.get() == nullptr) {
      LOG(FATAL) << "Open failed for '" << file.GetFilename() << "' " << error_msg;
      UNREACHABLE();
    } else if (!dex_file->IsStandardDexFile()) {
      LOG(FATAL) << "Expected a standard dex file '" << file.GetFilename() << "'";
      UNREACHABLE();
    }

    return dex_file;
  }

  std::ofstream OpenStream(const ScratchFile& file) {
    std::ofstream ofs(file.GetFilename(), std::ofstream::out);
    if (ofs.fail()) {
      LOG(FATAL) << "Open failed for '" << file.GetFilename() << "' " << strerror(errno);
      UNREACHABLE();
    }
    return ofs;
  }

  const DexFile::ClassDef& FindClass(const char* desc, const DexFile& dex_file) {
    for (uint32_t i = 0; i < dex_file.NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(i);
      if (strcmp(desc, dex_file.GetClassDescriptor(class_def)) == 0) {
        return class_def;
      }
    }
    LOG(FATAL) << "Could not find class " << desc;
    UNREACHABLE();
  }

  HiddenApiAccessFlags::ApiList GetFieldHiddenFlags(const char* name,
                                                    uint32_t expected_visibility,
                                                    const DexFile::ClassDef& class_def,
                                                    const DexFile& dex_file) {
    const uint8_t* class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {
      LOG(FATAL) << "Class " << dex_file.GetClassDescriptor(class_def) << " has no data";
      UNREACHABLE();
    }

    for (ClassDataItemIterator it(dex_file, class_data); it.HasNext(); it.Next()) {
      if (it.IsAtMethod()) {
        break;
      }
      const DexFile::FieldId& fid = dex_file.GetFieldId(it.GetMemberIndex());
      if (strcmp(name, dex_file.GetFieldName(fid)) == 0) {
        uint32_t actual_visibility = it.GetFieldAccessFlags() & kAccVisibilityFlags;
        if (actual_visibility != expected_visibility) {
          LOG(FATAL) << "Field " << name << " in class " << dex_file.GetClassDescriptor(class_def)
                     << " does not have the expected visibility flags (" << expected_visibility
                     << " != " << actual_visibility << ")";
          UNREACHABLE();
        }
        return it.DecodeHiddenAccessFlags();
      }
    }

    LOG(FATAL) << "Could not find field " << name << " in class "
               << dex_file.GetClassDescriptor(class_def);
    UNREACHABLE();
  }

  HiddenApiAccessFlags::ApiList GetMethodHiddenFlags(const char* name,
                                                     uint32_t expected_visibility,
                                                     bool expected_native,
                                                     const DexFile::ClassDef& class_def,
                                                     const DexFile& dex_file) {
    const uint8_t* class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {
      LOG(FATAL) << "Class " << dex_file.GetClassDescriptor(class_def) << " has no data";
      UNREACHABLE();
    }

    for (ClassDataItemIterator it(dex_file, class_data); it.HasNext(); it.Next()) {
      if (!it.IsAtMethod()) {
        continue;
      }
      const DexFile::MethodId& mid = dex_file.GetMethodId(it.GetMemberIndex());
      if (strcmp(name, dex_file.GetMethodName(mid)) == 0) {
        if (expected_native != it.MemberIsNative()) {
          LOG(FATAL) << "Expected native=" << expected_native << " for method " << name
                     << " in class " << dex_file.GetClassDescriptor(class_def);
          UNREACHABLE();
        }
        uint32_t actual_visibility = it.GetMethodAccessFlags() & kAccVisibilityFlags;
        if (actual_visibility != expected_visibility) {
          LOG(FATAL) << "Method " << name << " in class " << dex_file.GetClassDescriptor(class_def)
                     << " does not have the expected visibility flags (" << expected_visibility
                     << " != " << actual_visibility << ")";
          UNREACHABLE();
        }
        return it.DecodeHiddenAccessFlags();
      }
    }

    LOG(FATAL) << "Could not find method " << name << " in class "
               << dex_file.GetClassDescriptor(class_def);
    UNREACHABLE();
  }

  HiddenApiAccessFlags::ApiList GetIFieldHiddenFlags(const DexFile& dex_file) {
    return GetFieldHiddenFlags("ifield", kAccPublic, FindClass("LMain;", dex_file), dex_file);
  }

  HiddenApiAccessFlags::ApiList GetSFieldHiddenFlags(const DexFile& dex_file) {
    return GetFieldHiddenFlags("sfield", kAccPrivate, FindClass("LMain;", dex_file), dex_file);
  }

  HiddenApiAccessFlags::ApiList GetIMethodHiddenFlags(const DexFile& dex_file) {
    return GetMethodHiddenFlags(
        "imethod", 0, /* native */ false, FindClass("LMain;", dex_file), dex_file);
  }

  HiddenApiAccessFlags::ApiList GetSMethodHiddenFlags(const DexFile& dex_file) {
    return GetMethodHiddenFlags(
        "smethod", kAccPublic, /* native */ false, FindClass("LMain;", dex_file), dex_file);
  }

  HiddenApiAccessFlags::ApiList GetINMethodHiddenFlags(const DexFile& dex_file) {
    return GetMethodHiddenFlags(
        "inmethod", kAccPublic, /* native */ true, FindClass("LMain;", dex_file), dex_file);
  }

  HiddenApiAccessFlags::ApiList GetSNMethodHiddenFlags(const DexFile& dex_file) {
    return GetMethodHiddenFlags(
        "snmethod", kAccProtected, /* native */ true, FindClass("LMain;", dex_file), dex_file);
  }
};

TEST_F(HiddenApiTest, InstanceFieldNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:I" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:I" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:I" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(dark_greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetIFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:LBadType2;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(dark_greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType3;" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSFieldHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(J)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(J)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(J)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetIMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(C)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(C)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(C)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetINMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodNoMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kWhitelist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodLightGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kLightGreylist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodDarkGreylistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodBlacklistMatch) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodTwoListsMatch1) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodTwoListsMatch2) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kBlacklist, GetSNMethodHiddenFlags(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodTwoListsMatch3) {
  ScratchFile dex, light_greylist, dark_greylist, blacklist;
  OpenStream(light_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(dark_greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType3;)V" << std::endl;
  auto dex_file = RunHiddenApi(light_greylist, dark_greylist, blacklist, {}, &dex);
  ASSERT_EQ(HiddenApiAccessFlags::kDarkGreylist, GetSNMethodHiddenFlags(*dex_file));
}

}  // namespace art
