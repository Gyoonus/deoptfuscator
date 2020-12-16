/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "profile_saver.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "android-base/strings.h"

#include "art_method-inl.h"
#include "base/enums.h"
#include "base/logging.h"  // For VLOG.
#include "base/scoped_arena_containers.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "class_table-inl.h"
#include "compiler_filter.h"
#include "dex/dex_file_loader.h"
#include "dex_reference_collection.h"
#include "gc/collector_type.h"
#include "gc/gc_cause.h"
#include "gc/scoped_gc_critical_section.h"
#include "jit/profile_compilation_info.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

ProfileSaver* ProfileSaver::instance_ = nullptr;
pthread_t ProfileSaver::profiler_pthread_ = 0U;

// At what priority to schedule the saver threads. 9 is the lowest foreground priority on device.
static constexpr int kProfileSaverPthreadPriority = 9;

static void SetProfileSaverThreadPriority(pthread_t thread, int priority) {
#if defined(ART_TARGET_ANDROID)
  int result = setpriority(PRIO_PROCESS, pthread_gettid_np(thread), priority);
  if (result != 0) {
    LOG(ERROR) << "Failed to setpriority to :" << priority;
  }
#else
  UNUSED(thread);
  UNUSED(priority);
#endif
}

static int GetDefaultThreadPriority() {
#if defined(ART_TARGET_ANDROID)
  pthread_attr_t attr;
  sched_param param;
  pthread_attr_init(&attr);
  pthread_attr_getschedparam(&attr, &param);
  return param.sched_priority;
#else
  return 0;
#endif
}

ProfileSaver::ProfileSaver(const ProfileSaverOptions& options,
                           const std::string& output_filename,
                           jit::JitCodeCache* jit_code_cache,
                           const std::vector<std::string>& code_paths)
    : jit_code_cache_(jit_code_cache),
      shutting_down_(false),
      last_time_ns_saver_woke_up_(0),
      jit_activity_notifications_(0),
      wait_lock_("ProfileSaver wait lock"),
      period_condition_("ProfileSaver period condition", wait_lock_),
      total_bytes_written_(0),
      total_number_of_writes_(0),
      total_number_of_code_cache_queries_(0),
      total_number_of_skipped_writes_(0),
      total_number_of_failed_writes_(0),
      total_ms_of_sleep_(0),
      total_ns_of_work_(0),
      max_number_of_profile_entries_cached_(0),
      total_number_of_hot_spikes_(0),
      total_number_of_wake_ups_(0),
      options_(options) {
  DCHECK(options_.IsEnabled());
  AddTrackedLocations(output_filename, code_paths);
}

ProfileSaver::~ProfileSaver() {
  for (auto& it : profile_cache_) {
    delete it.second;
  }
}

