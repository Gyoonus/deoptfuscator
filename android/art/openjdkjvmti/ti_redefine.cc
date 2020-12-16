/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_redefine.h"

#include <limits>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "art_method-inl.h"
#include "base/array_ref.h"
#include "base/stringpiece.h"
#include "class_linker-inl.h"
#include "debugger.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_types.h"
#include "events-inl.h"
#include "gc/allocation_listener.h"
#include "gc/heap.h"
#include "instrumentation.h"
#include "intern_table.h"
#include "jdwp/jdwp.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_event.h"
#include "jdwp/object_registry.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni_env_ext-inl.h"
#include "jvmti_allocator.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/object.h"
#include "nativehelper/scoped_local_ref.h"
#include "non_debuggable_classes.h"
#include "object_lock.h"
#include "runtime.h"
#include "ti_breakpoint.h"
#include "ti_class_loader.h"
#include "transform.h"
#include "verifier/method_verifier.h"
#include "verifier/verifier_enums.h"

namespace openjdkjvmti {

using android::base::StringPrintf;

// A helper that fills in a classes obsolete_methods_ and obsolete_dex_caches_ classExt fields as
// they are created. This ensures that we can always call any method of an obsolete ArtMethod object
// almost as soon as they are created since the GetObsoleteDexCache method will succeed.
class ObsoleteMap {
 public:
  art::ArtMethod* FindObsoleteVersion(art::ArtMethod* original)
      REQUIRES(art::Locks::mutator_lock_, art::Roles::uninterruptible_) {
    auto method_pair = id_map_.find(original);
    if (method_pair != id_map_.end()) {
      art::ArtMethod* res = obsolete_methods_->GetElementPtrSize<art::ArtMethod*>(
          method_pair->second, art::kRuntimePointerSize);
      DCHECK(res != nullptr);
      DCHECK_EQ(original, res->GetNonObsoleteMethod());
      return res;
    } else {
      return nullptr;
    }
  }

  void RecordObsolete(art::ArtMethod* original, art::ArtMethod* obsolete)
      REQUIRES(art::Locks::mutator_lock_, art::Roles::uninterruptible_) {
    DCHECK(original != nullptr);
    DCHECK(obsolete != nullptr);
    int32_t slot = next_free_slot_++;
    DCHECK_LT(slot, obsolete_methods_->GetLength());
    DCHECK(nullptr ==
           obsolete_methods_->GetElementPtrSize<art::ArtMethod*>(slot, art::kRuntimePointerSize));
    DCHECK(nullptr == obsolete_dex_caches_->Get(slot));
    obsolete_methods_->SetElementPtrSize(slot, obsolete, art::kRuntimePointerSize);
    obsolete_dex_caches_->Set(slot, original_dex_cache_);
    id_map_.insert({original, slot});
  }

  ObsoleteMap(art::ObjPtr<art::mirror::PointerArray> obsolete_methods,
              art::ObjPtr<art::mirror::ObjectArray<art::mirror::DexCache>> obsolete_dex_caches,
              art::ObjPtr<art::mirror::DexCache> original_dex_cache)
      : next_free_slot_(0),
        obsolete_methods_(obsolete_methods),
        obsolete_dex_caches_(obsolete_dex_caches),
        original_dex_cache_(original_dex_cache) {
    // Figure out where the first unused slot in the obsolete_methods_ array is.
    while (obsolete_methods_->GetElementPtrSize<art::ArtMethod*>(
        next_free_slot_, art::kRuntimePointerSize) != nullptr) {
      DCHECK(obsolete_dex_caches_->Get(next_free_slot_) != nullptr);
      next_free_slot_++;
    }
    // Sanity check that the same slot in obsolete_dex_caches_ is free.
    DCHECK(obsolete_dex_caches_->Get(next_free_slot_) == nullptr);
  }

 private:
  int32_t next_free_slot_;
  std::unordered_map<art::ArtMethod*, int32_t> id_map_;
  // Pointers to the fields in mirror::ClassExt. These can be held as ObjPtr since this is only used
  // when we have an exclusive mutator_lock_ (i.e. all threads are suspended).
  art::ObjPtr<art::mirror::PointerArray> obsolete_methods_;
  art::ObjPtr<art::mirror::ObjectArray<art::mirror::DexCache>> obsolete_dex_caches_;
  art::ObjPtr<art::mirror::DexCache> original_dex_cache_;
};

// This visitor walks thread stacks and allocates and sets up the obsolete methods. It also does
// some basic sanity checks that the obsolete method is sane.
class ObsoleteMethodStackVisitor : public art::StackVisitor {
 protected:
  ObsoleteMethodStackVisitor(
      art::Thread* thread,
      art::LinearAlloc* allocator,
      const std::unordered_set<art::ArtMethod*>& obsoleted_methods,
      ObsoleteMap* obsolete_maps)
        : StackVisitor(thread,
                       /*context*/nullptr,
                       StackVisitor::StackWalkKind::kIncludeInlinedFrames),
          allocator_(allocator),
          obsoleted_methods_(obsoleted_methods),
          obsolete_maps_(obsolete_maps) { }

  ~ObsoleteMethodStackVisitor() OVERRIDE {}

 public:
  // Returns true if we successfully installed obsolete methods on this thread, filling
  // obsolete_maps_ with the translations if needed. Returns false and fills error_msg if we fail.
  // The stack is cleaned up when we fail.
  static void UpdateObsoleteFrames(
      art::Thread* thread,
      art::LinearAlloc* allocator,
      const std::unordered_set<art::ArtMethod*>& obsoleted_methods,
      ObsoleteMap* obsolete_maps)
        REQUIRES(art::Locks::mutator_lock_) {
    ObsoleteMethodStackVisitor visitor(thread,
                                       allocator,
                                       obsoleted_methods,
                                       obsolete_maps);
    visitor.WalkStack();
  }

  bool VisitFrame() OVERRIDE REQUIRES(art::Locks::mutator_lock_) {
    art::ScopedAssertNoThreadSuspension snts("Fixing up the stack for obsolete methods.");
    art::ArtMethod* old_method = GetMethod();
    if (obsoleted_methods_.find(old_method) != obsoleted_methods_.end()) {
      // We cannot ensure that the right dex file is used in inlined frames so we don't support
      // redefining them.
      DCHECK(!IsInInlinedFrame()) << "Inlined frames are not supported when using redefinition";
      art::ArtMethod* new_obsolete_method = obsolete_maps_->FindObsoleteVersion(old_method);
      if (new_obsolete_method == nullptr) {
        // Create a new Obsolete Method and put it in the list.
        art::Runtime* runtime = art::Runtime::Current();
        art::ClassLinker* cl = runtime->GetClassLinker();
        auto ptr_size = cl->GetImagePointerSize();
        const size_t method_size = art::ArtMethod::Size(ptr_size);
        auto* method_storage = allocator_->Alloc(art::Thread::Current(), method_size);
        CHECK(method_storage != nullptr) << "Unable to allocate storage for obsolete version of '"
                                         << old_method->PrettyMethod() << "'";
        new_obsolete_method = new (method_storage) art::ArtMethod();
        new_obsolete_method->CopyFrom(old_method, ptr_size);
        DCHECK_EQ(new_obsolete_method->GetDeclaringClass(), old_method->GetDeclaringClass());
        new_obsolete_method->SetIsObsolete();
        new_obsolete_method->SetDontCompile();
        cl->SetEntryPointsForObsoleteMethod(new_obsolete_method);
        obsolete_maps_->RecordObsolete(old_method, new_obsolete_method);
        // Update JIT Data structures to point to the new method.
        art::jit::Jit* jit = art::Runtime::Current()->GetJit();
        if (jit != nullptr) {
          // Notify the JIT we are making this obsolete method. It will update the jit's internal
          // structures to keep track of the new obsolete method.
          jit->GetCodeCache()->MoveObsoleteMethod(old_method, new_obsolete_method);
        }
      }
      DCHECK(new_obsolete_method != nullptr);
      SetMethod(new_obsolete_method);
    }
    return true;
  }

