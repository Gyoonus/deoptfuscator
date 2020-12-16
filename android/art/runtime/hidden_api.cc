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

#include <metricslogger/metrics_logger.h>

#include "hidden_api.h"

#include <nativehelper/scoped_local_ref.h>

#include "base/dumpable.h"
#include "thread-current-inl.h"
#include "well_known_classes.h"

using android::metricslogger::ComplexEventLogger;
using android::metricslogger::ACTION_HIDDEN_API_ACCESSED;
using android::metricslogger::FIELD_HIDDEN_API_ACCESS_METHOD;
using android::metricslogger::FIELD_HIDDEN_API_ACCESS_DENIED;
using android::metricslogger::FIELD_HIDDEN_API_SIGNATURE;

namespace art {
namespace hiddenapi {

// Set to true if we should always print a warning in logcat for all hidden API accesses, not just
// dark grey and black. This can be set to true for developer preview / beta builds, but should be
// false for public release builds.
// Note that when flipping this flag, you must also update the expectations of test 674-hiddenapi
// as it affects whether or not we warn for light grey APIs that have been added to the exemptions
// list.
static constexpr bool kLogAllAccesses = false;

static inline std::ostream& operator<<(std::ostream& os, AccessMethod value) {
  switch (value) {
    case kNone:
      LOG(FATAL) << "Internal access to hidden API should not be logged";
      UNREACHABLE();
    case kReflection:
      os << "reflection";
      break;
    case kJNI:
      os << "JNI";
      break;
    case kLinking:
      os << "linking";
      break;
  }
  return os;
}

static constexpr bool EnumsEqual(EnforcementPolicy policy, HiddenApiAccessFlags::ApiList apiList) {
  return static_cast<int>(policy) == static_cast<int>(apiList);
}

// GetMemberAction-related static_asserts.
static_assert(
    EnumsEqual(EnforcementPolicy::kDarkGreyAndBlackList, HiddenApiAccessFlags::kDarkGreylist) &&
    EnumsEqual(EnforcementPolicy::kBlacklistOnly, HiddenApiAccessFlags::kBlacklist),
    "Mismatch between EnforcementPolicy and ApiList enums");
static_assert(
    EnforcementPolicy::kJustWarn < EnforcementPolicy::kDarkGreyAndBlackList &&
    EnforcementPolicy::kDarkGreyAndBlackList < EnforcementPolicy::kBlacklistOnly,
    "EnforcementPolicy values ordering not correct");

namespace detail {

MemberSignature::MemberSignature(ArtField* field) {
  class_name_ = field->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = field->GetName();
  type_signature_ = field->GetTypeDescriptor();
  type_ = kField;
}

MemberSignature::MemberSignature(ArtMethod* method) {
  // If this is a proxy method, print the signature of the interface method.
  method = method->GetInterfaceMethodIfProxy(
      Runtime::Current()->GetClassLinker()->GetImagePointerSize());

  class_name_ = method->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = method->GetName();
  type_signature_ = method->GetSignature().ToString();
  type_ = kMethod;
}

inline std::vector<const char*> MemberSignature::GetSignatureParts() const {
  if (type_ == kField) {
    return { class_name_.c_str(), "->", member_name_.c_str(), ":", type_signature_.c_str() };
  } else {
    DCHECK_EQ(type_, kMethod);
    return { class_name_.c_str(), "->", member_name_.c_str(), type_signature_.c_str() };
  }
}

bool MemberSignature::DoesPrefixMatch(const std::string& prefix) const {
  size_t pos = 0;
  for (const char* part : GetSignatureParts()) {
    size_t count = std::min(prefix.length() - pos, strlen(part));
    if (prefix.compare(pos, count, part, 0, count) == 0) {
      pos += count;
    } else {
      return false;
    }
  }
  // We have a complete match if all parts match (we exit the loop without
  // returning) AND we've matched the whole prefix.
  return pos == prefix.length();
}

bool MemberSignature::IsExempted(const std::vector<std::string>& exemptions) {
  for (const std::string& exemption : exemptions) {
    if (DoesPrefixMatch(exemption)) {
      return true;
    }
  }
  return false;
}

void MemberSignature::Dump(std::ostream& os) const {
  for (const char* part : GetSignatureParts()) {
    os << part;
  }
}

void MemberSignature::WarnAboutAccess(AccessMethod access_method,
                                      HiddenApiAccessFlags::ApiList list) {
  LOG(WARNING) << "Accessing hidden " << (type_ == kField ? "field " : "method ")
               << Dumpable<MemberSignature>(*this) << " (" << list << ", " << access_method << ")";
}
// Convert an AccessMethod enum to a value for logging from the proto enum.
// This method may look odd (the enum values are current the same), but it
// prevents coupling the internal enum to the proto enum (which should never
// be changed) so that we are free to change the internal one if necessary in
// future.
inline static int32_t GetEnumValueForLog(AccessMethod access_method) {
  switch (access_method) {
    case kNone:
      return android::metricslogger::ACCESS_METHOD_NONE;
    case kReflection:
      return android::metricslogger::ACCESS_METHOD_REFLECTION;
    case kJNI:
      return android::metricslogger::ACCESS_METHOD_JNI;
    case kLinking:
      return android::metricslogger::ACCESS_METHOD_LINKING;
    default:
      DCHECK(false);
  }
}

void MemberSignature::LogAccessToEventLog(AccessMethod access_method, Action action_taken) {
  if (access_method == kLinking || access_method == kNone) {
    // Linking warnings come from static analysis/compilation of the bytecode
    // and can contain false positives (i.e. code that is never run). We choose
    // not to log these in the event log.
    // None does not correspond to actual access, so should also be ignored.
    return;
  }
  ComplexEventLogger log_maker(ACTION_HIDDEN_API_ACCESSED);
  log_maker.AddTaggedData(FIELD_HIDDEN_API_ACCESS_METHOD, GetEnumValueForLog(access_method));
  if (action_taken == kDeny) {
    log_maker.AddTaggedData(FIELD_HIDDEN_API_ACCESS_DENIED, 1);
  }
  const std::string& package_name = Runtime::Current()->GetProcessPackageName();
  if (!package_name.empty()) {
    log_maker.SetPackageName(package_name);
  }
  std::ostringstream signature_str;
  Dump(signature_str);
  log_maker.AddTaggedData(FIELD_HIDDEN_API_SIGNATURE, signature_str.str());
  log_maker.Record();
}

static ALWAYS_INLINE bool CanUpdateMemberAccessFlags(ArtField*) {
  return true;
}

static ALWAYS_INLINE bool CanUpdateMemberAccessFlags(ArtMethod* method) {
  return !method->IsIntrinsic();
}

template<typename T>
static ALWAYS_INLINE void MaybeWhitelistMember(Runtime* runtime, T* member)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (CanUpdateMemberAccessFlags(member) && runtime->ShouldDedupeHiddenApiWarnings()) {
    member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
        member->GetAccessFlags(), HiddenApiAccessFlags::kWhitelist));
  }
}