void ProfileSaver::Run() {
  Thread* self = Thread::Current();

  // Fetch the resolved classes for the app images after sleeping for
  // options_.GetSaveResolvedClassesDelayMs().
  // TODO(calin) This only considers the case of the primary profile file.
  // Anything that gets loaded in the same VM will not have their resolved
  // classes save (unless they started before the initial saving was done).
  {
    MutexLock mu(self, wait_lock_);
    const uint64_t end_time = NanoTime() + MsToNs(options_.GetSaveResolvedClassesDelayMs());
    while (true) {
      const uint64_t current_time = NanoTime();
      if (current_time >= end_time) {
        break;
      }
      period_condition_.TimedWait(self, NsToMs(end_time - current_time), 0);
    }
    total_ms_of_sleep_ += options_.GetSaveResolvedClassesDelayMs();
  }
  FetchAndCacheResolvedClassesAndMethods(/*startup*/ true);


  // When we save without waiting for JIT notifications we use a simple
  // exponential back off policy bounded by max_wait_without_jit.
  uint32_t max_wait_without_jit = options_.GetMinSavePeriodMs() * 16;
  uint64_t cur_wait_without_jit = options_.GetMinSavePeriodMs();
  // Loop for the profiled methods.
  while (!ShuttingDown(self)) {
    uint64_t sleep_start = NanoTime();
    {
      uint64_t sleep_time = 0;
      {
        MutexLock mu(self, wait_lock_);
        if (options_.GetWaitForJitNotificationsToSave()) {
          period_condition_.Wait(self);
        } else {
          period_condition_.TimedWait(self, cur_wait_without_jit, 0);
          if (cur_wait_without_jit < max_wait_without_jit) {
            cur_wait_without_jit *= 2;
          }
        }
        sleep_time = NanoTime() - sleep_start;
      }
      // Check if the thread was woken up for shutdown.
      if (ShuttingDown(self)) {
        break;
      }
      total_number_of_wake_ups_++;
      // We might have been woken up by a huge number of notifications to guarantee saving.
      // If we didn't meet the minimum saving period go back to sleep (only if missed by
      // a reasonable margin).
      uint64_t min_save_period_ns = MsToNs(options_.GetMinSavePeriodMs());
      while (min_save_period_ns * 0.9 > sleep_time) {
        {
          MutexLock mu(self, wait_lock_);
          period_condition_.TimedWait(self, NsToMs(min_save_period_ns - sleep_time), 0);
          sleep_time = NanoTime() - sleep_start;
        }
        // Check if the thread was woken up for shutdown.
        if (ShuttingDown(self)) {
          break;
        }
        total_number_of_wake_ups_++;
      }
    }
    total_ms_of_sleep_ += NsToMs(NanoTime() - sleep_start);

    if (ShuttingDown(self)) {
      break;
    }

    uint16_t number_of_new_methods = 0;
    uint64_t start_work = NanoTime();
    bool profile_saved_to_disk = ProcessProfilingInfo(/*force_save*/false, &number_of_new_methods);
    // Update the notification counter based on result. Note that there might be contention on this
    // but we don't care about to be 100% precise.
    if (!profile_saved_to_disk) {
      // If we didn't save to disk it may be because we didn't have enough new methods.
      // Set the jit activity notifications to number_of_new_methods so we can wake up earlier
      // if needed.
      jit_activity_notifications_ = number_of_new_methods;
    }
    total_ns_of_work_ += NanoTime() - start_work;
  }
}

void ProfileSaver::NotifyJitActivity() {
  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  if (instance_ == nullptr || instance_->shutting_down_) {
    return;
  }
  instance_->NotifyJitActivityInternal();
}

void ProfileSaver::WakeUpSaver() {
  jit_activity_notifications_ = 0;
  last_time_ns_saver_woke_up_ = NanoTime();
  period_condition_.Signal(Thread::Current());
}

void ProfileSaver::NotifyJitActivityInternal() {
  // Unlikely to overflow but if it happens,
  // we would have waken up the saver long before that.
  jit_activity_notifications_++;
  // Note that we are not as precise as we could be here but we don't want to wake the saver
  // every time we see a hot method.
  if (jit_activity_notifications_ > options_.GetMinNotificationBeforeWake()) {
    MutexLock wait_mutex(Thread::Current(), wait_lock_);
    if ((NanoTime() - last_time_ns_saver_woke_up_) > MsToNs(options_.GetMinSavePeriodMs())) {
      WakeUpSaver();
    } else if (jit_activity_notifications_ > options_.GetMaxNotificationBeforeWake()) {
      // Make sure to wake up the saver if we see a spike in the number of notifications.
      // This is a precaution to avoid losing a big number of methods in case
      // this is a spike with no jit after.
      total_number_of_hot_spikes_++;
      WakeUpSaver();
    }
  }
}

class ScopedDefaultPriority {
 public:
  explicit ScopedDefaultPriority(pthread_t thread) : thread_(thread) {
    SetProfileSaverThreadPriority(thread_, GetDefaultThreadPriority());
  }

  ~ScopedDefaultPriority() {
    SetProfileSaverThreadPriority(thread_, kProfileSaverPthreadPriority);
  }

 private:
  const pthread_t thread_;
};

// GetClassLoadersVisitor takes a snapshot of the class loaders and stores them in the out
// class_loaders argument. Not affected by class unloading since there are no suspend points in
// the caller.
class GetClassLoadersVisitor : public ClassLoaderVisitor {
 public:
  explicit GetClassLoadersVisitor(VariableSizedHandleScope* hs,
                                  std::vector<Handle<mirror::ClassLoader>>* class_loaders)
      : hs_(hs),
        class_loaders_(class_loaders) {}

  void Visit(ObjPtr<mirror::ClassLoader> class_loader)
      REQUIRES_SHARED(Locks::classlinker_classes_lock_, Locks::mutator_lock_) OVERRIDE {
    class_loaders_->push_back(hs_->NewHandle(class_loader));
  }

 private:
  VariableSizedHandleScope* const hs_;
  std::vector<Handle<mirror::ClassLoader>>* const class_loaders_;
};

