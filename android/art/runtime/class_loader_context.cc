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

#include "class_loader_context.h"

#include "art_field-inl.h"
#include "base/dchecked_vector.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "class_loader_utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "handle_scope-inl.h"
#include "jni_internal.h"
#include "oat_file_assistant.h"
#include "obj_ptr-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

static constexpr char kPathClassLoaderString[] = "PCL";
static constexpr char kDelegateLastClassLoaderString[] = "DLC";
static constexpr char kClassLoaderOpeningMark = '[';
static constexpr char kClassLoaderClosingMark = ']';
static constexpr char kClassLoaderSeparator = ';';
static constexpr char kClasspathSeparator = ':';
static constexpr char kDexFileChecksumSeparator = '*';

ClassLoaderContext::ClassLoaderContext()
    : special_shared_library_(false),
      dex_files_open_attempted_(false),
      dex_files_open_result_(false),
      owns_the_dex_files_(true) {}

ClassLoaderContext::ClassLoaderContext(bool owns_the_dex_files)
    : special_shared_library_(false),
      dex_files_open_attempted_(true),
      dex_files_open_result_(true),
      owns_the_dex_files_(owns_the_dex_files) {}

ClassLoaderContext::~ClassLoaderContext() {
  if (!owns_the_dex_files_) {
    // If the context does not own the dex/oat files release the unique pointers to
    // make sure we do not de-allocate them.
    for (ClassLoaderInfo& info : class_loader_chain_) {
      for (std::unique_ptr<OatFile>& oat_file : info.opened_oat_files) {
        oat_file.release();
      }
      for (std::unique_ptr<const DexFile>& dex_file : info.opened_dex_files) {
        dex_file.release();
      }
    }
  }
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::Default() {
  return Create("");
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::Create(const std::string& spec) {
  std::unique_ptr<ClassLoaderContext> result(new ClassLoaderContext());
  if (result->Parse(spec)) {
    return result;
  } else {
    return nullptr;
  }
}

// The expected format is: "ClassLoaderType1[ClasspathElem1*Checksum1:ClasspathElem2*Checksum2...]".
// The checksum part of the format is expected only if parse_cheksums is true.
bool ClassLoaderContext::ParseClassLoaderSpec(const std::string& class_loader_spec,
                                              ClassLoaderType class_loader_type,
                                              bool parse_checksums) {
  const char* class_loader_type_str = GetClassLoaderTypeName(class_loader_type);
  size_t type_str_size = strlen(class_loader_type_str);

  CHECK_EQ(0, class_loader_spec.compare(0, type_str_size, class_loader_type_str));

  // Check the opening and closing markers.
  if (class_loader_spec[type_str_size] != kClassLoaderOpeningMark) {
    return false;
  }
  if (class_loader_spec[class_loader_spec.length() - 1] != kClassLoaderClosingMark) {
    return false;
  }

  // At this point we know the format is ok; continue and extract the classpath.
  // Note that class loaders with an empty class path are allowed.
  std::string classpath = class_loader_spec.substr(type_str_size + 1,
                                                   class_loader_spec.length() - type_str_size - 2);

  class_loader_chain_.push_back(ClassLoaderInfo(class_loader_type));

  if (!parse_checksums) {
    Split(classpath, kClasspathSeparator, &class_loader_chain_.back().classpath);
  } else {
    std::vector<std::string> classpath_elements;
    Split(classpath, kClasspathSeparator, &classpath_elements);
    for (const std::string& element : classpath_elements) {
      std::vector<std::string> dex_file_with_checksum;
      Split(element, kDexFileChecksumSeparator, &dex_file_with_checksum);
      if (dex_file_with_checksum.size() != 2) {
        return false;
      }
      uint32_t checksum = 0;
      if (!ParseInt(dex_file_with_checksum[1].c_str(), &checksum)) {
        return false;
      }
      class_loader_chain_.back().classpath.push_back(dex_file_with_checksum[0]);
      class_loader_chain_.back().checksums.push_back(checksum);
    }
  }

  return true;
}

// Extracts the class loader type from the given spec.
// Return ClassLoaderContext::kInvalidClassLoader if the class loader type is not
// recognized.
ClassLoaderContext::ClassLoaderType
ClassLoaderContext::ExtractClassLoaderType(const std::string& class_loader_spec) {
  const ClassLoaderType kValidTypes[] = {kPathClassLoader, kDelegateLastClassLoader};
  for (const ClassLoaderType& type : kValidTypes) {
    const char* type_str = GetClassLoaderTypeName(type);
    if (class_loader_spec.compare(0, strlen(type_str), type_str) == 0) {
      return type;
    }
  }
  return kInvalidClassLoader;
}

// The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
// ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
// ClasspathElem is the path of dex/jar/apk file.
bool ClassLoaderContext::Parse(const std::string& spec, bool parse_checksums) {
  if (spec.empty()) {
    // By default we load the dex files in a PathClassLoader.
    // So an empty spec is equivalent to an empty PathClassLoader (this happens when running
    // tests)
    class_loader_chain_.push_back(ClassLoaderInfo(kPathClassLoader));
    return true;
  }

  // Stop early if we detect the special shared library, which may be passed as the classpath
  // for dex2oat when we want to skip the shared libraries check.
  if (spec == OatFile::kSpecialSharedLibrary) {
    LOG(INFO) << "The ClassLoaderContext is a special shared library.";
    special_shared_library_ = true;
    return true;
  }

  std::vector<std::string> class_loaders;
  Split(spec, kClassLoaderSeparator, &class_loaders);

  for (const std::string& class_loader : class_loaders) {
    ClassLoaderType type = ExtractClassLoaderType(class_loader);
    if (type == kInvalidClassLoader) {
      LOG(ERROR) << "Invalid class loader type: " << class_loader;
      return false;
    }
    if (!ParseClassLoaderSpec(class_loader, type, parse_checksums)) {
      LOG(ERROR) << "Invalid class loader spec: " << class_loader;
      return false;
    }
  }
  return true;
}

// Opens requested class path files and appends them to opened_dex_files. If the dex files have
// been stripped, this opens them from their oat files (which get added to opened_oat_files).
bool ClassLoaderContext::OpenDexFiles(InstructionSet isa, const std::string& classpath_dir) {
  if (dex_files_open_attempted_) {
    // Do not attempt to re-open the files if we already tried.
    return dex_files_open_result_;
  }

  dex_files_open_attempted_ = true;
  // Assume we can open all dex files. If not, we will set this to false as we go.
  dex_files_open_result_ = true;

  if (special_shared_library_) {
    // Nothing to open if the context is a special shared library.
    return true;
  }

  // Note that we try to open all dex files even if some fail.
  // We may get resource-only apks which we cannot load.
  // TODO(calin): Refine the dex opening interface to be able to tell if an archive contains
  // no dex files. So that we can distinguish the real failures...
  const ArtDexFileLoader dex_file_loader;
  for (ClassLoaderInfo& info : class_loader_chain_) {
    size_t opened_dex_files_index = info.opened_dex_files.size();
    for (const std::string& cp_elem : info.classpath) {
      // If path is relative, append it to the provided base directory.
      std::string location = cp_elem;
      if (location[0] != '/' && !classpath_dir.empty()) {
        location = classpath_dir + (classpath_dir.back() == '/' ? "" : "/") + location;
      }

      std::string error_msg;
      // When opening the dex files from the context we expect their checksum to match their
      // contents. So pass true to verify_checksum.
      if (!dex_file_loader.Open(location.c_str(),
                                location.c_str(),
                                Runtime::Current()->IsVerificationEnabled(),
                                /*verify_checksum*/ true,
                                &error_msg,
                                &info.opened_dex_files)) {
        // If we fail to open the dex file because it's been stripped, try to open the dex file
        // from its corresponding oat file.
        // This could happen when we need to recompile a pre-build whose dex code has been stripped.
        // (for example, if the pre-build is only quicken and we want to re-compile it
        // speed-profile).
        // TODO(calin): Use the vdex directly instead of going through the oat file.
        OatFileAssistant oat_file_assistant(location.c_str(), isa, false);
        std::unique_ptr<OatFile> oat_file(oat_file_assistant.GetBestOatFile());
        std::vector<std::unique_ptr<const DexFile>> oat_dex_files;
        if (oat_file != nullptr &&
            OatFileAssistant::LoadDexFiles(*oat_file, location, &oat_dex_files)) {
          info.opened_oat_files.push_back(std::move(oat_file));
          info.opened_dex_files.insert(info.opened_dex_files.end(),
                                       std::make_move_iterator(oat_dex_files.begin()),
                                       std::make_move_iterator(oat_dex_files.end()));
        } else {
          LOG(WARNING) << "Could not open dex files from location: " << location;
          dex_files_open_result_ = false;
        }
      }
    }

    // We finished opening the dex files from the classpath.
    // Now update the classpath and the checksum with the locations of the dex files.
    //
    // We do this because initially the classpath contains the paths of the dex files; and
    // some of them might be multi-dexes. So in order to have a consistent view we replace all the
    // file paths with the actual dex locations being loaded.
    // This will allow the context to VerifyClassLoaderContextMatch which expects or multidex
    // location in the class paths.
    // Note that this will also remove the paths that could not be opened.
    info.original_classpath = std::move(info.classpath);
    info.classpath.clear();
    info.checksums.clear();
    for (size_t k = opened_dex_files_index; k < info.opened_dex_files.size(); k++) {
      std::unique_ptr<const DexFile>& dex = info.opened_dex_files[k];
      info.classpath.push_back(dex->GetLocation());
      info.checksums.push_back(dex->GetLocationChecksum());
    }
  }

  return dex_files_open_result_;
}

bool ClassLoaderContext::RemoveLocationsFromClassPaths(
    const dchecked_vector<std::string>& locations) {
  CHECK(!dex_files_open_attempted_)
      << "RemoveLocationsFromClasspaths cannot be call after OpenDexFiles";

  std::set<std::string> canonical_locations;
  for (const std::string& location : locations) {
    canonical_locations.insert(DexFileLoader::GetDexCanonicalLocation(location.c_str()));
  }
  bool removed_locations = false;
  for (ClassLoaderInfo& info : class_loader_chain_) {
    size_t initial_size = info.classpath.size();
    auto kept_it = std::remove_if(
        info.classpath.begin(),
        info.classpath.end(),
        [canonical_locations](const std::string& location) {
            return ContainsElement(canonical_locations,
                                   DexFileLoader::GetDexCanonicalLocation(location.c_str()));
        });
    info.classpath.erase(kept_it, info.classpath.end());
    if (initial_size != info.classpath.size()) {
      removed_locations = true;
    }
  }
  return removed_locations;
}

std::string ClassLoaderContext::EncodeContextForDex2oat(const std::string& base_dir) const {
  return EncodeContext(base_dir, /*for_dex2oat*/ true, /*stored_context*/ nullptr);
}

std::string ClassLoaderContext::EncodeContextForOatFile(const std::string& base_dir,
                                                        ClassLoaderContext* stored_context) const {
  return EncodeContext(base_dir, /*for_dex2oat*/ false, stored_context);
}

std::string ClassLoaderContext::EncodeContext(const std::string& base_dir,
                                              bool for_dex2oat,
                                              ClassLoaderContext* stored_context) const {
  CheckDexFilesOpened("EncodeContextForOatFile");
  if (special_shared_library_) {
    return OatFile::kSpecialSharedLibrary;
  }

  if (stored_context != nullptr) {
    DCHECK_EQ(class_loader_chain_.size(), stored_context->class_loader_chain_.size());
  }

  std::ostringstream out;
  if (class_loader_chain_.empty()) {
    // We can get in this situation if the context was created with a class path containing the
    // source dex files which were later removed (happens during run-tests).
    out << GetClassLoaderTypeName(kPathClassLoader)
        << kClassLoaderOpeningMark
        << kClassLoaderClosingMark;
    return out.str();
  }

  for (size_t i = 0; i < class_loader_chain_.size(); i++) {
    const ClassLoaderInfo& info = class_loader_chain_[i];
    if (i > 0) {
      out << kClassLoaderSeparator;
    }
    out << GetClassLoaderTypeName(info.type);
    out << kClassLoaderOpeningMark;
    std::set<std::string> seen_locations;
    SafeMap<std::string, std::string> remap;
    if (stored_context != nullptr) {
      DCHECK_EQ(info.original_classpath.size(),
                stored_context->class_loader_chain_[i].classpath.size());
      for (size_t k = 0; k < info.original_classpath.size(); ++k) {
        // Note that we don't care if the same name appears twice.
        remap.Put(info.original_classpath[k], stored_context->class_loader_chain_[i].classpath[k]);
      }
    }
    for (size_t k = 0; k < info.opened_dex_files.size(); k++) {
      const std::unique_ptr<const DexFile>& dex_file = info.opened_dex_files[k];
      if (for_dex2oat) {
        // dex2oat only needs the base location. It cannot accept multidex locations.
        // So ensure we only add each file once.
        bool new_insert = seen_locations.insert(
            DexFileLoader::GetBaseLocation(dex_file->GetLocation())).second;
        if (!new_insert) {
          continue;
        }
      }
      std::string location = dex_file->GetLocation();
      // If there is a stored class loader remap, fix up the multidex strings.
      if (!remap.empty()) {
        std::string base_dex_location = DexFileLoader::GetBaseLocation(location);
        auto it = remap.find(base_dex_location);
        CHECK(it != remap.end()) << base_dex_location;
        location = it->second + DexFileLoader::GetMultiDexSuffix(location);
      }
      if (k > 0) {
        out << kClasspathSeparator;
      }
      // Find paths that were relative and convert them back from absolute.
      if (!base_dir.empty() && location.substr(0, base_dir.length()) == base_dir) {
        out << location.substr(base_dir.length() + 1).c_str();
      } else {
        out << location.c_str();
      }
      // dex2oat does not need the checksums.
      if (!for_dex2oat) {
        out << kDexFileChecksumSeparator;
        out << dex_file->GetLocationChecksum();
      }
    }
    out << kClassLoaderClosingMark;
  }
  return out.str();
}

jobject ClassLoaderContext::CreateClassLoader(
    const std::vector<const DexFile*>& compilation_sources) const {
  CheckDexFilesOpened("CreateClassLoader");

  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();

  if (class_loader_chain_.empty()) {
    return class_linker->CreatePathClassLoader(self, compilation_sources);
  }

  // Create the class loaders starting from the top most parent (the one on the last position
  // in the chain) but omit the first class loader which will contain the compilation_sources and
  // needs special handling.
  jobject current_parent = nullptr;  // the starting parent is the BootClassLoader.
  for (size_t i = class_loader_chain_.size() - 1; i > 0; i--) {
    std::vector<const DexFile*> class_path_files = MakeNonOwningPointerVector(
        class_loader_chain_[i].opened_dex_files);
    current_parent = class_linker->CreateWellKnownClassLoader(
        self,
        class_path_files,
        GetClassLoaderClass(class_loader_chain_[i].type),
        current_parent);
  }

  // We set up all the parents. Move on to create the first class loader.
  // Its classpath comes first, followed by compilation sources. This ensures that whenever
  // we need to resolve classes from it the classpath elements come first.

  std::vector<const DexFile*> first_class_loader_classpath = MakeNonOwningPointerVector(
      class_loader_chain_[0].opened_dex_files);
  first_class_loader_classpath.insert(first_class_loader_classpath.end(),
                                    compilation_sources.begin(),
                                    compilation_sources.end());

  return class_linker->CreateWellKnownClassLoader(
      self,
      first_class_loader_classpath,
      GetClassLoaderClass(class_loader_chain_[0].type),
      current_parent);
}

std::vector<const DexFile*> ClassLoaderContext::FlattenOpenedDexFiles() const {
  CheckDexFilesOpened("FlattenOpenedDexFiles");

  std::vector<const DexFile*> result;
  for (const ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::unique_ptr<const DexFile>& dex_file : info.opened_dex_files) {
      result.push_back(dex_file.get());
    }
  }
  return result;
}

const char* ClassLoaderContext::GetClassLoaderTypeName(ClassLoaderType type) {
  switch (type) {
    case kPathClassLoader: return kPathClassLoaderString;
    case kDelegateLastClassLoader: return kDelegateLastClassLoaderString;
    default:
      LOG(FATAL) << "Invalid class loader type " << type;
      UNREACHABLE();
  }
}

void ClassLoaderContext::CheckDexFilesOpened(const std::string& calling_method) const {
  CHECK(dex_files_open_attempted_)
      << "Dex files were not successfully opened before the call to " << calling_method
      << "attempt=" << dex_files_open_attempted_ << ", result=" << dex_files_open_result_;
}

// Collects the dex files from the give Java dex_file object. Only the dex files with
// at least 1 class are collected. If a null java_dex_file is passed this method does nothing.
static bool CollectDexFilesFromJavaDexFile(ObjPtr<mirror::Object> java_dex_file,
                                           ArtField* const cookie_field,
                                           std::vector<const DexFile*>* out_dex_files)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  if (java_dex_file == nullptr) {
    return true;
  }
  // On the Java side, the dex files are stored in the cookie field.
  mirror::LongArray* long_array = cookie_field->GetObject(java_dex_file)->AsLongArray();
  if (long_array == nullptr) {
    // This should never happen so log a warning.
    LOG(ERROR) << "Unexpected null cookie";
    return false;
  }
  int32_t long_array_size = long_array->GetLength();
  // Index 0 from the long array stores the oat file. The dex files start at index 1.
  for (int32_t j = 1; j < long_array_size; ++j) {
    const DexFile* cp_dex_file = reinterpret_cast<const DexFile*>(static_cast<uintptr_t>(
        long_array->GetWithoutChecks(j)));
    if (cp_dex_file != nullptr && cp_dex_file->NumClassDefs() > 0) {
      // TODO(calin): It's unclear why the dex files with no classes are skipped here and when
      // cp_dex_file can be null.
      out_dex_files->push_back(cp_dex_file);
    }
  }
  return true;
}