 private:
  // The linear allocator we should use to make new methods.
  art::LinearAlloc* allocator_;
  // The set of all methods which could be obsoleted.
  const std::unordered_set<art::ArtMethod*>& obsoleted_methods_;
  // A map from the original to the newly allocated obsolete method for frames on this thread. The
  // values in this map are added to the obsolete_methods_ (and obsolete_dex_caches_) fields of
  // the redefined classes ClassExt as it is filled.
  ObsoleteMap* obsolete_maps_;
};

jvmtiError Redefiner::IsModifiableClass(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        jclass klass,
                                        jboolean* is_redefinable) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);
  art::ObjPtr<art::mirror::Object> obj(self->DecodeJObject(klass));
  if (obj.IsNull()) {
    return ERR(INVALID_CLASS);
  }
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(obj->AsClass()));
  std::string err_unused;
  *is_redefinable =
      Redefiner::GetClassRedefinitionError(h_klass, &err_unused) != ERR(UNMODIFIABLE_CLASS)
      ? JNI_TRUE : JNI_FALSE;
  return OK;
}

jvmtiError Redefiner::GetClassRedefinitionError(jclass klass, /*out*/std::string* error_msg) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);
  art::ObjPtr<art::mirror::Object> obj(self->DecodeJObject(klass));
  if (obj.IsNull()) {
    return ERR(INVALID_CLASS);
  }
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(obj->AsClass()));
  return Redefiner::GetClassRedefinitionError(h_klass, error_msg);
}

jvmtiError Redefiner::GetClassRedefinitionError(art::Handle<art::mirror::Class> klass,
                                                /*out*/std::string* error_msg) {
  if (!klass->IsResolved()) {
    // It's only a problem to try to retransform/redefine a unprepared class if it's happening on
    // the same thread as the class-linking process. If it's on another thread we will be able to
    // wait for the preparation to finish and continue from there.
    if (klass->GetLockOwnerThreadId() == art::Thread::Current()->GetThreadId()) {
      *error_msg = "Modification of class " + klass->PrettyClass() +
          " from within the classes ClassLoad callback is not supported to prevent deadlocks." +
          " Please use ClassFileLoadHook directly instead.";
      return ERR(INTERNAL);
    } else {
      LOG(WARNING) << klass->PrettyClass() << " is not yet resolved. Attempting to transform "
                   << "it could cause arbitrary length waits as the class is being resolved.";
    }
  }
  if (klass->IsPrimitive()) {
    *error_msg = "Modification of primitive classes is not supported";
    return ERR(UNMODIFIABLE_CLASS);
  } else if (klass->IsInterface()) {
    *error_msg = "Modification of Interface classes is currently not supported";
    return ERR(UNMODIFIABLE_CLASS);
  } else if (klass->IsStringClass()) {
    *error_msg = "Modification of String class is not supported";
    return ERR(UNMODIFIABLE_CLASS);
  } else if (klass->IsArrayClass()) {
    *error_msg = "Modification of Array classes is not supported";
    return ERR(UNMODIFIABLE_CLASS);
  } else if (klass->IsProxyClass()) {
    *error_msg = "Modification of proxy classes is not supported";
    return ERR(UNMODIFIABLE_CLASS);
  }

  for (jclass c : art::NonDebuggableClasses::GetNonDebuggableClasses()) {
    if (klass.Get() == art::Thread::Current()->DecodeJObject(c)->AsClass()) {
      *error_msg = "Class might have stack frames that cannot be made obsolete";
      return ERR(UNMODIFIABLE_CLASS);
    }
  }

  return OK;
}

// Moves dex data to an anonymous, read-only mmap'd region.
std::unique_ptr<art::MemMap> Redefiner::MoveDataToMemMap(const std::string& original_location,
                                                         art::ArrayRef<const unsigned char> data,
                                                         std::string* error_msg) {
  std::unique_ptr<art::MemMap> map(art::MemMap::MapAnonymous(
      StringPrintf("%s-transformed", original_location.c_str()).c_str(),
      nullptr,
      data.size(),
      PROT_READ|PROT_WRITE,
      /*low_4gb*/false,
      /*reuse*/false,
      error_msg));
  if (map == nullptr) {
    return map;
  }
  memcpy(map->Begin(), data.data(), data.size());
  // Make the dex files mmap read only. This matches how other DexFiles are mmaped and prevents
  // programs from corrupting it.
  map->Protect(PROT_READ);
  return map;
}

Redefiner::ClassRedefinition::ClassRedefinition(
    Redefiner* driver,
    jclass klass,
    const art::DexFile* redefined_dex_file,
    const char* class_sig,
    art::ArrayRef<const unsigned char> orig_dex_file) :
      driver_(driver),
      klass_(klass),
      dex_file_(redefined_dex_file),
      class_sig_(class_sig),
      original_dex_file_(orig_dex_file) {
  GetMirrorClass()->MonitorEnter(driver_->self_);
}

Redefiner::ClassRedefinition::~ClassRedefinition() {
  if (driver_ != nullptr) {
    GetMirrorClass()->MonitorExit(driver_->self_);
  }
}

jvmtiError Redefiner::RedefineClasses(ArtJvmTiEnv* env,
                                      EventHandler* event_handler,
                                      art::Runtime* runtime,
                                      art::Thread* self,
                                      jint class_count,
                                      const jvmtiClassDefinition* definitions,
                                      /*out*/std::string* error_msg) {
  if (env == nullptr) {
    *error_msg = "env was null!";
    return ERR(INVALID_ENVIRONMENT);
  } else if (class_count < 0) {
    *error_msg = "class_count was less then 0";
    return ERR(ILLEGAL_ARGUMENT);
  } else if (class_count == 0) {
    // We don't actually need to do anything. Just return OK.
    return OK;
  } else if (definitions == nullptr) {
    *error_msg = "null definitions!";
    return ERR(NULL_POINTER);
  }
  std::vector<ArtClassDefinition> def_vector;
  def_vector.reserve(class_count);
  for (jint i = 0; i < class_count; i++) {
    jvmtiError res = Redefiner::GetClassRedefinitionError(definitions[i].klass, error_msg);
    if (res != OK) {
      return res;
    }
    // We make a copy of the class_bytes to pass into the retransformation.
    // This makes cleanup easier (since we unambiguously own the bytes) and also is useful since we
    // will need to keep the original bytes around unaltered for subsequent RetransformClasses calls
    // to get the passed in bytes.
    unsigned char* class_bytes_copy = nullptr;
    res = env->Allocate(definitions[i].class_byte_count, &class_bytes_copy);
    if (res != OK) {
      return res;
    }
    memcpy(class_bytes_copy, definitions[i].class_bytes, definitions[i].class_byte_count);

    ArtClassDefinition def;
    res = def.Init(self, definitions[i]);
    if (res != OK) {
      return res;
    }
    def_vector.push_back(std::move(def));
  }
  // Call all the transformation events.
  jvmtiError res = Transformer::RetransformClassesDirect(event_handler,
                                                         self,
                                                         &def_vector);
  if (res != OK) {
    // Something went wrong with transformation!
    return res;
  }
  return RedefineClassesDirect(env, runtime, self, def_vector, error_msg);
}

jvmtiError Redefiner::RedefineClassesDirect(ArtJvmTiEnv* env,
                                            art::Runtime* runtime,
                                            art::Thread* self,
                                            const std::vector<ArtClassDefinition>& definitions,
                                            std::string* error_msg) {
  DCHECK(env != nullptr);
  if (definitions.size() == 0) {
    // We don't actually need to do anything. Just return OK.
    return OK;
  }
  // Stop JIT for the duration of this redefine since the JIT might concurrently compile a method we
  // are going to redefine.
  art::jit::ScopedJitSuspend suspend_jit;
  // Get shared mutator lock so we can lock all the classes.
  art::ScopedObjectAccess soa(self);
  Redefiner r(env, runtime, self, error_msg);
  for (const ArtClassDefinition& def : definitions) {
    // Only try to transform classes that have been modified.
    if (def.IsModified()) {
      jvmtiError res = r.AddRedefinition(env, def);
      if (res != OK) {
        return res;
      }
    }
  }
  return r.Run();
}