// GetClassesVisitor takes a snapshot of the loaded classes that we may want to visit and stores
// them in the out argument. Not affected by class unloading since there are no suspend points in
// the caller.
class GetClassesVisitor : public ClassVisitor {
 public:
  explicit GetClassesVisitor(bool profile_boot_class_path,
                             ScopedArenaVector<ObjPtr<mirror::Class>>* out)
      : profile_boot_class_path_(profile_boot_class_path),
        out_(out) {}

  virtual bool operator()(ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (klass->IsProxyClass() ||
        klass->IsArrayClass() ||
        klass->IsPrimitive() ||
        !klass->IsResolved() ||
        klass->IsErroneousResolved() ||
        (!profile_boot_class_path_ && klass->GetClassLoader() == nullptr)) {
      return true;
    }
    out_->push_back(klass);
    return true;
  }

 private:
  const bool profile_boot_class_path_;
  ScopedArenaVector<ObjPtr<mirror::Class>>* const out_;
};

using MethodReferenceCollection = DexReferenceCollection<uint16_t, ScopedArenaAllocatorAdapter>;
using TypeReferenceCollection = DexReferenceCollection<dex::TypeIndex,
                                                       ScopedArenaAllocatorAdapter>;

// Iterate over all of the loaded classes and visit each one. For each class, add it to the
// resolved_classes out argument if startup is true.
// Add methods to the hot_methods out argument if the number of samples is greater or equal to
// hot_method_sample_threshold, add it to sampled_methods if it has at least one sample.
static void SampleClassesAndExecutedMethods(pthread_t profiler_pthread,
                                            bool profile_boot_class_path,
                                            ScopedArenaAllocator* allocator,
                                            uint32_t hot_method_sample_threshold,
                                            bool startup,
                                            TypeReferenceCollection* resolved_classes,
                                            MethodReferenceCollection* hot_methods,
                                            MethodReferenceCollection* sampled_methods) {
  Thread* const self = Thread::Current();
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  // Restore profile saver thread priority during the GC critical section. This helps prevent
  // priority inversions blocking the GC for long periods of time.
  std::unique_ptr<ScopedDefaultPriority> sdp;
  // Only restore default priority if we are the profile saver thread. Other threads that call this
  // are threads calling Stop and the signal catcher (for SIGUSR1).
  if (pthread_self() == profiler_pthread) {
    sdp.reset(new ScopedDefaultPriority(profiler_pthread));
  }

  // Do ScopedGCCriticalSection before acquiring mutator lock to prevent the GC running and
  // blocking threads during thread root flipping. Since the GC is a background thread, blocking it
  // is not a problem.
  ScopedObjectAccess soa(self);
  gc::ScopedGCCriticalSection sgcs(self,
                                   gc::kGcCauseProfileSaver,
                                   gc::kCollectorTypeCriticalSection);
  VariableSizedHandleScope hs(soa.Self());
  std::vector<Handle<mirror::ClassLoader>> class_loaders;
  if (profile_boot_class_path) {
    // First add the boot class loader since visit classloaders doesn't visit it.
    class_loaders.push_back(hs.NewHandle<mirror::ClassLoader>(nullptr));
  }
  GetClassLoadersVisitor class_loader_visitor(&hs, &class_loaders);
  {
    // Read the class loaders into a temporary array to prevent contention problems on the
    // class_linker_classes_lock.
    ScopedTrace trace2("Get class loaders");
    ReaderMutexLock mu(soa.Self(), *Locks::classlinker_classes_lock_);
    class_linker->VisitClassLoaders(&class_loader_visitor);
  }
  ScopedArenaVector<ObjPtr<mirror::Class>> classes(allocator->Adapter());
  for (Handle<mirror::ClassLoader> class_loader : class_loaders) {
    ClassTable* table = class_linker->ClassTableForClassLoader(class_loader.Get());
    if (table == nullptr) {
      // If the class loader has not loaded any classes, it may have a null table.
      continue;
    }
    GetClassesVisitor get_classes_visitor(profile_boot_class_path, &classes);
    {
      // Collect the classes into a temporary array to prevent lock contention on the class
      // table lock. We want to avoid blocking class loading in other threads as much as
      // possible.
      ScopedTrace trace3("Visiting class table");
      table->Visit(get_classes_visitor);
    }
    for (ObjPtr<mirror::Class> klass : classes) {
      if (startup) {
        // We only record classes for the startup case. This may change in the future.
        resolved_classes->AddReference(&klass->GetDexFile(), klass->GetDexTypeIndex());
      }
      // Visit all of the methods in the class to see which ones were executed.
      for (ArtMethod& method : klass->GetMethods(kRuntimePointerSize)) {
        if (!method.IsNative()) {
          DCHECK(!method.IsProxyMethod());
          const uint16_t counter = method.GetCounter();
          // Mark startup methods as hot if they have more than hot_method_sample_threshold
          // samples. This means they will get compiled by the compiler driver.
          if (method.GetProfilingInfo(kRuntimePointerSize) != nullptr ||
              method.PreviouslyWarm() ||
              counter >= hot_method_sample_threshold) {
            hot_methods->AddReference(method.GetDexFile(), method.GetDexMethodIndex());
          } else if (counter != 0) {
            sampled_methods->AddReference(method.GetDexFile(), method.GetDexMethodIndex());
          }
        } else {
          // We do not record native methods. Once we AOT-compile the app, all native
          // methods shall have their thunks compiled.
        }
      }
    }
    classes.clear();
  }
}

