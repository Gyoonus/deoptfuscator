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

#ifndef ART_RUNTIME_HIDDEN_API_H_
#define ART_RUNTIME_HIDDEN_API_H_

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/mutex.h"
#include "dex/hidden_api_access_flags.h"
#include "mirror/class-inl.h"
#include "reflection.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

// Hidden API enforcement policy
// This must be kept in sync with ApplicationInfo.ApiEnforcementPolicy in
// frameworks/base/core/java/android/content/pm/ApplicationInfo.java
enum class EnforcementPolicy {
  kNoChecks             = 0,
  kJustWarn             = 1,  // keep checks enabled, but allow everything (enables logging)
  kDarkGreyAndBlackList = 2,  // ban dark grey & blacklist
  kBlacklistOnly        = 3,  // ban blacklist violations only
  kMax = kBlacklistOnly,
};

inline EnforcementPolicy EnforcementPolicyFromInt(int api_policy_int) {
  DCHECK_GE(api_policy_int, 0);
  DCHECK_LE(api_policy_int, static_cast<int>(EnforcementPolicy::kMax));
  return static_cast<EnforcementPolicy>(api_policy_int);
}

enum Action {
  kAllow,
  kAllowButWarn,
  kAllowButWarnAndToast,
  kDeny
};

enum AccessMethod {
  kNone,  // internal test that does not correspond to an actual access by app
  kReflection,
  kJNI,
  kLinking,
};

// Do not change the values of items in this enum, as they are written to the
// event log for offline analysis. Any changes will interfere with that analysis.
enum AccessContextFlags {
  // Accessed member is a field if this bit is set, else a method
  kMemberIsField = 1 << 0,
  // Indicates if access was denied to the member, instead of just printing a warning.
  kAccessDenied  = 1 << 1,
};

inline Action GetActionFromAccessFlags(HiddenApiAccessFlags::ApiList api_list) {
  if (api_list == HiddenApiAccessFlags::kWhitelist) {
    return kAllow;
  }

  EnforcementPolicy policy = Runtime::Current()->GetHiddenApiEnforcementPolicy();
  if (policy == EnforcementPolicy::kNoChecks) {
    // Exit early. Nothing to enforce.
    return kAllow;
  }

  // if policy is "just warn", always warn. We returned above for whitelist APIs.
  if (policy == EnforcementPolicy::kJustWarn) {
    return kAllowButWarn;
  }
  DCHECK(policy >= EnforcementPolicy::kDarkGreyAndBlackList);
  // The logic below relies on equality of values in the enums EnforcementPolicy and
  // HiddenApiAccessFlags::ApiList, and their ordering. Assertions are in hidden_api.cc.
  if (static_cast<int>(policy) > static_cast<int>(api_list)) {
    return api_list == HiddenApiAccessFlags::kDarkGreylist
        ? kAllowButWarnAndToast
        : kAllowButWarn;
  } else {
    return kDeny;
  }
}

class ScopedHiddenApiEnforcementPolicySetting {
 public:
  explicit ScopedHiddenApiEnforcementPolicySetting(EnforcementPolicy new_policy)
      : initial_policy_(Runtime::Current()->GetHiddenApiEnforcementPolicy()) {
    Runtime::Current()->SetHiddenApiEnforcementPolicy(new_policy);
  }

  ~ScopedHiddenApiEnforcementPolicySetting() {
    Runtime::Current()->SetHiddenApiEnforcementPolicy(initial_policy_);
  }

 private:
  const EnforcementPolicy initial_policy_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHiddenApiEnforcementPolicySetting);
};

// Implementation details. DO NOT ACCESS DIRECTLY.
namespace detail {

// Class to encapsulate the signature of a member (ArtField or ArtMethod). This
// is used as a helper when matching prefixes, and when logging the signature.
class MemberSignature {
 private:
  enum MemberType {
    kField,
    kMethod,
  };

  std::string class_name_;
  std::string member_name_;
  std::string type_signature_;
  std::string tmp_;
  MemberType type_;

  inline std::vector<const char*> GetSignatureParts() const;