// Collects all the dex files loaded by the given class loader.
// Returns true for success or false if an unexpected state is discovered (e.g. a null dex cookie,
// a null list of dex elements or a null dex element).
static bool CollectDexFilesFromSupportedClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                                    Handle<mirror::ClassLoader> class_loader,
                                                    std::vector<const DexFile*>* out_dex_files)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  CHECK(IsPathOrDexClassLoader(soa, class_loader) || IsDelegateLastClassLoader(soa, class_loader));

  // All supported class loaders inherit from BaseDexClassLoader.
  // We need to get the DexPathList and loop through it.
  ArtField* const cookie_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexFile_cookie);
  ArtField* const dex_file_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);
  ObjPtr<mirror::Object> dex_path_list =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_pathList)->
          GetObject(class_loader.Get());
  CHECK(cookie_field != nullptr);
  CHECK(dex_file_field != nullptr);
  if (dex_path_list == nullptr) {
    // This may be null if the current class loader is under construction and it does not
    // have its fields setup yet.
    return true;
  }
  // DexPathList has an array dexElements of Elements[] which each contain a dex file.
  ObjPtr<mirror::Object> dex_elements_obj =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList_dexElements)->
          GetObject(dex_path_list);
  // Loop through each dalvik.system.DexPathList$Element's dalvik.system.DexFile and look
  // at the mCookie which is a DexFile vector.
  if (dex_elements_obj == nullptr) {
    // TODO(calin): It's unclear if we should just assert here. For now be prepared for the worse
    // and assume we have no elements.
    return true;
  } else {
    StackHandleScope<1> hs(soa.Self());
    Handle<mirror::ObjectArray<mirror::Object>> dex_elements(
        hs.NewHandle(dex_elements_obj->AsObjectArray<mirror::Object>()));
    for (int32_t i = 0; i < dex_elements->GetLength(); ++i) {
      mirror::Object* element = dex_elements->GetWithoutChecks(i);
      if (element == nullptr) {
        // Should never happen, log an error and break.
        // TODO(calin): It's unclear if we should just assert here.
        // This code was propagated to oat_file_manager from the class linker where it would
        // throw a NPE. For now, return false which will mark this class loader as unsupported.
        LOG(ERROR) << "Unexpected null in the dex element list";
        return false;
      }
      ObjPtr<mirror::Object> dex_file = dex_file_field->GetObject(element);
      if (!CollectDexFilesFromJavaDexFile(dex_file, cookie_field, out_dex_files)) {
        return false;
      }
    }
  }

  return true;
}