void ProfileSaver::FetchAndCacheResolvedClassesAndMethods(bool startup) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  const uint64_t start_time = NanoTime();

  // Resolve any new registered locations.
  ResolveTrackedLocations();

  Thread* const self = Thread::Current();
  Runtime* const runtime = Runtime::Current();
  ArenaStack stack(runtime->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  MethodReferenceCollection hot_methods(allocator.Adapter(), allocator.Adapter());
  MethodReferenceCollection sampled_methods(allocator.Adapter(), allocator.Adapter());
  TypeReferenceCollection resolved_classes(allocator.Adapter(), allocator.Adapter());
  const bool is_low_ram = Runtime::Current()->GetHeap()->IsLowMemoryMode();
  pthread_t profiler_pthread;
  {
    MutexLock mu(self, *Locks::profiler_lock_);
    profiler_pthread = profiler_pthread_;
  }
  const uint32_t hot_method_sample_threshold = startup ?
      options_.GetHotStartupMethodSamples(is_low_ram) :
      std::numeric_limits<uint32_t>::max();
  SampleClassesAndExecutedMethods(profiler_pthread,
                                  options_.GetProfileBootClassPath(),
                                  &allocator,
                                  hot_method_sample_threshold,
                                  startup,
                                  &resolved_classes,
                                  &hot_methods,
                                  &sampled_methods);
  MutexLock mu(self, *Locks::profiler_lock_);
  uint64_t total_number_of_profile_entries_cached = 0;
  using Hotness = ProfileCompilationInfo::MethodHotness;

  for (const auto& it : tracked_dex_base_locations_) {
    std::set<DexCacheResolvedClasses> resolved_classes_for_location;
    const std::string& filename = it.first;
    auto info_it = profile_cache_.find(filename);
    if (info_it == profile_cache_.end()) {
      info_it = profile_cache_.Put(
          filename,
          new ProfileCompilationInfo(Runtime::Current()->GetArenaPool()));
    }
    ProfileCompilationInfo* cached_info = info_it->second;

    const std::set<std::string>& locations = it.second;
    for (const auto& pair : hot_methods.GetMap()) {
      const DexFile* const dex_file = pair.first;
      const std::string base_location = DexFileLoader::GetBaseLocation(dex_file->GetLocation());
      if (locations.find(base_location) != locations.end()) {
        const MethodReferenceCollection::IndexVector& indices = pair.second;
        uint8_t flags = Hotness::kFlagHot;
        flags |= startup ? Hotness::kFlagStartup : Hotness::kFlagPostStartup;
        cached_info->AddMethodsForDex(
            static_cast<Hotness::Flag>(flags),
            dex_file,
            indices.begin(),
            indices.end());
      }
    }
    for (const auto& pair : sampled_methods.GetMap()) {
      const DexFile* const dex_file = pair.first;
      const std::string base_location = DexFileLoader::GetBaseLocation(dex_file->GetLocation());
      if (locations.find(base_location) != locations.end()) {
        const MethodReferenceCollection::IndexVector& indices = pair.second;
        cached_info->AddMethodsForDex(startup ? Hotness::kFlagStartup : Hotness::kFlagPostStartup,
                                      dex_file,
                                      indices.begin(),
                                      indices.end());
      }
    }
    for (const auto& pair : resolved_classes.GetMap()) {
      const DexFile* const dex_file = pair.first;
      const std::string base_location = DexFileLoader::GetBaseLocation(dex_file->GetLocation());
      if (locations.find(base_location) != locations.end()) {
        const TypeReferenceCollection::IndexVector& classes = pair.second;
        VLOG(profiler) << "Added " << classes.size() << " classes for location "
                       << base_location
                       << " (" << dex_file->GetLocation() << ")";
        cached_info->AddClassesForDex(dex_file, classes.begin(), classes.end());
      } else {
        VLOG(profiler) << "Location not found " << base_location
                       << " (" << dex_file->GetLocation() << ")";
      }
    }
    total_number_of_profile_entries_cached += resolved_classes_for_location.size();
  }
  max_number_of_profile_entries_cached_ = std::max(
      max_number_of_profile_entries_cached_,
      total_number_of_profile_entries_cached);
  VLOG(profiler) << "Profile saver recorded " << hot_methods.NumReferences() << " hot methods and "
                 << sampled_methods.NumReferences() << " sampled methods with threshold "
                 << hot_method_sample_threshold << " in "
                 << PrettyDuration(NanoTime() - start_time);
}

