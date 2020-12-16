/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "indirect_reference_table-inl.h"

#include "base/dumpable-inl.h"
#include "base/systrace.h"
#include "base/utils.h"
#include "java_vm_ext.h"
#include "jni_internal.h"
#include "nth_caller_visitor.h"
#include "reference_table.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

#include <cstdlib>

namespace art {

static constexpr bool kDumpStackOnNonLocalReference = false;
static constexpr bool kDebugIRT = false;

// Maximum table size we allow.
static constexpr size_t kMaxTableSizeInBytes = 128 * MB;

const char* GetIndirectRefKindString(const IndirectRefKind& kind) {
  switch (kind) {
    case kHandleScopeOrInvalid:
      return "HandleScopeOrInvalid";
    case kLocal:
      return "Local";
    case kGlobal:
      return "Global";
    case kWeakGlobal:
      return "WeakGlobal";
  }
  return "IndirectRefKind Error";
}

void IndirectReferenceTable::AbortIfNoCheckJNI(const std::string& msg) {
  // If -Xcheck:jni is on, it'll give a more detailed error before aborting.
  JavaVMExt* vm = Runtime::Current()->GetJavaVM();
  if (!vm->IsCheckJniEnabled()) {
    // Otherwise, we want to abort rather than hand back a bad reference.
    LOG(FATAL) << msg;
  } else {
    LOG(ERROR) << msg;
  }
}

IndirectReferenceTable::IndirectReferenceTable(size_t max_count,
                                               IndirectRefKind desired_kind,
                                               ResizableCapacity resizable,
                                               std::string* error_msg)
    : segment_state_(kIRTFirstSegment),
      kind_(desired_kind),
      max_entries_(max_count),
      current_num_holes_(0),
      resizable_(resizable) {
  CHECK(error_msg != nullptr);
  CHECK_NE(desired_kind, kHandleScopeOrInvalid);

  // Overflow and maximum check.
  CHECK_LE(max_count, kMaxTableSizeInBytes / sizeof(IrtEntry));

  const size_t table_bytes = max_count * sizeof(IrtEntry);
  table_mem_map_.reset(MemMap::MapAnonymous("indirect ref table", nullptr, table_bytes,
                                            PROT_READ | PROT_WRITE, false, false, error_msg));
  if (table_mem_map_.get() == nullptr && error_msg->empty()) {
    *error_msg = "Unable to map memory for indirect ref table";
  }

  if (table_mem_map_.get() != nullptr) {
    table_ = reinterpret_cast<IrtEntry*>(table_mem_map_->Begin());
  } else {
    table_ = nullptr;
  }
  segment_state_ = kIRTFirstSegment;
  last_known_previous_state_ = kIRTFirstSegment;
}

IndirectReferenceTable::~IndirectReferenceTable() {
}

void IndirectReferenceTable::ConstexprChecks() {
  // Use this for some assertions. They can't be put into the header as C++ wants the class
  // to be complete.

  // Check kind.
  static_assert((EncodeIndirectRefKind(kLocal) & (~kKindMask)) == 0, "Kind encoding error");
  static_assert((EncodeIndirectRefKind(kGlobal) & (~kKindMask)) == 0, "Kind encoding error");
  static_assert((EncodeIndirectRefKind(kWeakGlobal) & (~kKindMask)) == 0, "Kind encoding error");
  static_assert(DecodeIndirectRefKind(EncodeIndirectRefKind(kLocal)) == kLocal,
                "Kind encoding error");
  static_assert(DecodeIndirectRefKind(EncodeIndirectRefKind(kGlobal)) == kGlobal,
                "Kind encoding error");
  static_assert(DecodeIndirectRefKind(EncodeIndirectRefKind(kWeakGlobal)) == kWeakGlobal,
                "Kind encoding error");

  // Check serial.
  static_assert(DecodeSerial(EncodeSerial(0u)) == 0u, "Serial encoding error");
  static_assert(DecodeSerial(EncodeSerial(1u)) == 1u, "Serial encoding error");
  static_assert(DecodeSerial(EncodeSerial(2u)) == 2u, "Serial encoding error");
  static_assert(DecodeSerial(EncodeSerial(3u)) == 3u, "Serial encoding error");

  // Table index.
  static_assert(DecodeIndex(EncodeIndex(0u)) == 0u, "Index encoding error");
  static_assert(DecodeIndex(EncodeIndex(1u)) == 1u, "Index encoding error");
  static_assert(DecodeIndex(EncodeIndex(2u)) == 2u, "Index encoding error");
  static_assert(DecodeIndex(EncodeIndex(3u)) == 3u, "Index encoding error");
}

bool IndirectReferenceTable::IsValid() const {
  return table_mem_map_.get() != nullptr;
}

// Holes:
//
// To keep the IRT compact, we want to fill "holes" created by non-stack-discipline Add & Remove
// operation sequences. For simplicity and lower memory overhead, we do not use a free list or
// similar. Instead, we scan for holes, with the expectation that we will find holes fast as they
// are usually near the end of the table (see the header, TODO: verify this assumption). To avoid
// scans when there are no holes, the number of known holes should be tracked.
//
// A previous implementation stored the top index and the number of holes as the segment state.
// This constraints the maximum number of references to 16-bit. We want to relax this, as it
// is easy to require more references (e.g., to list all classes in large applications). Thus,
// the implicitly stack-stored state, the IRTSegmentState, is only the top index.
//
// Thus, hole count is a local property of the current segment, and needs to be recovered when
// (or after) a frame is pushed or popped. To keep JNI transitions simple (and inlineable), we
// cannot do work when the segment changes. Thus, Add and Remove need to ensure the current
// hole count is correct.
//
// To be able to detect segment changes, we require an additional local field that can describe
// the known segment. This is last_known_previous_state_. The requirement will become clear with
// the following (some non-trivial) cases that have to be supported:
//
// 1) Segment with holes (current_num_holes_ > 0), push new segment, add/remove reference
// 2) Segment with holes (current_num_holes_ > 0), pop segment, add/remove reference
// 3) Segment with holes (current_num_holes_ > 0), push new segment, pop segment, add/remove
//    reference
// 4) Empty segment, push new segment, create a hole, pop a segment, add/remove a reference
// 5) Base segment, push new segment, create a hole, pop a segment, push new segment, add/remove
//    reference
//
// Storing the last known *previous* state (bottom index) allows conservatively detecting all the
// segment changes above. The condition is simply that the last known state is greater than or
// equal to the current previous state, and smaller than the current state (top index). The
// condition is conservative as it adds O(1) overhead to operations on an empty segment.

static size_t CountNullEntries(const IrtEntry* table, size_t from, size_t to) {
  size_t count = 0;
  for (size_t index = from; index != to; ++index) {
    if (table[index].GetReference()->IsNull()) {
      count++;
    }
  }
  return count;
}

void IndirectReferenceTable::RecoverHoles(IRTSegmentState prev_state) {
  if (last_known_previous_state_.top_index >= segment_state_.top_index ||
      last_known_previous_state_.top_index < prev_state.top_index) {
    const size_t top_index = segment_state_.top_index;
    size_t count = CountNullEntries(table_, prev_state.top_index, top_index);

    if (kDebugIRT) {
      LOG(INFO) << "+++ Recovered holes: "
                << " Current prev=" << prev_state.top_index
                << " Current top_index=" << top_index
                << " Old num_holes=" << current_num_holes_
                << " New num_holes=" << count;
    }

    current_num_holes_ = count;
    last_known_previous_state_ = prev_state;
  } else if (kDebugIRT) {
    LOG(INFO) << "No need to recover holes";
  }
}

ALWAYS_INLINE
static inline void CheckHoleCount(IrtEntry* table,
                                  size_t exp_num_holes,
                                  IRTSegmentState prev_state,
                                  IRTSegmentState cur_state) {
  if (kIsDebugBuild) {
    size_t count = CountNullEntries(table, prev_state.top_index, cur_state.top_index);
    CHECK_EQ(exp_num_holes, count) << "prevState=" << prev_state.top_index
                                   << " topIndex=" << cur_state.top_index;
  }
}

bool IndirectReferenceTable::Resize(size_t new_size, std::string* error_msg) {
  CHECK_GT(new_size, max_entries_);

  constexpr size_t kMaxEntries = kMaxTableSizeInBytes / sizeof(IrtEntry);
  if (new_size > kMaxEntries) {
    *error_msg = android::base::StringPrintf("Requested size exceeds maximum: %zu", new_size);
    return false;
  }
  // Note: the above check also ensures that there is no overflow below.

  const size_t table_bytes = new_size * sizeof(IrtEntry);
  std::unique_ptr<MemMap> new_map(MemMap::MapAnonymous("indirect ref table",
                                                       nullptr,
                                                       table_bytes,
                                                       PROT_READ | PROT_WRITE,
                                                       false,
                                                       false,
                                                       error_msg));
  if (new_map == nullptr) {
    return false;
  }

  memcpy(new_map->Begin(), table_mem_map_->Begin(), table_mem_map_->Size());
  table_mem_map_ = std::move(new_map);
  table_ = reinterpret_cast<IrtEntry*>(table_mem_map_->Begin());
  max_entries_ = new_size;

  return true;
}

IndirectRef IndirectReferenceTable::Add(IRTSegmentState previous_state,
                                        ObjPtr<mirror::Object> obj,
                                        std::string* error_msg) {
  if (kDebugIRT) {
    LOG(INFO) << "+++ Add: previous_state=" << previous_state.top_index
              << " top_index=" << segment_state_.top_index
              << " last_known_prev_top_index=" << last_known_previous_state_.top_index
              << " holes=" << current_num_holes_;
  }

  size_t top_index = segment_state_.top_index;

  CHECK(obj != nullptr);
  VerifyObject(obj);
  DCHECK(table_ != nullptr);

  if (top_index == max_entries_) {
    if (resizable_ == ResizableCapacity::kNo) {
      std::ostringstream oss;
      oss << "JNI ERROR (app bug): " << kind_ << " table overflow "
          << "(max=" << max_entries_ << ")"
          << MutatorLockedDumpable<IndirectReferenceTable>(*this);
      *error_msg = oss.str();
      return nullptr;
    }

    // Try to double space.
    if (std::numeric_limits<size_t>::max() / 2 < max_entries_) {
      std::ostringstream oss;
      oss << "JNI ERROR (app bug): " << kind_ << " table overflow "
          << "(max=" << max_entries_ << ")" << std::endl
          << MutatorLockedDumpable<IndirectReferenceTable>(*this)
          << " Resizing failed: exceeds size_t";
      *error_msg = oss.str();
      return nullptr;
    }

    std::string inner_error_msg;
    if (!Resize(max_entries_ * 2, &inner_error_msg)) {
      std::ostringstream oss;
      oss << "JNI ERROR (app bug): " << kind_ << " table overflow "
          << "(max=" << max_entries_ << ")" << std::endl
          << MutatorLockedDumpable<IndirectReferenceTable>(*this)
          << " Resizing failed: " << inner_error_msg;
      *error_msg = oss.str();
      return nullptr;
    }
  }

  RecoverHoles(previous_state);
  CheckHoleCount(table_, current_num_holes_, previous_state, segment_state_);

  // We know there's enough room in the table.  Now we just need to find
  // the right spot.  If there's a hole, find it and fill it; otherwise,
  // add to the end of the list.
  IndirectRef result;
  size_t index;
  if (current_num_holes_ > 0) {
    DCHECK_GT(top_index, 1U);
    // Find the first hole; likely to be near the end of the list.
    IrtEntry* p_scan = &table_[top_index - 1];
    DCHECK(!p_scan->GetReference()->IsNull());
    --p_scan;
    while (!p_scan->GetReference()->IsNull()) {
      DCHECK_GE(p_scan, table_ + previous_state.top_index);
      --p_scan;
    }
    index = p_scan - table_;
    current_num_holes_--;
  } else {
    // Add to the end.
    index = top_index++;
    segment_state_.top_index = top_index;
  }
  table_[index].Add(obj);
  result = ToIndirectRef(index);
  if (kDebugIRT) {
    LOG(INFO) << "+++ added at " << ExtractIndex(result) << " top=" << segment_state_.top_index
              << " holes=" << current_num_holes_;
  }

  DCHECK(result != nullptr);
  return result;
}

void IndirectReferenceTable::AssertEmpty() {
  for (size_t i = 0; i < Capacity(); ++i) {
    if (!table_[i].GetReference()->IsNull()) {
      LOG(FATAL) << "Internal Error: non-empty local reference table\n"
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this);
      UNREACHABLE();
    }
  }
}