static bool GetDexFilesFromDexElementsArray(
    ScopedObjectAccessAlreadyRunnable& soa,
    Handle<mirror::ObjectArray<mirror::Object>> dex_elements,
    std::vector<const DexFile*>* out_dex_files) REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(dex_elements != nullptr);

  ArtField* const cookie_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexFile_cookie);
  ArtField* const dex_file_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);
  ObjPtr<mirror::Class> const element_class = soa.Decode<mirror::Class>(
      WellKnownClasses::dalvik_system_DexPathList__Element);
  ObjPtr<mirror::Class> const dexfile_class = soa.Decode<mirror::Class>(
      WellKnownClasses::dalvik_system_DexFile);

  for (int32_t i = 0; i < dex_elements->GetLength(); ++i) {
    mirror::Object* element = dex_elements->GetWithoutChecks(i);
    // We can hit a null element here because this is invoked with a partially filled dex_elements
    // array from DexPathList. DexPathList will open each dex sequentially, each time passing the
    // list of dex files which were opened before.
    if (element == nullptr) {
      continue;
    }

    // We support this being dalvik.system.DexPathList$Element and dalvik.system.DexFile.
    // TODO(calin): Code caried over oat_file_manager: supporting both classes seem to be
    // a historical glitch. All the java code opens dex files using an array of Elements.
    ObjPtr<mirror::Object> dex_file;
    if (element_class == element->GetClass()) {
      dex_file = dex_file_field->GetObject(element);
    } else if (dexfile_class == element->GetClass()) {
      dex_file = element;
    } else {
      LOG(ERROR) << "Unsupported element in dex_elements: "
                 << mirror::Class::PrettyClass(element->GetClass());
      return false;
    }

    if (!CollectDexFilesFromJavaDexFile(dex_file, cookie_field, out_dex_files)) {
      return false;
    }
  }
  return true;
}