bool ProfileSaver::ProcessProfilingInfo(bool force_save, /*out*/uint16_t* number_of_new_methods) {
  ScopedTrace trace(__PRETTY_FUNCTION__);

  // Resolve any new registered locations.
  ResolveTrackedLocations();

  SafeMap<std::string, std::set<std::string>> tracked_locations;
  {
    // Make a copy so that we don't hold the lock while doing I/O.
    MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
    tracked_locations = tracked_dex_base_locations_;
  }

  bool profile_file_saved = false;
  if (number_of_new_methods != nullptr) {
    *number_of_new_methods = 0;
  }

  // We only need to do this once, not once per dex location.
  // TODO: Figure out a way to only do it when stuff has changed? It takes 30-50ms.
  FetchAndCacheResolvedClassesAndMethods(/*startup*/ false);

  for (const auto& it : tracked_locations) {
    if (!force_save && ShuttingDown(Thread::Current())) {
      // The ProfileSaver is in shutdown mode, meaning a stop request was made and
      // we need to exit cleanly (by waiting for the saver thread to finish). Unless
      // we have a request for a forced save, do not do any processing so that we
      // speed up the exit.
      return true;
    }
    const std::string& filename = it.first;
    const std::set<std::string>& locations = it.second;
    std::vector<ProfileMethodInfo> profile_methods;
    {
      ScopedObjectAccess soa(Thread::Current());
      jit_code_cache_->GetProfiledMethods(locations, profile_methods);
      total_number_of_code_cache_queries_++;
    }
    {
      ProfileCompilationInfo info(Runtime::Current()->GetArenaPool());
      if (!info.Load(filename, /*clear_if_invalid*/ true)) {
        LOG(WARNING) << "Could not forcefully load profile " << filename;
        continue;
      }
      uint64_t last_save_number_of_methods = info.GetNumberOfMethods();
      uint64_t last_save_number_of_classes = info.GetNumberOfResolvedClasses();

      // Try to add the method data. Note this may fail is the profile loaded from disk contains
      // outdated data (e.g. the previous profiled dex files might have been updated).
      // If this happens we clear the profile data and for the save to ensure the file is cleared.
      if (!info.AddMethods(profile_methods,
              ProfileCompilationInfo::MethodHotness::kFlagPostStartup)) {
        LOG(WARNING) << "Could not add methods to the existing profiler. "
            << "Clearing the profile data.";
        info.ClearData();
        force_save = true;
      }

      auto profile_cache_it = profile_cache_.find(filename);
      if (profile_cache_it != profile_cache_.end()) {
        if (!info.MergeWith(*(profile_cache_it->second))) {
          LOG(WARNING) << "Could not merge the profile. Clearing the profile data.";
          info.ClearData();
          force_save = true;
        }
      }

      int64_t delta_number_of_methods =
          info.GetNumberOfMethods() - last_save_number_of_methods;
      int64_t delta_number_of_classes =
          info.GetNumberOfResolvedClasses() - last_save_number_of_classes;

      if (!force_save &&
          delta_number_of_methods < options_.GetMinMethodsToSave() &&
          delta_number_of_classes < options_.GetMinClassesToSave()) {
        VLOG(profiler) << "Not enough information to save to: " << filename
                       << " Number of methods: " << delta_number_of_methods
                       << " Number of classes: " << delta_number_of_classes;
        total_number_of_skipped_writes_++;
        continue;
      }

      if (number_of_new_methods != nullptr) {
        *number_of_new_methods =
            std::max(static_cast<uint16_t>(delta_number_of_methods),
                     *number_of_new_methods);
      }
      uint64_t bytes_written;
      // Force the save. In case the profile data is corrupted or the the profile
      // has the wrong version this will "fix" the file to the correct format.
      if (info.Save(filename, &bytes_written)) {
        // We managed to save the profile. Clear the cache stored during startup.
        if (profile_cache_it != profile_cache_.end()) {
          ProfileCompilationInfo *cached_info = profile_cache_it->second;
          profile_cache_.erase(profile_cache_it);
          delete cached_info;
        }
        if (bytes_written > 0) {
          total_number_of_writes_++;
          total_bytes_written_ += bytes_written;
          profile_file_saved = true;
        } else {
          // At this point we could still have avoided the write.
          // We load and merge the data from the file lazily at its first ever
          // save attempt. So, whatever we are trying to save could already be
          // in the file.
          total_number_of_skipped_writes_++;
        }
      } else {
        LOG(WARNING) << "Could not save profiling info to " << filename;
        total_number_of_failed_writes_++;
      }
    }
  }

  // Trim the maps to madvise the pages used for profile info.
  // It is unlikely we will need them again in the near feature.
  Runtime::Current()->GetArenaPool()->TrimMaps();

  return profile_file_saved;
}

