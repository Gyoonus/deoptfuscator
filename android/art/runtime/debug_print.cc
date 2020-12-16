/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <sstream>

#include "debug_print.h"

#include "class_linker.h"
#include "class_table.h"
#include "class_loader_utils.h"
#include "dex/utf.h"
#include "gc/heap.h"
#include "gc/space/space-inl.h"
#include "mirror/class.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "well_known_classes.h"

namespace art {

std::string DescribeSpace(ObjPtr<mirror::Class> klass) {
  std::ostringstream oss;
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::space::ContinuousSpace* cs =
      heap->FindContinuousSpaceFromObject(klass.Ptr(), /* fail_ok */ true);
  if (cs != nullptr) {
    if (cs->IsImageSpace()) {
      gc::space::ImageSpace* ispace = cs->AsImageSpace();
      oss << "image;" << ispace->GetName() << ";"
          // If the file name is the same as the name, output "+" instead to shorten the output.
          << (ispace->GetImageFilename() == cs->GetName() ? "+" : ispace->GetImageFilename())
          << ";" << static_cast<const void*>(ispace->Begin());
    } else {
      oss << "continuous;" << cs->GetName();
    }
  } else {
    gc::space::DiscontinuousSpace* ds =
        heap->FindDiscontinuousSpaceFromObject(klass, /* fail_ok */ true);
    if (ds != nullptr) {
      oss << "discontinuous;" << ds->GetName();
    } else {
      oss << "invalid";
    }
  }
  return oss.str();
}

std::string DescribeLoaders(ObjPtr<mirror::ClassLoader> loader, const char* class_descriptor) {
  std::ostringstream oss;
  uint32_t hash = ComputeModifiedUtf8Hash(class_descriptor);
  ObjPtr<mirror::Class> path_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_PathClassLoader);
  ObjPtr<mirror::Class> dex_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_DexClassLoader);
  ObjPtr<mirror::Class> delegate_last_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_DelegateLastClassLoader);

  // Print the class loader chain.
  bool found_class = false;
  const char* loader_separator = "";
  if (loader == nullptr) {
    oss << "BootClassLoader";  // This would be unexpected.
  }
  for (; loader != nullptr; loader = loader->GetParent()) {
    ClassTable* table = Runtime::Current()->GetClassLinker()->ClassTableForClassLoader(loader);
    oss << loader_separator << loader->GetClass()->PrettyDescriptor()
        << "/" << static_cast<const void*>(table);
    loader_separator = ";";
    // If we didn't find the class yet, try to find it in the current class loader.
    if (!found_class) {
      ObjPtr<mirror::Class> klass =
          (table != nullptr) ? table->Lookup(class_descriptor, hash) : nullptr;
      if (klass != nullptr) {
        found_class = true;
        oss << "[hit:" << DescribeSpace(klass) << "]";
      }
    }

    // For PathClassLoader, DexClassLoader or DelegateLastClassLoader
    // also dump the dex file locations.
    if (loader->GetClass() == path_class_loader ||
        loader->GetClass() == dex_class_loader ||
        loader->GetClass() == delegate_last_class_loader) {
      oss << "(";
      ScopedObjectAccessUnchecked soa(Thread::Current());
      StackHandleScope<1> hs(soa.Self());
      Handle<mirror::ClassLoader> handle(hs.NewHandle(loader));
      const char* path_separator = "";
      const DexFile* base_dex_file = nullptr;
      VisitClassLoaderDexFiles(
          soa,
          handle,
          [&](const DexFile* dex_file) {
              oss << path_separator;
              path_separator = ":";
              if (base_dex_file != nullptr &&
                  dex_file->GetLocation().length() > base_dex_file->GetLocation().length() &&
                  dex_file->GetLocation().compare(0u,
                                                  base_dex_file->GetLocation().length(),
                                                  base_dex_file->GetLocation()) == 0) {
                // Replace the base location with "+" to shorten the output.
                oss << "+" << dex_file->GetLocation().substr(base_dex_file->GetLocation().length());
              } else {
                oss << dex_file->GetLocation();
                base_dex_file = dex_file;
              }
              oss << "/" << static_cast<const void*>(dex_file);
              return true;  // Continue with the next DexFile.
          });
      oss << ")";
    }
  }

  return oss.str();
}

void DumpB77342775DebugData(ObjPtr<mirror::Class> target_class, ObjPtr<mirror::Class> src_class) {
  std::string target_descriptor_storage;
  const char* target_descriptor = target_class->GetDescriptor(&target_descriptor_storage);
  const char kCheckedPrefix[] = "Lorg/apache/http/";
  // Avoid spam for other packages. (That spam would break some ART run-tests for example.)
  if (strncmp(target_descriptor, kCheckedPrefix, sizeof(kCheckedPrefix) - 1) != 0) {
    return;
  }
  auto matcher = [target_descriptor, target_class](ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (klass->DescriptorEquals(target_descriptor)) {
      LOG(ERROR) << "    descriptor match in "
          << DescribeLoaders(klass->GetClassLoader(), target_descriptor)
          << " match? " << std::boolalpha << (klass == target_class);
    }
  };

  std::string source_descriptor_storage;
  const char* source_descriptor = src_class->GetDescriptor(&source_descriptor_storage);

  LOG(ERROR) << "Maybe bug 77342775, looking for " << target_descriptor
      << " " << target_class.Ptr() << "[" << DescribeSpace(target_class) << "]"
      << " defined in " << target_class->GetDexFile().GetLocation()
      << "/" << static_cast<const void*>(&target_class->GetDexFile())
      << "\n  with loader: " << DescribeLoaders(target_class->GetClassLoader(), target_descriptor);
  if (target_class->IsInterface()) {
    ObjPtr<mirror::IfTable> iftable = src_class->GetIfTable();
    CHECK(iftable != nullptr);
    size_t ifcount = iftable->Count();
    LOG(ERROR) << "  in interface table for " << source_descriptor
        << " " << src_class.Ptr() << "[" << DescribeSpace(src_class) << "]"
        << " defined in " << src_class->GetDexFile().GetLocation()
        << "/" << static_cast<const void*>(&src_class->GetDexFile())
        << " ifcount=" << ifcount
        << "\n  with loader " << DescribeLoaders(src_class->GetClassLoader(), source_descriptor);
    for (size_t i = 0; i != ifcount; ++i) {
      ObjPtr<mirror::Class> iface = iftable->GetInterface(i);
      CHECK(iface != nullptr);
      LOG(ERROR) << "  iface #" << i << ": " << iface->PrettyDescriptor();
      matcher(iface);
    }
  } else {
    LOG(ERROR) << "  in superclass chain for " << source_descriptor
        << " " << src_class.Ptr() << "[" << DescribeSpace(src_class) << "]"
        << " defined in " << src_class->GetDexFile().GetLocation()
        << "/" << static_cast<const void*>(&src_class->GetDexFile())
        << "\n  with loader " << DescribeLoaders(src_class->GetClassLoader(), source_descriptor);
    for (ObjPtr<mirror::Class> klass = src_class;
         klass != nullptr;
         klass = klass->GetSuperClass()) {
      LOG(ERROR) << "  - " << klass->PrettyDescriptor();
      matcher(klass);
    }
  }
}

}  // namespace art