jvmtiError Redefiner::AddRedefinition(ArtJvmTiEnv* env, const ArtClassDefinition& def) {
  std::string original_dex_location;
  jvmtiError ret = OK;
  if ((ret = GetClassLocation(env, def.GetClass(), &original_dex_location))) {
    *error_msg_ = "Unable to get original dex file location!";
    return ret;
  }
  char* generic_ptr_unused = nullptr;
  char* signature_ptr = nullptr;
  if ((ret = env->GetClassSignature(def.GetClass(), &signature_ptr, &generic_ptr_unused)) != OK) {
    *error_msg_ = "Unable to get class signature!";
    return ret;
  }
  JvmtiUniquePtr<char> generic_unique_ptr(MakeJvmtiUniquePtr(env, generic_ptr_unused));
  JvmtiUniquePtr<char> signature_unique_ptr(MakeJvmtiUniquePtr(env, signature_ptr));
  std::unique_ptr<art::MemMap> map(MoveDataToMemMap(original_dex_location,
                                                    def.GetDexData(),
                                                    error_msg_));
  std::ostringstream os;
  if (map.get() == nullptr) {
    os << "Failed to create anonymous mmap for modified dex file of class " << def.GetName()
       << "in dex file " << original_dex_location << " because: " << *error_msg_;
    *error_msg_ = os.str();
    return ERR(OUT_OF_MEMORY);
  }
  if (map->Size() < sizeof(art::DexFile::Header)) {
    *error_msg_ = "Could not read dex file header because dex_data was too short";
    return ERR(INVALID_CLASS_FORMAT);
  }
  uint32_t checksum = reinterpret_cast<const art::DexFile::Header*>(map->Begin())->checksum_;
  const art::ArtDexFileLoader dex_file_loader;
  std::unique_ptr<const art::DexFile> dex_file(dex_file_loader.Open(map->GetName(),
                                                                    checksum,
                                                                    std::move(map),
                                                                    /*verify*/true,
                                                                    /*verify_checksum*/true,
                                                                    error_msg_));
  if (dex_file.get() == nullptr) {
    os << "Unable to load modified dex file for " << def.GetName() << ": " << *error_msg_;
    *error_msg_ = os.str();
    return ERR(INVALID_CLASS_FORMAT);
  }
  redefinitions_.push_back(
      Redefiner::ClassRedefinition(this,
                                   def.GetClass(),
                                   dex_file.release(),
                                   signature_ptr,
                                   def.GetNewOriginalDexFile()));
  return OK;
}

art::mirror::Class* Redefiner::ClassRedefinition::GetMirrorClass() {
  return driver_->self_->DecodeJObject(klass_)->AsClass();
}

art::mirror::ClassLoader* Redefiner::ClassRedefinition::GetClassLoader() {
  return GetMirrorClass()->GetClassLoader();
}

art::mirror::DexCache* Redefiner::ClassRedefinition::CreateNewDexCache(
    art::Handle<art::mirror::ClassLoader> loader) {
  art::StackHandleScope<2> hs(driver_->self_);
  art::ClassLinker* cl = driver_->runtime_->GetClassLinker();
  art::Handle<art::mirror::DexCache> cache(hs.NewHandle(
      art::ObjPtr<art::mirror::DexCache>::DownCast(
          cl->GetClassRoot(art::ClassLinker::kJavaLangDexCache)->AllocObject(driver_->self_))));
  if (cache.IsNull()) {
    driver_->self_->AssertPendingOOMException();
    return nullptr;
  }
  art::Handle<art::mirror::String> location(hs.NewHandle(
      cl->GetInternTable()->InternStrong(dex_file_->GetLocation().c_str())));
  if (location.IsNull()) {
    driver_->self_->AssertPendingOOMException();
    return nullptr;
  }
  art::WriterMutexLock mu(driver_->self_, *art::Locks::dex_lock_);
  art::mirror::DexCache::InitializeDexCache(driver_->self_,
                                            cache.Get(),
                                            location.Get(),
                                            dex_file_.get(),
                                            loader.IsNull() ? driver_->runtime_->GetLinearAlloc()
                                                            : loader->GetAllocator(),
                                            art::kRuntimePointerSize);
  return cache.Get();
}

void Redefiner::RecordFailure(jvmtiError result,
                              const std::string& class_sig,
                              const std::string& error_msg) {
  *error_msg_ = StringPrintf("Unable to perform redefinition of '%s': %s",
                             class_sig.c_str(),
                             error_msg.c_str());
  result_ = result;
}

art::mirror::Object* Redefiner::ClassRedefinition::AllocateOrGetOriginalDexFile() {
  // If we have been specifically given a new set of bytes use that
  if (original_dex_file_.size() != 0) {
    return art::mirror::ByteArray::AllocateAndFill(
        driver_->self_,
        reinterpret_cast<const signed char*>(original_dex_file_.data()),
        original_dex_file_.size());
  }

  // See if we already have one set.
  art::ObjPtr<art::mirror::ClassExt> ext(GetMirrorClass()->GetExtData());
  if (!ext.IsNull()) {
    art::ObjPtr<art::mirror::Object> old_original_dex_file(ext->GetOriginalDexFile());
    if (!old_original_dex_file.IsNull()) {
      // We do. Use it.
      return old_original_dex_file.Ptr();
    }
  }

  // return the current dex_cache which has the dex file in it.
  art::ObjPtr<art::mirror::DexCache> current_dex_cache(GetMirrorClass()->GetDexCache());
  // TODO Handle this or make it so it cannot happen.
  if (current_dex_cache->GetDexFile()->NumClassDefs() != 1) {
    LOG(WARNING) << "Current dex file has more than one class in it. Calling RetransformClasses "
                 << "on this class might fail if no transformations are applied to it!";
  }
  return current_dex_cache.Ptr();
}

struct CallbackCtx {
  ObsoleteMap* obsolete_map;
  art::LinearAlloc* allocator;
  std::unordered_set<art::ArtMethod*> obsolete_methods;

  explicit CallbackCtx(ObsoleteMap* map, art::LinearAlloc* alloc)
      : obsolete_map(map), allocator(alloc) {}
};

void DoAllocateObsoleteMethodsCallback(art::Thread* t, void* vdata) NO_THREAD_SAFETY_ANALYSIS {
  CallbackCtx* data = reinterpret_cast<CallbackCtx*>(vdata);
  ObsoleteMethodStackVisitor::UpdateObsoleteFrames(t,
                                                   data->allocator,
                                                   data->obsolete_methods,
                                                   data->obsolete_map);
}

// This creates any ArtMethod* structures needed for obsolete methods and ensures that the stack is
// updated so they will be run.
// TODO Rewrite so we can do this only once regardless of how many redefinitions there are.
void Redefiner::ClassRedefinition::FindAndAllocateObsoleteMethods(art::mirror::Class* art_klass) {
  art::ScopedAssertNoThreadSuspension ns("No thread suspension during thread stack walking");
  art::mirror::ClassExt* ext = art_klass->GetExtData();
  CHECK(ext->GetObsoleteMethods() != nullptr);
  art::ClassLinker* linker = driver_->runtime_->GetClassLinker();
  // This holds pointers to the obsolete methods map fields which are updated as needed.
  ObsoleteMap map(ext->GetObsoleteMethods(), ext->GetObsoleteDexCaches(), art_klass->GetDexCache());
  CallbackCtx ctx(&map, linker->GetAllocatorForClassLoader(art_klass->GetClassLoader()));
  // Add all the declared methods to the map
  for (auto& m : art_klass->GetDeclaredMethods(art::kRuntimePointerSize)) {
    if (m.IsIntrinsic()) {
      LOG(WARNING) << "Redefining intrinsic method " << m.PrettyMethod() << ". This may cause the "
                   << "unexpected use of the original definition of " << m.PrettyMethod() << "in "
                   << "methods that have already been compiled.";
    }
    // It is possible to simply filter out some methods where they cannot really become obsolete,
    // such as native methods and keep their original (possibly optimized) implementations. We don't
    // do this, however, since we would need to mark these functions (still in the classes
    // declared_methods array) as obsolete so we will find the correct dex file to get meta-data
    // from (for example about stack-frame size). Furthermore we would be unable to get some useful
    // error checking from the interpreter which ensure we don't try to start executing obsolete
    // methods.
    ctx.obsolete_methods.insert(&m);
  }
  {
    art::MutexLock mu(driver_->self_, *art::Locks::thread_list_lock_);
    art::ThreadList* list = art::Runtime::Current()->GetThreadList();
    list->ForEach(DoAllocateObsoleteMethodsCallback, static_cast<void*>(&ctx));
  }
}