// Removes an object. We extract the table offset bits from "iref"
// and zap the corresponding entry, leaving a hole if it's not at the top.
// If the entry is not between the current top index and the bottom index
// specified by the cookie, we don't remove anything. This is the behavior
// required by JNI's DeleteLocalRef function.
// This method is not called when a local frame is popped; this is only used
// for explicit single removals.
// Returns "false" if nothing was removed.
bool IndirectReferenceTable::Remove(IRTSegmentState previous_state, IndirectRef iref) {
  if (kDebugIRT) {
    LOG(INFO) << "+++ Remove: previous_state=" << previous_state.top_index
              << " top_index=" << segment_state_.top_index
              << " last_known_prev_top_index=" << last_known_previous_state_.top_index
              << " holes=" << current_num_holes_;
  }

  const uint32_t top_index = segment_state_.top_index;
  const uint32_t bottom_index = previous_state.top_index;

  DCHECK(table_ != nullptr);

  if (GetIndirectRefKind(iref) == kHandleScopeOrInvalid) {
    auto* self = Thread::Current();
    if (self->HandleScopeContains(reinterpret_cast<jobject>(iref))) {
      auto* env = self->GetJniEnv();
      DCHECK(env != nullptr);
      if (env->IsCheckJniEnabled()) {
        ScopedObjectAccess soa(self);
        LOG(WARNING) << "Attempt to remove non-JNI local reference, dumping thread";
        if (kDumpStackOnNonLocalReference) {
          self->Dump(LOG_STREAM(WARNING));
        }
      }
      return true;
    }
  }
  const uint32_t idx = ExtractIndex(iref);
  if (idx < bottom_index) {
    // Wrong segment.
    LOG(WARNING) << "Attempt to remove index outside index area (" << idx
                 << " vs " << bottom_index << "-" << top_index << ")";
    return false;
  }
  if (idx >= top_index) {
    // Bad --- stale reference?
    LOG(WARNING) << "Attempt to remove invalid index " << idx
                 << " (bottom=" << bottom_index << " top=" << top_index << ")";
    return false;
  }

  RecoverHoles(previous_state);
  CheckHoleCount(table_, current_num_holes_, previous_state, segment_state_);

  if (idx == top_index - 1) {
    // Top-most entry.  Scan up and consume holes.

    if (!CheckEntry("remove", iref, idx)) {
      return false;
    }

    *table_[idx].GetReference() = GcRoot<mirror::Object>(nullptr);
    if (current_num_holes_ != 0) {
      uint32_t collapse_top_index = top_index;
      while (--collapse_top_index > bottom_index && current_num_holes_ != 0) {
        if (kDebugIRT) {
          ScopedObjectAccess soa(Thread::Current());
          LOG(INFO) << "+++ checking for hole at " << collapse_top_index - 1
                    << " (previous_state=" << bottom_index << ") val="
                    << table_[collapse_top_index - 1].GetReference()->Read<kWithoutReadBarrier>();
        }
        if (!table_[collapse_top_index - 1].GetReference()->IsNull()) {
          break;
        }
        if (kDebugIRT) {
          LOG(INFO) << "+++ ate hole at " << (collapse_top_index - 1);
        }
        current_num_holes_--;
      }
      segment_state_.top_index = collapse_top_index;

      CheckHoleCount(table_, current_num_holes_, previous_state, segment_state_);
    } else {
      segment_state_.top_index = top_index - 1;
      if (kDebugIRT) {
        LOG(INFO) << "+++ ate last entry " << top_index - 1;
      }
    }
  } else {
    // Not the top-most entry.  This creates a hole.  We null out the entry to prevent somebody
    // from deleting it twice and screwing up the hole count.
    if (table_[idx].GetReference()->IsNull()) {
      LOG(INFO) << "--- WEIRD: removing null entry " << idx;
      return false;
    }
    if (!CheckEntry("remove", iref, idx)) {
      return false;
    }

    *table_[idx].GetReference() = GcRoot<mirror::Object>(nullptr);
    current_num_holes_++;
    CheckHoleCount(table_, current_num_holes_, previous_state, segment_state_);
    if (kDebugIRT) {
      LOG(INFO) << "+++ left hole at " << idx << ", holes=" << current_num_holes_;
    }
  }

  return true;
}