void* ProfileSaver::RunProfileSaverThread(void* arg) {
  Runtime* runtime = Runtime::Current();

  bool attached = runtime->AttachCurrentThread("Profile Saver",
                                               /*as_daemon*/true,
                                               runtime->GetSystemThreadGroup(),
                                               /*create_peer*/true);
  if (!attached) {
    CHECK(runtime->IsShuttingDown(Thread::Current()));
    return nullptr;
  }

  ProfileSaver* profile_saver = reinterpret_cast<ProfileSaver*>(arg);
  profile_saver->Run();

  runtime->DetachCurrentThread();
  VLOG(profiler) << "Profile saver shutdown";
  return nullptr;
}

static bool ShouldProfileLocation(const std::string& location, bool profile_aot_code) {
  if (profile_aot_code) {
    // If we have to profile all the code, irrespective of its compilation state, return true
    // right away.
    return true;
  }

  OatFileManager& oat_manager = Runtime::Current()->GetOatFileManager();
  const OatFile* oat_file = oat_manager.FindOpenedOatFileFromDexLocation(location);
  if (oat_file == nullptr) {
    // This can happen if we fallback to run code directly from the APK.
    // Profile it with the hope that the background dexopt will get us back into
    // a good state.
    VLOG(profiler) << "Asked to profile a location without an oat file:" << location;
    return true;
  }
  CompilerFilter::Filter filter = oat_file->GetCompilerFilter();
  if ((filter == CompilerFilter::kSpeed) || (filter == CompilerFilter::kEverything)) {
    VLOG(profiler)
        << "Skip profiling oat file because it's already speed|everything compiled: "
        << location << " oat location: " << oat_file->GetLocation();
    return false;
  }
  return true;
}