 public:
  explicit MemberSignature(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_);
  explicit MemberSignature(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const;

  // Performs prefix match on this member. Since the full member signature is
  // composed of several parts, we match each part in turn (rather than
  // building the entire thing in memory and performing a simple prefix match)
  bool DoesPrefixMatch(const std::string& prefix) const;

  bool IsExempted(const std::vector<std::string>& exemptions);

  void WarnAboutAccess(AccessMethod access_method, HiddenApiAccessFlags::ApiList list);

  void LogAccessToEventLog(AccessMethod access_method, Action action_taken);
};

template<typename T>
Action GetMemberActionImpl(T* member,
                           HiddenApiAccessFlags::ApiList api_list,
                           Action action,
                           AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Returns true if the caller is either loaded by the boot strap class loader or comes from
// a dex file located in ${ANDROID_ROOT}/framework/.
ALWAYS_INLINE
inline bool IsCallerTrusted(ObjPtr<mirror::Class> caller,
                            ObjPtr<mirror::ClassLoader> caller_class_loader,
                            ObjPtr<mirror::DexCache> caller_dex_cache)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (caller_class_loader.IsNull()) {
    // Boot class loader.
    return true;
  }

  if (!caller_dex_cache.IsNull()) {
    const DexFile* caller_dex_file = caller_dex_cache->GetDexFile();
    if (caller_dex_file != nullptr && caller_dex_file->IsPlatformDexFile()) {
      // Caller is in a platform dex file.
      return true;
    }
  }

  if (!caller.IsNull() &&
      caller->ShouldSkipHiddenApiChecks() &&
      Runtime::Current()->IsJavaDebuggable()) {
    // We are in debuggable mode and this caller has been marked trusted.
    return true;
  }

  return false;
}

}  // namespace detail

// Returns true if access to `member` should be denied to the caller of the
// reflective query. The decision is based on whether the caller is trusted or
// not. Because different users of this function determine this in a different
// way, `fn_caller_is_trusted(self)` is called and should return true if the
// caller is allowed to access the platform.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline Action GetMemberAction(T* member,
                              Thread* self,
                              std::function<bool(Thread*)> fn_caller_is_trusted,
                              AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

  // Decode hidden API access flags.
  // NB Multiple threads might try to access (and overwrite) these simultaneously,
  // causing a race. We only do that if access has not been denied, so the race
  // cannot change Java semantics. We should, however, decode the access flags
  // once and use it throughout this function, otherwise we may get inconsistent
  // results, e.g. print whitelist warnings (b/78327881).
  HiddenApiAccessFlags::ApiList api_list = member->GetHiddenApiAccessFlags();

  Action action = GetActionFromAccessFlags(member->GetHiddenApiAccessFlags());
  if (action == kAllow) {
    // Nothing to do.
    return action;
  }

  // Member is hidden. Invoke `fn_caller_in_platform` and find the origin of the access.
  // This can be *very* expensive. Save it for last.
  if (fn_caller_is_trusted(self)) {
    // Caller is trusted. Exit.
    return kAllow;
  }

  // Member is hidden and caller is not in the platform.
  return detail::GetMemberActionImpl(member, api_list, action, access_method);
}

inline bool IsCallerTrusted(ObjPtr<mirror::Class> caller) REQUIRES_SHARED(Locks::mutator_lock_) {
  return !caller.IsNull() &&
      detail::IsCallerTrusted(caller, caller->GetClassLoader(), caller->GetDexCache());
}

// Returns true if access to `member` should be denied to a caller loaded with
// `caller_class_loader`.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline Action GetMemberAction(T* member,
                              ObjPtr<mirror::ClassLoader> caller_class_loader,
                              ObjPtr<mirror::DexCache> caller_dex_cache,
                              AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  bool is_caller_trusted =
      detail::IsCallerTrusted(/* caller */ nullptr, caller_class_loader, caller_dex_cache);
  return GetMemberAction(member,
                         /* thread */ nullptr,
                         [is_caller_trusted] (Thread*) { return is_caller_trusted; },
                         access_method);
}

// Calls back into managed code to notify VMRuntime.nonSdkApiUsageConsumer that
// |member| was accessed. This is usually called when an API is on the black,
// dark grey or light grey lists. Given that the callback can execute arbitrary
// code, a call to this method can result in thread suspension.
template<typename T> void NotifyHiddenApiListener(T* member)
    REQUIRES_SHARED(Locks::mutator_lock_);


}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