// Try and get the declared method. First try to get a virtual method then a direct method if that's
// not found.
static art::ArtMethod* FindMethod(art::Handle<art::mirror::Class> klass,
                                  art::StringPiece name,
                                  art::Signature sig) REQUIRES_SHARED(art::Locks::mutator_lock_) {
  DCHECK(!klass->IsProxyClass());
  for (art::ArtMethod& m : klass->GetDeclaredMethodsSlice(art::kRuntimePointerSize)) {
    if (m.GetName() == name && m.GetSignature() == sig) {
      return &m;
    }
  }
  return nullptr;
}

bool Redefiner::ClassRedefinition::CheckSameMethods() {
  art::StackHandleScope<1> hs(driver_->self_);
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(GetMirrorClass()));
  DCHECK_EQ(dex_file_->NumClassDefs(), 1u);

  art::ClassDataItemIterator new_iter(*dex_file_,
                                      dex_file_->GetClassData(dex_file_->GetClassDef(0)));

  // Make sure we have the same number of methods.
  uint32_t num_new_method = new_iter.NumVirtualMethods() + new_iter.NumDirectMethods();
  uint32_t num_old_method = h_klass->GetDeclaredMethodsSlice(art::kRuntimePointerSize).size();
  if (num_new_method != num_old_method) {
    bool bigger = num_new_method > num_old_method;
    RecordFailure(bigger ? ERR(UNSUPPORTED_REDEFINITION_METHOD_ADDED)
                         : ERR(UNSUPPORTED_REDEFINITION_METHOD_DELETED),
                  StringPrintf("Total number of declared methods changed from %d to %d",
                               num_old_method, num_new_method));
    return false;
  }

  // Skip all of the fields. We should have already checked this.
  new_iter.SkipAllFields();
  // Check each of the methods. NB we don't need to specifically check for removals since the 2 dex
  // files have the same number of methods, which means there must be an equal amount of additions
  // and removals.
  for (; new_iter.HasNextMethod(); new_iter.Next()) {
    // Get the data on the method we are searching for
    const art::DexFile::MethodId& new_method_id = dex_file_->GetMethodId(new_iter.GetMemberIndex());
    const char* new_method_name = dex_file_->GetMethodName(new_method_id);
    art::Signature new_method_signature = dex_file_->GetMethodSignature(new_method_id);
    art::ArtMethod* old_method = FindMethod(h_klass, new_method_name, new_method_signature);
    // If we got past the check for the same number of methods above that means there must be at
    // least one added and one removed method. We will return the ADDED failure message since it is
    // easier to get a useful error report for it.
    if (old_method == nullptr) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_METHOD_ADDED),
                    StringPrintf("Unknown method '%s' (sig: %s) was added!",
                                  new_method_name,
                                  new_method_signature.ToString().c_str()));
      return false;
    }
    // Since direct methods have different flags than virtual ones (specifically direct methods must
    // have kAccPrivate or kAccStatic or kAccConstructor flags) we can tell if a method changes from
    // virtual to direct.
    uint32_t new_flags = new_iter.GetMethodAccessFlags();
    if (new_flags != (old_method->GetAccessFlags() & art::kAccValidMethodFlags)) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED),
                    StringPrintf("method '%s' (sig: %s) had different access flags",
                                 new_method_name,
                                 new_method_signature.ToString().c_str()));
      return false;
    }
  }
  return true;
}

bool Redefiner::ClassRedefinition::CheckSameFields() {
  art::StackHandleScope<1> hs(driver_->self_);
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(GetMirrorClass()));
  DCHECK_EQ(dex_file_->NumClassDefs(), 1u);
  art::ClassDataItemIterator new_iter(*dex_file_,
                                      dex_file_->GetClassData(dex_file_->GetClassDef(0)));
  const art::DexFile& old_dex_file = h_klass->GetDexFile();
  art::ClassDataItemIterator old_iter(old_dex_file,
                                      old_dex_file.GetClassData(*h_klass->GetClassDef()));
  // Instance and static fields can be differentiated by their flags so no need to check them
  // separately.
  while (new_iter.HasNextInstanceField() || new_iter.HasNextStaticField()) {
    // Get the data on the method we are searching for
    const art::DexFile::FieldId& new_field_id = dex_file_->GetFieldId(new_iter.GetMemberIndex());
    const char* new_field_name = dex_file_->GetFieldName(new_field_id);
    const char* new_field_type = dex_file_->GetFieldTypeDescriptor(new_field_id);

    if (!(old_iter.HasNextInstanceField() || old_iter.HasNextStaticField())) {
      // We are missing the old version of this method!
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED),
                    StringPrintf("Unknown field '%s' (type: %s) added!",
                                  new_field_name,
                                  new_field_type));
      return false;
    }

    const art::DexFile::FieldId& old_field_id = old_dex_file.GetFieldId(old_iter.GetMemberIndex());
    const char* old_field_name = old_dex_file.GetFieldName(old_field_id);
    const char* old_field_type = old_dex_file.GetFieldTypeDescriptor(old_field_id);

    // Check name and type.
    if (strcmp(old_field_name, new_field_name) != 0 ||
        strcmp(old_field_type, new_field_type) != 0) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED),
                    StringPrintf("Field changed from '%s' (sig: %s) to '%s' (sig: %s)!",
                                  old_field_name,
                                  old_field_type,
                                  new_field_name,
                                  new_field_type));
      return false;
    }

    // Since static fields have different flags than instance ones (specifically static fields must
    // have the kAccStatic flag) we can tell if a field changes from static to instance.
    if (new_iter.GetFieldAccessFlags() != old_iter.GetFieldAccessFlags()) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED),
                    StringPrintf("Field '%s' (sig: %s) had different access flags",
                                  new_field_name,
                                  new_field_type));
      return false;
    }

    new_iter.Next();
    old_iter.Next();
  }
  if (old_iter.HasNextInstanceField() || old_iter.HasNextStaticField()) {
    RecordFailure(ERR(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED),
                  StringPrintf("field '%s' (sig: %s) is missing!",
                                old_dex_file.GetFieldName(old_dex_file.GetFieldId(
                                    old_iter.GetMemberIndex())),
                                old_dex_file.GetFieldTypeDescriptor(old_dex_file.GetFieldId(
                                    old_iter.GetMemberIndex()))));
    return false;
  }
  return true;
}