void ProfileSaver::Start(const ProfileSaverOptions& options,
                         const std::string& output_filename,
                         jit::JitCodeCache* jit_code_cache,
                         const std::vector<std::string>& code_paths) {
  Runtime* const runtime = Runtime::Current();
  DCHECK(options.IsEnabled());
  DCHECK(runtime->GetJit() != nullptr);
  DCHECK(!output_filename.empty());
  DCHECK(jit_code_cache != nullptr);

  std::vector<std::string> code_paths_to_profile;
  for (const std::string& location : code_paths) {
    if (ShouldProfileLocation(location, options.GetProfileAOTCode()))  {
      code_paths_to_profile.push_back(location);
    }
  }

  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  // Support getting profile samples for the boot class path. This will be used to generate the boot
  // image profile. The intention is to use this code to generate to boot image but not use it in
  // production. b/37966211
  if (options.GetProfileBootClassPath()) {
    std::set<std::string> code_paths_keys;
    for (const std::string& location : code_paths) {
      code_paths_keys.insert(ProfileCompilationInfo::GetProfileDexFileKey(location));
    }
    for (const DexFile* dex_file : runtime->GetClassLinker()->GetBootClassPath()) {
      // Don't check ShouldProfileLocation since the boot class path may be speed compiled.
      const std::string& location = dex_file->GetLocation();
      const std::string key = ProfileCompilationInfo::GetProfileDexFileKey(location);
      VLOG(profiler) << "Registering boot dex file " << location;
      if (code_paths_keys.find(key) != code_paths_keys.end()) {
        LOG(WARNING) << "Boot class path location key conflicts with code path " << location;
      } else if (instance_ == nullptr) {
        // Only add the boot class path once since Start may be called multiple times for secondary
        // dexes.
        // We still do the collision check above. This handles any secondary dexes that conflict
        // with the boot class path dex files.
        code_paths_to_profile.push_back(location);
      }
    }
  }
  if (code_paths_to_profile.empty()) {
    VLOG(profiler) << "No code paths should be profiled.";
    return;
  }

  if (instance_ != nullptr) {
    // If we already have an instance, make sure it uses the same jit_code_cache.
    // This may be called multiple times via Runtime::registerAppInfo (e.g. for
    // apps which share the same runtime).
    DCHECK_EQ(instance_->jit_code_cache_, jit_code_cache);
    // Add the code_paths to the tracked locations.
    instance_->AddTrackedLocations(output_filename, code_paths_to_profile);
    return;
  }

  VLOG(profiler) << "Starting profile saver using output file: " << output_filename
      << ". Tracking: " << android::base::Join(code_paths_to_profile, ':');

  instance_ = new ProfileSaver(options,
                               output_filename,
                               jit_code_cache,
                               code_paths_to_profile);

  // Create a new thread which does the saving.
  CHECK_PTHREAD_CALL(
      pthread_create,
      (&profiler_pthread_, nullptr, &RunProfileSaverThread, reinterpret_cast<void*>(instance_)),
      "Profile saver thread");

  SetProfileSaverThreadPriority(profiler_pthread_, kProfileSaverPthreadPriority);
}

void ProfileSaver::Stop(bool dump_info) {
  ProfileSaver* profile_saver = nullptr;
  pthread_t profiler_pthread = 0U;

  {
    MutexLock profiler_mutex(Thread::Current(), *Locks::profiler_lock_);
    VLOG(profiler) << "Stopping profile saver thread";
    profile_saver = instance_;
    profiler_pthread = profiler_pthread_;
    if (instance_ == nullptr) {
      DCHECK(false) << "Tried to stop a profile saver which was not started";
      return;
    }
    if (instance_->shutting_down_) {
      DCHECK(false) << "Tried to stop the profile saver twice";
      return;
    }
    instance_->shutting_down_ = true;
  }

  {
    // Wake up the saver thread if it is sleeping to allow for a clean exit.
    MutexLock wait_mutex(Thread::Current(), profile_saver->wait_lock_);
    profile_saver->period_condition_.Signal(Thread::Current());
  }

  // Force save everything before destroying the thread since we want profiler_pthread_ to remain
  // valid.
  instance_->ProcessProfilingInfo(/*force_save*/true, /*number_of_new_methods*/nullptr);

  // Wait for the saver thread to stop.
  CHECK_PTHREAD_CALL(pthread_join, (profiler_pthread, nullptr), "profile saver thread shutdown");

  {
    MutexLock profiler_mutex(Thread::Current(), *Locks::profiler_lock_);
    if (dump_info) {
      instance_->DumpInfo(LOG_STREAM(INFO));
    }
    instance_ = nullptr;
    profiler_pthread_ = 0U;
  }
  delete profile_saver;
}

bool ProfileSaver::ShuttingDown(Thread* self) {
  MutexLock mu(self, *Locks::profiler_lock_);
  return shutting_down_;
}

bool ProfileSaver::IsStarted() {
  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  return instance_ != nullptr;
}

static void AddTrackedLocationsToMap(const std::string& output_filename,
                                     const std::vector<std::string>& code_paths,
                                     SafeMap<std::string, std::set<std::string>>* map) {
  auto it = map->find(output_filename);
  if (it == map->end()) {
    map->Put(output_filename, std::set<std::string>(code_paths.begin(), code_paths.end()));
  } else {
    it->second.insert(code_paths.begin(), code_paths.end());
  }
}