// Adds the `class_loader` info to the `context`.
// The dex file present in `dex_elements` array (if not null) will be added at the end of
// the classpath.
// This method is recursive (w.r.t. the class loader parent) and will stop once it reaches the
// BootClassLoader. Note that the class loader chain is expected to be short.
bool ClassLoaderContext::AddInfoToContextFromClassLoader(
      ScopedObjectAccessAlreadyRunnable& soa,
      Handle<mirror::ClassLoader> class_loader,
      Handle<mirror::ObjectArray<mirror::Object>> dex_elements)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (ClassLinker::IsBootClassLoader(soa, class_loader.Get())) {
    // Nothing to do for the boot class loader as we don't add its dex files to the context.
    return true;
  }

  ClassLoaderContext::ClassLoaderType type;
  if (IsPathOrDexClassLoader(soa, class_loader)) {
    type = kPathClassLoader;
  } else if (IsDelegateLastClassLoader(soa, class_loader)) {
    type = kDelegateLastClassLoader;
  } else {
    LOG(WARNING) << "Unsupported class loader";
    return false;
  }

  // Inspect the class loader for its dex files.
  std::vector<const DexFile*> dex_files_loaded;
  CollectDexFilesFromSupportedClassLoader(soa, class_loader, &dex_files_loaded);

  // If we have a dex_elements array extract its dex elements now.
  // This is used in two situations:
  //   1) when a new ClassLoader is created DexPathList will open each dex file sequentially
  //      passing the list of already open dex files each time. This ensures that we see the
  //      correct context even if the ClassLoader under construction is not fully build.
  //   2) when apk splits are loaded on the fly, the framework will load their dex files by
  //      appending them to the current class loader. When the new code paths are loaded in
  //      BaseDexClassLoader, the paths already present in the class loader will be passed
  //      in the dex_elements array.
  if (dex_elements != nullptr) {
    GetDexFilesFromDexElementsArray(soa, dex_elements, &dex_files_loaded);
  }

  class_loader_chain_.push_back(ClassLoaderContext::ClassLoaderInfo(type));
  ClassLoaderInfo& info = class_loader_chain_.back();
  for (const DexFile* dex_file : dex_files_loaded) {
    info.classpath.push_back(dex_file->GetLocation());
    info.checksums.push_back(dex_file->GetLocationChecksum());
    info.opened_dex_files.emplace_back(dex_file);
  }

  // We created the ClassLoaderInfo for the current loader. Move on to its parent.

  StackHandleScope<1> hs(Thread::Current());
  Handle<mirror::ClassLoader> parent = hs.NewHandle(class_loader->GetParent());

  // Note that dex_elements array is null here. The elements are considered to be part of the
  // current class loader and are not passed to the parents.
  ScopedNullHandle<mirror::ObjectArray<mirror::Object>> null_dex_elements;
  return AddInfoToContextFromClassLoader(soa, parent, null_dex_elements);
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::CreateContextForClassLoader(
    jobject class_loader,
    jobjectArray dex_elements) {
  CHECK(class_loader != nullptr);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> h_class_loader =
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader));
  Handle<mirror::ObjectArray<mirror::Object>> h_dex_elements =
      hs.NewHandle(soa.Decode<mirror::ObjectArray<mirror::Object>>(dex_elements));

  std::unique_ptr<ClassLoaderContext> result(new ClassLoaderContext(/*owns_the_dex_files*/ false));
  if (result->AddInfoToContextFromClassLoader(soa, h_class_loader, h_dex_elements)) {
    return result;
  } else {
    return nullptr;
  }
}