bool Redefiner::ClassRedefinition::CheckClass() {
  art::StackHandleScope<1> hs(driver_->self_);
  // Easy check that only 1 class def is present.
  if (dex_file_->NumClassDefs() != 1) {
    RecordFailure(ERR(ILLEGAL_ARGUMENT),
                  StringPrintf("Expected 1 class def in dex file but found %d",
                               dex_file_->NumClassDefs()));
    return false;
  }
  // Get the ClassDef from the new DexFile.
  // Since the dex file has only a single class def the index is always 0.
  const art::DexFile::ClassDef& def = dex_file_->GetClassDef(0);
  // Get the class as it is now.
  art::Handle<art::mirror::Class> current_class(hs.NewHandle(GetMirrorClass()));

  // Check the access flags didn't change.
  if (def.GetJavaAccessFlags() != (current_class->GetAccessFlags() & art::kAccValidClassFlags)) {
    RecordFailure(ERR(UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED),
                  "Cannot change modifiers of class by redefinition");
    return false;
  }

  // Check class name.
  // These should have been checked by the dexfile verifier on load.
  DCHECK_NE(def.class_idx_, art::dex::TypeIndex::Invalid()) << "Invalid type index";
  const char* descriptor = dex_file_->StringByTypeIdx(def.class_idx_);
  DCHECK(descriptor != nullptr) << "Invalid dex file structure!";
  if (!current_class->DescriptorEquals(descriptor)) {
    std::string storage;
    RecordFailure(ERR(NAMES_DONT_MATCH),
                  StringPrintf("expected file to contain class called '%s' but found '%s'!",
                               current_class->GetDescriptor(&storage),
                               descriptor));
    return false;
  }
  if (current_class->IsObjectClass()) {
    if (def.superclass_idx_ != art::dex::TypeIndex::Invalid()) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED), "Superclass added!");
      return false;
    }
  } else {
    const char* super_descriptor = dex_file_->StringByTypeIdx(def.superclass_idx_);
    DCHECK(descriptor != nullptr) << "Invalid dex file structure!";
    if (!current_class->GetSuperClass()->DescriptorEquals(super_descriptor)) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED), "Superclass changed");
      return false;
    }
  }
  const art::DexFile::TypeList* interfaces = dex_file_->GetInterfacesList(def);
  if (interfaces == nullptr) {
    if (current_class->NumDirectInterfaces() != 0) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED), "Interfaces added");
      return false;
    }
  } else {
    DCHECK(!current_class->IsProxyClass());
    const art::DexFile::TypeList* current_interfaces = current_class->GetInterfaceTypeList();
    if (current_interfaces == nullptr || current_interfaces->Size() != interfaces->Size()) {
      RecordFailure(ERR(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED), "Interfaces added or removed");
      return false;
    }
    // The order of interfaces is (barely) meaningful so we error if it changes.
    const art::DexFile& orig_dex_file = current_class->GetDexFile();
    for (uint32_t i = 0; i < interfaces->Size(); i++) {
      if (strcmp(
            dex_file_->StringByTypeIdx(interfaces->GetTypeItem(i).type_idx_),
            orig_dex_file.StringByTypeIdx(current_interfaces->GetTypeItem(i).type_idx_)) != 0) {
        RecordFailure(ERR(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED),
                      "Interfaces changed or re-ordered");
        return false;
      }
    }
  }
  return true;
}

bool Redefiner::ClassRedefinition::CheckRedefinable() {
  std::string err;
  art::StackHandleScope<1> hs(driver_->self_);

  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(GetMirrorClass()));
  jvmtiError res = Redefiner::GetClassRedefinitionError(h_klass, &err);
  if (res != OK) {
    RecordFailure(res, err);
    return false;
  } else {
    return true;
  }
}

bool Redefiner::ClassRedefinition::CheckRedefinitionIsValid() {
  return CheckRedefinable() &&
      CheckClass() &&
      CheckSameFields() &&
      CheckSameMethods();
}

class RedefinitionDataIter;

// A wrapper that lets us hold onto the arbitrary sized data needed for redefinitions in a
// reasonably sane way. This adds no fields to the normal ObjectArray. By doing this we can avoid
// having to deal with the fact that we need to hold an arbitrary number of references live.
class RedefinitionDataHolder {
 public:
  enum DataSlot : int32_t {
    kSlotSourceClassLoader = 0,
    kSlotJavaDexFile = 1,
    kSlotNewDexFileCookie = 2,
    kSlotNewDexCache = 3,
    kSlotMirrorClass = 4,
    kSlotOrigDexFile = 5,
    kSlotOldObsoleteMethods = 6,
    kSlotOldDexCaches = 7,

    // Must be last one.
    kNumSlots = 8,
  };

  // This needs to have a HandleScope passed in that is capable of creating a new Handle without
  // overflowing. Only one handle will be created. This object has a lifetime identical to that of
  // the passed in handle-scope.
  RedefinitionDataHolder(art::StackHandleScope<1>* hs,
                         art::Runtime* runtime,
                         art::Thread* self,
                         std::vector<Redefiner::ClassRedefinition>* redefinitions)
      REQUIRES_SHARED(art::Locks::mutator_lock_) :
    arr_(
      hs->NewHandle(
        art::mirror::ObjectArray<art::mirror::Object>::Alloc(
            self,
            runtime->GetClassLinker()->GetClassRoot(art::ClassLinker::kObjectArrayClass),
            redefinitions->size() * kNumSlots))),
    redefinitions_(redefinitions) {}

  bool IsNull() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return arr_.IsNull();
  }

  art::mirror::ClassLoader* GetSourceClassLoader(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::ClassLoader*>(GetSlot(klass_index, kSlotSourceClassLoader));
  }
  art::mirror::Object* GetJavaDexFile(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return GetSlot(klass_index, kSlotJavaDexFile);
  }
  art::mirror::LongArray* GetNewDexFileCookie(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::LongArray*>(GetSlot(klass_index, kSlotNewDexFileCookie));
  }
  art::mirror::DexCache* GetNewDexCache(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::DexCache*>(GetSlot(klass_index, kSlotNewDexCache));
  }
  art::mirror::Class* GetMirrorClass(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::Class*>(GetSlot(klass_index, kSlotMirrorClass));
  }

  art::mirror::Object* GetOriginalDexFile(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::Object*>(GetSlot(klass_index, kSlotOrigDexFile));
  }

  art::mirror::PointerArray* GetOldObsoleteMethods(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::PointerArray*>(
        GetSlot(klass_index, kSlotOldObsoleteMethods));
  }

  art::mirror::ObjectArray<art::mirror::DexCache>* GetOldDexCaches(jint klass_index) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return art::down_cast<art::mirror::ObjectArray<art::mirror::DexCache>*>(
        GetSlot(klass_index, kSlotOldDexCaches));
  }

  void SetSourceClassLoader(jint klass_index, art::mirror::ClassLoader* loader)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotSourceClassLoader, loader);
  }
  void SetJavaDexFile(jint klass_index, art::mirror::Object* dexfile)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotJavaDexFile, dexfile);
  }
  void SetNewDexFileCookie(jint klass_index, art::mirror::LongArray* cookie)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotNewDexFileCookie, cookie);
  }
  void SetNewDexCache(jint klass_index, art::mirror::DexCache* cache)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotNewDexCache, cache);
  }
  void SetMirrorClass(jint klass_index, art::mirror::Class* klass)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotMirrorClass, klass);
  }
  void SetOriginalDexFile(jint klass_index, art::mirror::Object* bytes)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotOrigDexFile, bytes);
  }
  void SetOldObsoleteMethods(jint klass_index, art::mirror::PointerArray* methods)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotOldObsoleteMethods, methods);
  }
  void SetOldDexCaches(jint klass_index, art::mirror::ObjectArray<art::mirror::DexCache>* caches)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    SetSlot(klass_index, kSlotOldDexCaches, caches);
  }

  int32_t Length() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return arr_->GetLength() / kNumSlots;
  }

  std::vector<Redefiner::ClassRedefinition>* GetRedefinitions()
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return redefinitions_;
  }

  bool operator==(const RedefinitionDataHolder& other) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return arr_.Get() == other.arr_.Get();
  }

  bool operator!=(const RedefinitionDataHolder& other) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return !(*this == other);
  }

  RedefinitionDataIter begin() REQUIRES_SHARED(art::Locks::mutator_lock_);
  RedefinitionDataIter end() REQUIRES_SHARED(art::Locks::mutator_lock_);

 private:
  mutable art::Handle<art::mirror::ObjectArray<art::mirror::Object>> arr_;
  std::vector<Redefiner::ClassRedefinition>* redefinitions_;

  art::mirror::Object* GetSlot(jint klass_index,
                               DataSlot slot) const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DCHECK_LT(klass_index, Length());
    return arr_->Get((kNumSlots * klass_index) + slot);
  }

  void SetSlot(jint klass_index,
               DataSlot slot,
               art::ObjPtr<art::mirror::Object> obj) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DCHECK(!art::Runtime::Current()->IsActiveTransaction());
    DCHECK_LT(klass_index, Length());
    arr_->Set<false>((kNumSlots * klass_index) + slot, obj);
  }

  DISALLOW_COPY_AND_ASSIGN(RedefinitionDataHolder);
};

class RedefinitionDataIter {
 public:
  RedefinitionDataIter(int32_t idx, RedefinitionDataHolder& holder) : idx_(idx), holder_(holder) {}