void IndirectReferenceTable::Trim() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  const size_t top_index = Capacity();
  auto* release_start = AlignUp(reinterpret_cast<uint8_t*>(&table_[top_index]), kPageSize);
  uint8_t* release_end = table_mem_map_->End();
  madvise(release_start, release_end - release_start, MADV_DONTNEED);
}

void IndirectReferenceTable::VisitRoots(RootVisitor* visitor, const RootInfo& root_info) {
  BufferedRootVisitor<kDefaultBufferedRootCount> root_visitor(visitor, root_info);
  for (auto ref : *this) {
    if (!ref->IsNull()) {
      root_visitor.VisitRoot(*ref);
      DCHECK(!ref->IsNull());
    }
  }
}

void IndirectReferenceTable::Dump(std::ostream& os) const {
  os << kind_ << " table dump:\n";
  ReferenceTable::Table entries;
  for (size_t i = 0; i < Capacity(); ++i) {
    ObjPtr<mirror::Object> obj = table_[i].GetReference()->Read<kWithoutReadBarrier>();
    if (obj != nullptr) {
      obj = table_[i].GetReference()->Read();
      entries.push_back(GcRoot<mirror::Object>(obj));
    }
  }
  ReferenceTable::Dump(os, entries);
}

void IndirectReferenceTable::SetSegmentState(IRTSegmentState new_state) {
  if (kDebugIRT) {
    LOG(INFO) << "Setting segment state: "
              << segment_state_.top_index
              << " -> "
              << new_state.top_index;
  }
  segment_state_ = new_state;
}

bool IndirectReferenceTable::EnsureFreeCapacity(size_t free_capacity, std::string* error_msg) {
  size_t top_index = segment_state_.top_index;
  if (top_index < max_entries_ && top_index + free_capacity <= max_entries_) {
    return true;
  }

  // We're only gonna do a simple best-effort here, ensuring the asked-for capacity at the end.
  if (resizable_ == ResizableCapacity::kNo) {
    *error_msg = "Table is not resizable";
    return false;
  }

  // Try to increase the table size.

  // Would this overflow?
  if (std::numeric_limits<size_t>::max() - free_capacity < top_index) {
    *error_msg = "Cannot resize table, overflow.";
    return false;
  }

  if (!Resize(top_index + free_capacity, error_msg)) {
    LOG(WARNING) << "JNI ERROR: Unable to reserve space in EnsureFreeCapacity (" << free_capacity
                 << "): " << std::endl
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this)
                 << " Resizing failed: " << *error_msg;
    return false;
  }
  return true;
}

size_t IndirectReferenceTable::FreeCapacity() const {
  return max_entries_ - segment_state_.top_index;
}

}  // namespace art