static bool IsAbsoluteLocation(const std::string& location) {
  return !location.empty() && location[0] == '/';
}

bool ClassLoaderContext::VerifyClassLoaderContextMatch(const std::string& context_spec,
                                                       bool verify_names,
                                                       bool verify_checksums) const {
  if (verify_names || verify_checksums) {
    DCHECK(dex_files_open_attempted_);
    DCHECK(dex_files_open_result_);
  }

  ClassLoaderContext expected_context;
  if (!expected_context.Parse(context_spec, verify_checksums)) {
    LOG(WARNING) << "Invalid class loader context: " << context_spec;
    return false;
  }

  // Special shared library contexts always match. They essentially instruct the runtime
  // to ignore the class path check because the oat file is known to be loaded in different
  // contexts. OatFileManager will further verify if the oat file can be loaded based on the
  // collision check.
  if (special_shared_library_ || expected_context.special_shared_library_) {
    return true;
  }

  if (expected_context.class_loader_chain_.size() != class_loader_chain_.size()) {
    LOG(WARNING) << "ClassLoaderContext size mismatch. expected="
        << expected_context.class_loader_chain_.size()
        << ", actual=" << class_loader_chain_.size()
        << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
    return false;
  }

  for (size_t i = 0; i < class_loader_chain_.size(); i++) {
    const ClassLoaderInfo& info = class_loader_chain_[i];
    const ClassLoaderInfo& expected_info = expected_context.class_loader_chain_[i];
    if (info.type != expected_info.type) {
      LOG(WARNING) << "ClassLoaderContext type mismatch for position " << i
          << ". expected=" << GetClassLoaderTypeName(expected_info.type)
          << ", found=" << GetClassLoaderTypeName(info.type)
          << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
      return false;
    }
    if (info.classpath.size() != expected_info.classpath.size()) {
      LOG(WARNING) << "ClassLoaderContext classpath size mismatch for position " << i
            << ". expected=" << expected_info.classpath.size()
            << ", found=" << info.classpath.size()
            << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
      return false;
    }

    if (verify_checksums) {
      DCHECK_EQ(info.classpath.size(), info.checksums.size());
      DCHECK_EQ(expected_info.classpath.size(), expected_info.checksums.size());
    }

    if (!verify_names) {
      continue;
    }

    for (size_t k = 0; k < info.classpath.size(); k++) {
      // Compute the dex location that must be compared.
      // We shouldn't do a naive comparison `info.classpath[k] == expected_info.classpath[k]`
      // because even if they refer to the same file, one could be encoded as a relative location
      // and the other as an absolute one.
      bool is_dex_name_absolute = IsAbsoluteLocation(info.classpath[k]);
      bool is_expected_dex_name_absolute = IsAbsoluteLocation(expected_info.classpath[k]);
      std::string dex_name;
      std::string expected_dex_name;

      if (is_dex_name_absolute == is_expected_dex_name_absolute) {
        // If both locations are absolute or relative then compare them as they are.
        // This is usually the case for: shared libraries and secondary dex files.
        dex_name = info.classpath[k];
        expected_dex_name = expected_info.classpath[k];
      } else if (is_dex_name_absolute) {
        // The runtime name is absolute but the compiled name (the expected one) is relative.
        // This is the case for split apks which depend on base or on other splits.
        dex_name = info.classpath[k];
        expected_dex_name = OatFile::ResolveRelativeEncodedDexLocation(
            info.classpath[k].c_str(), expected_info.classpath[k]);
      } else if (is_expected_dex_name_absolute) {
        // The runtime name is relative but the compiled name is absolute.
        // There is no expected use case that would end up here as dex files are always loaded
        // with their absolute location. However, be tolerant and do the best effort (in case
        // there are unexpected new use case...).
        dex_name = OatFile::ResolveRelativeEncodedDexLocation(
            expected_info.classpath[k].c_str(), info.classpath[k]);
        expected_dex_name = expected_info.classpath[k];
      } else {
        // Both locations are relative. In this case there's not much we can be sure about
        // except that the names are the same. The checksum will ensure that the files are
        // are same. This should not happen outside testing and manual invocations.
        dex_name = info.classpath[k];
        expected_dex_name = expected_info.classpath[k];
      }

      // Compare the locations.
      if (dex_name != expected_dex_name) {
        LOG(WARNING) << "ClassLoaderContext classpath element mismatch for position " << i
            << ". expected=" << expected_info.classpath[k]
            << ", found=" << info.classpath[k]
            << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
        return false;
      }

      // Compare the checksums.
      if (info.checksums[k] != expected_info.checksums[k]) {
        LOG(WARNING) << "ClassLoaderContext classpath element checksum mismatch for position " << i
                     << ". expected=" << expected_info.checksums[k]
                     << ", found=" << info.checksums[k]
                     << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
        return false;
      }
    }
  }
  return true;
}

jclass ClassLoaderContext::GetClassLoaderClass(ClassLoaderType type) {
  switch (type) {
    case kPathClassLoader: return WellKnownClasses::dalvik_system_PathClassLoader;
    case kDelegateLastClassLoader: return WellKnownClasses::dalvik_system_DelegateLastClassLoader;
    case kInvalidClassLoader: break;  // will fail after the switch.
  }
  LOG(FATAL) << "Invalid class loader type " << type;
  UNREACHABLE();
}

}  // namespace art