  RedefinitionDataIter(const RedefinitionDataIter&) = default;
  RedefinitionDataIter(RedefinitionDataIter&&) = default;
  RedefinitionDataIter& operator=(const RedefinitionDataIter&) = default;
  RedefinitionDataIter& operator=(RedefinitionDataIter&&) = default;

  bool operator==(const RedefinitionDataIter& other) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return idx_ == other.idx_ && holder_ == other.holder_;
  }

  bool operator!=(const RedefinitionDataIter& other) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return !(*this == other);
  }

  RedefinitionDataIter operator++() {  // Value after modification.
    idx_++;
    return *this;
  }

  RedefinitionDataIter operator++(int) {
    RedefinitionDataIter temp = *this;
    idx_++;
    return temp;
  }

  RedefinitionDataIter operator+(ssize_t delta) const {
    RedefinitionDataIter temp = *this;
    temp += delta;
    return temp;
  }

  RedefinitionDataIter& operator+=(ssize_t delta) {
    idx_ += delta;
    return *this;
  }

  Redefiner::ClassRedefinition& GetRedefinition() REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return (*holder_.GetRedefinitions())[idx_];
  }

  RedefinitionDataHolder& GetHolder() {
    return holder_;
  }

  art::mirror::ClassLoader* GetSourceClassLoader() const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetSourceClassLoader(idx_);
  }
  art::mirror::Object* GetJavaDexFile() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetJavaDexFile(idx_);
  }
  art::mirror::LongArray* GetNewDexFileCookie() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetNewDexFileCookie(idx_);
  }
  art::mirror::DexCache* GetNewDexCache() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetNewDexCache(idx_);
  }
  art::mirror::Class* GetMirrorClass() const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetMirrorClass(idx_);
  }
  art::mirror::Object* GetOriginalDexFile() const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetOriginalDexFile(idx_);
  }
  art::mirror::PointerArray* GetOldObsoleteMethods() const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetOldObsoleteMethods(idx_);
  }
  art::mirror::ObjectArray<art::mirror::DexCache>* GetOldDexCaches() const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return holder_.GetOldDexCaches(idx_);
  }

  int32_t GetIndex() const {
    return idx_;
  }

  void SetSourceClassLoader(art::mirror::ClassLoader* loader)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetSourceClassLoader(idx_, loader);
  }
  void SetJavaDexFile(art::mirror::Object* dexfile) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetJavaDexFile(idx_, dexfile);
  }
  void SetNewDexFileCookie(art::mirror::LongArray* cookie)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetNewDexFileCookie(idx_, cookie);
  }
  void SetNewDexCache(art::mirror::DexCache* cache) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetNewDexCache(idx_, cache);
  }
  void SetMirrorClass(art::mirror::Class* klass) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetMirrorClass(idx_, klass);
  }
  void SetOriginalDexFile(art::mirror::Object* bytes)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetOriginalDexFile(idx_, bytes);
  }
  void SetOldObsoleteMethods(art::mirror::PointerArray* methods)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetOldObsoleteMethods(idx_, methods);
  }
  void SetOldDexCaches(art::mirror::ObjectArray<art::mirror::DexCache>* caches)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    holder_.SetOldDexCaches(idx_, caches);
  }

 private:
  int32_t idx_;
  RedefinitionDataHolder& holder_;
};

RedefinitionDataIter RedefinitionDataHolder::begin() {
  return RedefinitionDataIter(0, *this);
}

RedefinitionDataIter RedefinitionDataHolder::end() {
  return RedefinitionDataIter(Length(), *this);
}

bool Redefiner::ClassRedefinition::CheckVerification(const RedefinitionDataIter& iter) {
  DCHECK_EQ(dex_file_->NumClassDefs(), 1u);
  art::StackHandleScope<2> hs(driver_->self_);
  std::string error;
  // TODO Make verification log level lower
  art::verifier::FailureKind failure =
      art::verifier::MethodVerifier::VerifyClass(driver_->self_,
                                                 dex_file_.get(),
                                                 hs.NewHandle(iter.GetNewDexCache()),
                                                 hs.NewHandle(GetClassLoader()),
                                                 dex_file_->GetClassDef(0), /*class_def*/
                                                 nullptr, /*compiler_callbacks*/
                                                 true, /*allow_soft_failures*/
                                                 /*log_level*/
                                                 art::verifier::HardFailLogMode::kLogWarning,
                                                 &error);
  switch (failure) {
    case art::verifier::FailureKind::kNoFailure:
    case art::verifier::FailureKind::kSoftFailure:
      return true;
    case art::verifier::FailureKind::kHardFailure: {
      RecordFailure(ERR(FAILS_VERIFICATION), "Failed to verify class. Error was: " + error);
      return false;
    }
  }
}

// Looks through the previously allocated cookies to see if we need to update them with another new
// dexfile. This is so that even if multiple classes with the same classloader are redefined at
// once they are all added to the classloader.
bool Redefiner::ClassRedefinition::AllocateAndRememberNewDexFileCookie(
    art::Handle<art::mirror::ClassLoader> source_class_loader,
    art::Handle<art::mirror::Object> dex_file_obj,
    /*out*/RedefinitionDataIter* cur_data) {
  art::StackHandleScope<2> hs(driver_->self_);
  art::MutableHandle<art::mirror::LongArray> old_cookie(
      hs.NewHandle<art::mirror::LongArray>(nullptr));
  bool has_older_cookie = false;
  // See if we already have a cookie that a previous redefinition got from the same classloader.
  for (auto old_data = cur_data->GetHolder().begin(); old_data != *cur_data; ++old_data) {
    if (old_data.GetSourceClassLoader() == source_class_loader.Get()) {
      // Since every instance of this classloader should have the same cookie associated with it we
      // can stop looking here.
      has_older_cookie = true;
      old_cookie.Assign(old_data.GetNewDexFileCookie());
      break;
    }
  }
  if (old_cookie.IsNull()) {
    // No older cookie. Get it directly from the dex_file_obj
    // We should not have seen this classloader elsewhere.
    CHECK(!has_older_cookie);
    old_cookie.Assign(ClassLoaderHelper::GetDexFileCookie(dex_file_obj));
  }
  // Use the old cookie to generate the new one with the new DexFile* added in.
  art::Handle<art::mirror::LongArray>
      new_cookie(hs.NewHandle(ClassLoaderHelper::AllocateNewDexFileCookie(driver_->self_,
                                                                          old_cookie,
                                                                          dex_file_.get())));
  // Make sure the allocation worked.
  if (new_cookie.IsNull()) {
    return false;
  }

  // Save the cookie.
  cur_data->SetNewDexFileCookie(new_cookie.Get());
  // If there are other copies of this same classloader we need to make sure that we all have the
  // same cookie.
  if (has_older_cookie) {
    for (auto old_data = cur_data->GetHolder().begin(); old_data != *cur_data; ++old_data) {
      // We will let the GC take care of the cookie we allocated for this one.
      if (old_data.GetSourceClassLoader() == source_class_loader.Get()) {
        old_data.SetNewDexFileCookie(new_cookie.Get());
      }
    }
  }

  return true;
}

bool Redefiner::ClassRedefinition::FinishRemainingAllocations(
    /*out*/RedefinitionDataIter* cur_data) {
  art::ScopedObjectAccessUnchecked soa(driver_->self_);
  art::StackHandleScope<2> hs(driver_->self_);
  cur_data->SetMirrorClass(GetMirrorClass());
  // This shouldn't allocate
  art::Handle<art::mirror::ClassLoader> loader(hs.NewHandle(GetClassLoader()));
  // The bootclasspath is handled specially so it doesn't have a j.l.DexFile.
  if (!art::ClassLinker::IsBootClassLoader(soa, loader.Get())) {
    cur_data->SetSourceClassLoader(loader.Get());
    art::Handle<art::mirror::Object> dex_file_obj(hs.NewHandle(
        ClassLoaderHelper::FindSourceDexFileObject(driver_->self_, loader)));
    cur_data->SetJavaDexFile(dex_file_obj.Get());
    if (dex_file_obj == nullptr) {
      RecordFailure(ERR(INTERNAL), "Unable to find dex file!");
      return false;
    }
    // Allocate the new dex file cookie.
    if (!AllocateAndRememberNewDexFileCookie(loader, dex_file_obj, cur_data)) {
      driver_->self_->AssertPendingOOMException();
      driver_->self_->ClearException();
      RecordFailure(ERR(OUT_OF_MEMORY), "Unable to allocate dex file array for class loader");
      return false;
    }
  }
  cur_data->SetNewDexCache(CreateNewDexCache(loader));
  if (cur_data->GetNewDexCache() == nullptr) {
    driver_->self_->AssertPendingException();
    driver_->self_->ClearException();
    RecordFailure(ERR(OUT_OF_MEMORY), "Unable to allocate DexCache");
    return false;
  }

  // We won't always need to set this field.
  cur_data->SetOriginalDexFile(AllocateOrGetOriginalDexFile());
  if (cur_data->GetOriginalDexFile() == nullptr) {
    driver_->self_->AssertPendingOOMException();
    driver_->self_->ClearException();
    RecordFailure(ERR(OUT_OF_MEMORY), "Unable to allocate array for original dex file");
    return false;
  }
  return true;
}