template<typename T>
Action GetMemberActionImpl(T* member,
                           HiddenApiAccessFlags::ApiList api_list,
                           Action action,
                           AccessMethod access_method) {
  DCHECK_NE(action, kAllow);

  // Get the signature, we need it later.
  MemberSignature member_signature(member);

  Runtime* runtime = Runtime::Current();

  // Check for an exemption first. Exempted APIs are treated as white list.
  // We only do this if we're about to deny, or if the app is debuggable. This is because:
  // - we only print a warning for light greylist violations for debuggable apps
  // - for non-debuggable apps, there is no distinction between light grey & whitelisted APIs.
  // - we want to avoid the overhead of checking for exemptions for light greylisted APIs whenever
  //   possible.
  const bool shouldWarn = kLogAllAccesses || runtime->IsJavaDebuggable();
  if (shouldWarn || action == kDeny) {
    if (member_signature.IsExempted(runtime->GetHiddenApiExemptions())) {
      action = kAllow;
      // Avoid re-examining the exemption list next time.
      // Note this results in no warning for the member, which seems like what one would expect.
      // Exemptions effectively adds new members to the whitelist.
      MaybeWhitelistMember(runtime, member);
      return kAllow;
    }

    if (access_method != kNone) {
      // Print a log message with information about this class member access.
      // We do this if we're about to block access, or the app is debuggable.
      member_signature.WarnAboutAccess(access_method, api_list);
    }
  }

  if (kIsTargetBuild) {
    uint32_t eventLogSampleRate = runtime->GetHiddenApiEventLogSampleRate();
    // Assert that RAND_MAX is big enough, to ensure sampling below works as expected.
    static_assert(RAND_MAX >= 0xffff, "RAND_MAX too small");
    if (eventLogSampleRate != 0 &&
        (static_cast<uint32_t>(std::rand()) & 0xffff) < eventLogSampleRate) {
      member_signature.LogAccessToEventLog(access_method, action);
    }
  }

  if (action == kDeny) {
    // Block access
    return action;
  }

  // Allow access to this member but print a warning.
  DCHECK(action == kAllowButWarn || action == kAllowButWarnAndToast);

  if (access_method != kNone) {
    // Depending on a runtime flag, we might move the member into whitelist and
    // skip the warning the next time the member is accessed.
    MaybeWhitelistMember(runtime, member);

    // If this action requires a UI warning, set the appropriate flag.
    if (shouldWarn &&
        (action == kAllowButWarnAndToast || runtime->ShouldAlwaysSetHiddenApiWarningFlag())) {
      runtime->SetPendingHiddenApiWarning(true);
    }
  }

  return action;
}

// Need to instantiate this.
template Action GetMemberActionImpl<ArtField>(ArtField* member,
                                              HiddenApiAccessFlags::ApiList api_list,
                                              Action action,
                                              AccessMethod access_method);
template Action GetMemberActionImpl<ArtMethod>(ArtMethod* member,
                                               HiddenApiAccessFlags::ApiList api_list,
                                               Action action,
                                               AccessMethod access_method);
}  // namespace detail

template<typename T>
void NotifyHiddenApiListener(T* member) {
  Runtime* runtime = Runtime::Current();
  if (!runtime->IsAotCompiler()) {
    ScopedObjectAccessUnchecked soa(Thread::Current());

    ScopedLocalRef<jobject> consumer_object(soa.Env(),
        soa.Env()->GetStaticObjectField(
            WellKnownClasses::dalvik_system_VMRuntime,
            WellKnownClasses::dalvik_system_VMRuntime_nonSdkApiUsageConsumer));
    // If the consumer is non-null, we call back to it to let it know that we
    // have encountered an API that's in one of our lists.
    if (consumer_object != nullptr) {
      detail::MemberSignature member_signature(member);
      std::ostringstream member_signature_str;
      member_signature.Dump(member_signature_str);

      ScopedLocalRef<jobject> signature_str(
          soa.Env(),
          soa.Env()->NewStringUTF(member_signature_str.str().c_str()));

      // Call through to Consumer.accept(String memberSignature);
      soa.Env()->CallVoidMethod(consumer_object.get(),
                                WellKnownClasses::java_util_function_Consumer_accept,
                                signature_str.get());
    }
  }
}

template void NotifyHiddenApiListener<ArtMethod>(ArtMethod* member);
template void NotifyHiddenApiListener<ArtField>(ArtField* member);

}  // namespace hiddenapi
}  // namespace art