void ProfileSaver::AddTrackedLocations(const std::string& output_filename,
                                       const std::vector<std::string>& code_paths) {
  // Add the code paths to the list of tracked location.
  AddTrackedLocationsToMap(output_filename, code_paths, &tracked_dex_base_locations_);
  // The code paths may contain symlinks which could fool the profiler.
  // If the dex file is compiled with an absolute location but loaded with symlink
  // the profiler could skip the dex due to location mismatch.
  // To avoid this, we add the code paths to the temporary cache of 'to_be_resolved'
  // locations. When the profiler thread executes we will resolve the paths to their
  // real paths.
  // Note that we delay taking the realpath to avoid spending more time than needed
  // when registering location (as it is done during app launch).
  AddTrackedLocationsToMap(output_filename,
                           code_paths,
                           &tracked_dex_base_locations_to_be_resolved_);
}

void ProfileSaver::DumpInstanceInfo(std::ostream& os) {
  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  if (instance_ != nullptr) {
    instance_->DumpInfo(os);
  }
}

void ProfileSaver::DumpInfo(std::ostream& os) {
  os << "ProfileSaver total_bytes_written=" << total_bytes_written_ << '\n'
     << "ProfileSaver total_number_of_writes=" << total_number_of_writes_ << '\n'
     << "ProfileSaver total_number_of_code_cache_queries="
     << total_number_of_code_cache_queries_ << '\n'
     << "ProfileSaver total_number_of_skipped_writes=" << total_number_of_skipped_writes_ << '\n'
     << "ProfileSaver total_number_of_failed_writes=" << total_number_of_failed_writes_ << '\n'
     << "ProfileSaver total_ms_of_sleep=" << total_ms_of_sleep_ << '\n'
     << "ProfileSaver total_ms_of_work=" << NsToMs(total_ns_of_work_) << '\n'
     << "ProfileSaver max_number_profile_entries_cached="
     << max_number_of_profile_entries_cached_ << '\n'
     << "ProfileSaver total_number_of_hot_spikes=" << total_number_of_hot_spikes_ << '\n'
     << "ProfileSaver total_number_of_wake_ups=" << total_number_of_wake_ups_ << '\n';
}


void ProfileSaver::ForceProcessProfiles() {
  ProfileSaver* saver = nullptr;
  {
    MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
    saver = instance_;
  }
  // TODO(calin): this is not actually thread safe as the instance_ may have been deleted,
  // but we only use this in testing when we now this won't happen.
  // Refactor the way we handle the instance so that we don't end up in this situation.
  if (saver != nullptr) {
    saver->ProcessProfilingInfo(/*force_save*/true, /*number_of_new_methods*/nullptr);
  }
}

bool ProfileSaver::HasSeenMethod(const std::string& profile, bool hot, MethodReference ref) {
  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  if (instance_ != nullptr) {
    ProfileCompilationInfo info(Runtime::Current()->GetArenaPool());
    if (!info.Load(profile, /*clear_if_invalid*/false)) {
      return false;
    }
    ProfileCompilationInfo::MethodHotness hotness = info.GetMethodHotness(ref);
    // Ignore hot parameter for now since it was causing test 595 to be flaky. TODO: Investigate.
    // b/63635729
    UNUSED(hot);
    return hotness.IsInProfile();
  }
  return false;
}

void ProfileSaver::ResolveTrackedLocations() {
  SafeMap<std::string, std::set<std::string>> locations_to_be_resolved;
  {
    // Make a copy so that we don't hold the lock while doing I/O.
    MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
    locations_to_be_resolved = tracked_dex_base_locations_to_be_resolved_;
    tracked_dex_base_locations_to_be_resolved_.clear();
  }

  // Resolve the locations.
  SafeMap<std::string, std::vector<std::string>> resolved_locations_map;
  for (const auto& it : locations_to_be_resolved) {
    const std::string& filename = it.first;
    const std::set<std::string>& locations = it.second;
    auto resolved_locations_it = resolved_locations_map.Put(
        filename,
        std::vector<std::string>(locations.size()));

    for (const auto& location : locations) {
      UniqueCPtr<const char[]> location_real(realpath(location.c_str(), nullptr));
      // Note that it's ok if we cannot get the real path.
      if (location_real != nullptr) {
        resolved_locations_it->second.emplace_back(location_real.get());
      }
    }
  }

  // Add the resolved locations to the tracked collection.
  MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
  for (const auto& it : resolved_locations_map) {
    AddTrackedLocationsToMap(it.first, it.second, &tracked_dex_base_locations_);
  }
}

}   // namespace art