void Redefiner::ClassRedefinition::UnregisterJvmtiBreakpoints() {
  BreakpointUtil::RemoveBreakpointsInClass(driver_->env_, GetMirrorClass());
}

void Redefiner::ClassRedefinition::UnregisterBreakpoints() {
  if (LIKELY(!art::Dbg::IsDebuggerActive())) {
    return;
  }
  art::JDWP::JdwpState* state = art::Dbg::GetJdwpState();
  if (state != nullptr) {
    state->UnregisterLocationEventsOnClass(GetMirrorClass());
  }
}

void Redefiner::UnregisterAllBreakpoints() {
  for (Redefiner::ClassRedefinition& redef : redefinitions_) {
    redef.UnregisterBreakpoints();
    redef.UnregisterJvmtiBreakpoints();
  }
}

bool Redefiner::CheckAllRedefinitionAreValid() {
  for (Redefiner::ClassRedefinition& redef : redefinitions_) {
    if (!redef.CheckRedefinitionIsValid()) {
      return false;
    }
  }
  return true;
}

void Redefiner::RestoreObsoleteMethodMapsIfUnneeded(RedefinitionDataHolder& holder) {
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    data.GetRedefinition().RestoreObsoleteMethodMapsIfUnneeded(&data);
  }
}

bool Redefiner::EnsureAllClassAllocationsFinished(RedefinitionDataHolder& holder) {
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    if (!data.GetRedefinition().EnsureClassAllocationsFinished(&data)) {
      return false;
    }
  }
  return true;
}

bool Redefiner::FinishAllRemainingAllocations(RedefinitionDataHolder& holder) {
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    // Allocate the data this redefinition requires.
    if (!data.GetRedefinition().FinishRemainingAllocations(&data)) {
      return false;
    }
  }
  return true;
}

void Redefiner::ClassRedefinition::ReleaseDexFile() {
  dex_file_.release();
}

void Redefiner::ReleaseAllDexFiles() {
  for (Redefiner::ClassRedefinition& redef : redefinitions_) {
    redef.ReleaseDexFile();
  }
}

bool Redefiner::CheckAllClassesAreVerified(RedefinitionDataHolder& holder) {
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    if (!data.GetRedefinition().CheckVerification(data)) {
      return false;
    }
  }
  return true;
}

class ScopedDisableConcurrentAndMovingGc {
 public:
  ScopedDisableConcurrentAndMovingGc(art::gc::Heap* heap, art::Thread* self)
      : heap_(heap), self_(self) {
    if (heap_->IsGcConcurrentAndMoving()) {
      heap_->IncrementDisableMovingGC(self_);
    }
  }

  ~ScopedDisableConcurrentAndMovingGc() {
    if (heap_->IsGcConcurrentAndMoving()) {
      heap_->DecrementDisableMovingGC(self_);
    }
  }
 private:
  art::gc::Heap* heap_;
  art::Thread* self_;
};

jvmtiError Redefiner::Run() {
  art::StackHandleScope<1> hs(self_);
  // Allocate an array to hold onto all java temporary objects associated with this redefinition.
  // We will let this be collected after the end of this function.
  RedefinitionDataHolder holder(&hs, runtime_, self_, &redefinitions_);
  if (holder.IsNull()) {
    self_->AssertPendingOOMException();
    self_->ClearException();
    RecordFailure(ERR(OUT_OF_MEMORY), "Could not allocate storage for temporaries");
    return result_;
  }

  // First we just allocate the ClassExt and its fields that we need. These can be updated
  // atomically without any issues (since we allocate the map arrays as empty) so we don't bother
  // doing a try loop. The other allocations we need to ensure that nothing has changed in the time
  // between allocating them and pausing all threads before we can update them so we need to do a
  // try loop.
  if (!CheckAllRedefinitionAreValid() ||
      !EnsureAllClassAllocationsFinished(holder) ||
      !FinishAllRemainingAllocations(holder) ||
      !CheckAllClassesAreVerified(holder)) {
    return result_;
  }

  // At this point we can no longer fail without corrupting the runtime state.
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    art::ClassLinker* cl = runtime_->GetClassLinker();
    cl->RegisterExistingDexCache(data.GetNewDexCache(), data.GetSourceClassLoader());
    if (data.GetSourceClassLoader() == nullptr) {
      cl->AppendToBootClassPath(self_, data.GetRedefinition().GetDexFile());
    }
  }
  UnregisterAllBreakpoints();

  // Disable GC and wait for it to be done if we are a moving GC.  This is fine since we are done
  // allocating so no deadlocks.
  ScopedDisableConcurrentAndMovingGc sdcamgc(runtime_->GetHeap(), self_);

  // Do transition to final suspension
  // TODO We might want to give this its own suspended state!
  // TODO This isn't right. We need to change state without any chance of suspend ideally!
  art::ScopedThreadSuspension sts(self_, art::ThreadState::kNative);
  art::ScopedSuspendAll ssa("Final installation of redefined Classes!", /*long_suspend*/true);
  for (RedefinitionDataIter data = holder.begin(); data != holder.end(); ++data) {
    art::ScopedAssertNoThreadSuspension nts("Updating runtime objects for redefinition");
    ClassRedefinition& redef = data.GetRedefinition();
    if (data.GetSourceClassLoader() != nullptr) {
      ClassLoaderHelper::UpdateJavaDexFile(data.GetJavaDexFile(), data.GetNewDexFileCookie());
    }
    art::mirror::Class* klass = data.GetMirrorClass();
    // TODO Rewrite so we don't do a stack walk for each and every class.
    redef.FindAndAllocateObsoleteMethods(klass);
    redef.UpdateClass(klass, data.GetNewDexCache(), data.GetOriginalDexFile());
  }
  RestoreObsoleteMethodMapsIfUnneeded(holder);
  // TODO We should check for if any of the redefined methods are intrinsic methods here and, if any
  // are, force a full-world deoptimization before finishing redefinition. If we don't do this then
  // methods that have been jitted prior to the current redefinition being applied might continue
  // to use the old versions of the intrinsics!
  // TODO Do the dex_file release at a more reasonable place. This works but it muddles who really
  // owns the DexFile and when ownership is transferred.
  ReleaseAllDexFiles();
  return OK;
}

void Redefiner::ClassRedefinition::UpdateMethods(art::ObjPtr<art::mirror::Class> mclass,
                                                 const art::DexFile::ClassDef& class_def) {
  art::ClassLinker* linker = driver_->runtime_->GetClassLinker();
  art::PointerSize image_pointer_size = linker->GetImagePointerSize();
  const art::DexFile::TypeId& declaring_class_id = dex_file_->GetTypeId(class_def.class_idx_);
  const art::DexFile& old_dex_file = mclass->GetDexFile();
  // Update methods.
  for (art::ArtMethod& method : mclass->GetDeclaredMethods(image_pointer_size)) {
    const art::DexFile::StringId* new_name_id = dex_file_->FindStringId(method.GetName());
    art::dex::TypeIndex method_return_idx =
        dex_file_->GetIndexForTypeId(*dex_file_->FindTypeId(method.GetReturnTypeDescriptor()));
    const auto* old_type_list = method.GetParameterTypeList();
    std::vector<art::dex::TypeIndex> new_type_list;
    for (uint32_t i = 0; old_type_list != nullptr && i < old_type_list->Size(); i++) {
      new_type_list.push_back(
          dex_file_->GetIndexForTypeId(
              *dex_file_->FindTypeId(
                  old_dex_file.GetTypeDescriptor(
                      old_dex_file.GetTypeId(
                          old_type_list->GetTypeItem(i).type_idx_)))));
    }
    const art::DexFile::ProtoId* proto_id = dex_file_->FindProtoId(method_return_idx,
                                                                   new_type_list);
    CHECK(proto_id != nullptr || old_type_list == nullptr);
    const art::DexFile::MethodId* method_id = dex_file_->FindMethodId(declaring_class_id,
                                                                      *new_name_id,
                                                                      *proto_id);
    CHECK(method_id != nullptr);
    uint32_t dex_method_idx = dex_file_->GetIndexForMethodId(*method_id);
    method.SetDexMethodIndex(dex_method_idx);
    linker->SetEntryPointsToInterpreter(&method);
    method.SetCodeItemOffset(dex_file_->FindCodeItemOffset(class_def, dex_method_idx));
    // Clear all the intrinsics related flags.
    method.SetNotIntrinsic();
  }
}

void Redefiner::ClassRedefinition::UpdateFields(art::ObjPtr<art::mirror::Class> mclass) {
  // TODO The IFields & SFields pointers should be combined like the methods_ arrays were.
  for (auto fields_iter : {mclass->GetIFields(), mclass->GetSFields()}) {
    for (art::ArtField& field : fields_iter) {
      std::string declaring_class_name;
      const art::DexFile::TypeId* new_declaring_id =
          dex_file_->FindTypeId(field.GetDeclaringClass()->GetDescriptor(&declaring_class_name));
      const art::DexFile::StringId* new_name_id = dex_file_->FindStringId(field.GetName());
      const art::DexFile::TypeId* new_type_id = dex_file_->FindTypeId(field.GetTypeDescriptor());
      CHECK(new_name_id != nullptr && new_type_id != nullptr && new_declaring_id != nullptr);
      const art::DexFile::FieldId* new_field_id =
          dex_file_->FindFieldId(*new_declaring_id, *new_name_id, *new_type_id);
      CHECK(new_field_id != nullptr);
      // We only need to update the index since the other data in the ArtField cannot be updated.
      field.SetDexFieldIndex(dex_file_->GetIndexForFieldId(*new_field_id));
    }
  }
}

// Performs updates to class that will allow us to verify it.
void Redefiner::ClassRedefinition::UpdateClass(
    art::ObjPtr<art::mirror::Class> mclass,
    art::ObjPtr<art::mirror::DexCache> new_dex_cache,
    art::ObjPtr<art::mirror::Object> original_dex_file) {
  DCHECK_EQ(dex_file_->NumClassDefs(), 1u);
  const art::DexFile::ClassDef& class_def = dex_file_->GetClassDef(0);
  UpdateMethods(mclass, class_def);
  UpdateFields(mclass);

  // Update the class fields.
  // Need to update class last since the ArtMethod gets its DexFile from the class (which is needed
  // to call GetReturnTypeDescriptor and GetParameterTypeList above).
  mclass->SetDexCache(new_dex_cache.Ptr());
  mclass->SetDexClassDefIndex(dex_file_->GetIndexForClassDef(class_def));
  mclass->SetDexTypeIndex(dex_file_->GetIndexForTypeId(*dex_file_->FindTypeId(class_sig_.c_str())));
  art::ObjPtr<art::mirror::ClassExt> ext(mclass->GetExtData());
  CHECK(!ext.IsNull());
  ext->SetOriginalDexFile(original_dex_file);

  // Notify the jit that all the methods in this class were redefined. Need to do this last since
  // the jit relies on the dex_file_ being correct (for native methods at least) to find the method
  // meta-data.
  art::jit::Jit* jit = driver_->runtime_->GetJit();
  if (jit != nullptr) {
    art::PointerSize image_pointer_size =
        driver_->runtime_->GetClassLinker()->GetImagePointerSize();
    auto code_cache = jit->GetCodeCache();
    // Non-invokable methods don't have any JIT data associated with them so we don't need to tell
    // the jit about them.
    for (art::ArtMethod& method : mclass->GetDeclaredMethods(image_pointer_size)) {
      if (method.IsInvokable()) {
        code_cache->NotifyMethodRedefined(&method);
      }
    }
  }
}

// Restores the old obsolete methods maps if it turns out they weren't needed (ie there were no new
// obsolete methods).
void Redefiner::ClassRedefinition::RestoreObsoleteMethodMapsIfUnneeded(
    const RedefinitionDataIter* cur_data) {
  art::mirror::Class* klass = GetMirrorClass();
  art::mirror::ClassExt* ext = klass->GetExtData();
  art::mirror::PointerArray* methods = ext->GetObsoleteMethods();
  art::mirror::PointerArray* old_methods = cur_data->GetOldObsoleteMethods();
  int32_t old_length = old_methods == nullptr ? 0 : old_methods->GetLength();
  int32_t expected_length =
      old_length + klass->NumDirectMethods() + klass->NumDeclaredVirtualMethods();
  // Check to make sure we are only undoing this one.
  if (expected_length == methods->GetLength()) {
    for (int32_t i = 0; i < expected_length; i++) {
      art::ArtMethod* expected = nullptr;
      if (i < old_length) {
        expected = old_methods->GetElementPtrSize<art::ArtMethod*>(i, art::kRuntimePointerSize);
      }
      if (methods->GetElementPtrSize<art::ArtMethod*>(i, art::kRuntimePointerSize) != expected) {
        // We actually have some new obsolete methods. Just abort since we cannot safely shrink the
        // obsolete methods array.
        return;
      }
    }
    // No new obsolete methods! We can get rid of the maps.
    ext->SetObsoleteArrays(cur_data->GetOldObsoleteMethods(), cur_data->GetOldDexCaches());
  }
}

// This function does all (java) allocations we need to do for the Class being redefined.
// TODO Change this name maybe?
bool Redefiner::ClassRedefinition::EnsureClassAllocationsFinished(
    /*out*/RedefinitionDataIter* cur_data) {
  art::StackHandleScope<2> hs(driver_->self_);
  art::Handle<art::mirror::Class> klass(hs.NewHandle(
      driver_->self_->DecodeJObject(klass_)->AsClass()));
  if (klass == nullptr) {
    RecordFailure(ERR(INVALID_CLASS), "Unable to decode class argument!");
    return false;
  }
  // Allocate the classExt
  art::Handle<art::mirror::ClassExt> ext(hs.NewHandle(klass->EnsureExtDataPresent(driver_->self_)));
  if (ext == nullptr) {
    // No memory. Clear exception (it's not useful) and return error.
    driver_->self_->AssertPendingOOMException();
    driver_->self_->ClearException();
    RecordFailure(ERR(OUT_OF_MEMORY), "Could not allocate ClassExt");
    return false;
  }
  // First save the old values of the 2 arrays that make up the obsolete methods maps.  Then
  // allocate the 2 arrays that make up the obsolete methods map.  Since the contents of the arrays
  // are only modified when all threads (other than the modifying one) are suspended we don't need
  // to worry about missing the unsyncronized writes to the array. We do synchronize when setting it
  // however, since that can happen at any time.
  cur_data->SetOldObsoleteMethods(ext->GetObsoleteMethods());
  cur_data->SetOldDexCaches(ext->GetObsoleteDexCaches());
  if (!ext->ExtendObsoleteArrays(
        driver_->self_, klass->GetDeclaredMethodsSlice(art::kRuntimePointerSize).size())) {
    // OOM. Clear exception and return error.
    driver_->self_->AssertPendingOOMException();
    driver_->self_->ClearException();
    RecordFailure(ERR(OUT_OF_MEMORY), "Unable to allocate/extend obsolete methods map");
    return false;
  }
  return true;
}

}  // namespace openjdkjvmti
