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

#include "subtype_check.h"

#include "gtest/gtest.h"
#include "android-base/logging.h"

namespace art {

constexpr size_t BitString::kBitSizeAtPosition[BitString::kCapacity];
constexpr size_t BitString::kCapacity;

struct MockClass {
  explicit MockClass(MockClass* parent, size_t x = 0, size_t y = 0) {
    parent_ = parent;
    memset(&subtype_check_info_and_status_, 0u, sizeof(subtype_check_info_and_status_));

    // Start the numbering at '1' to match the bitstring numbering.
    // A bitstring numbering never starts at '0' which just means 'no value'.
    x_ = 1;
    if (parent_ != nullptr) {
      if (parent_->GetMaxChild() != nullptr) {
        x_ = parent_->GetMaxChild()->x_ + 1u;
      }

      parent_->children_.push_back(this);
      if (parent_->path_to_root_ != "") {
        path_to_root_ = parent->path_to_root_ + ",";
      }
      path_to_root_ += std::to_string(x_);
    } else {
      path_to_root_ = "";  // root has no path.
    }
    y_ = y;
    UNUSED(x);
  }

  MockClass() : MockClass(nullptr) {
  }

  ///////////////////////////////////////////////////////////////
  // Implementation of the SubtypeCheck::KlassT static interface.
  ///////////////////////////////////////////////////////////////

  MockClass* GetSuperClass() const {
    return parent_;
  }

  bool HasSuperClass() const {
    return GetSuperClass() != nullptr;
  }

  size_t Depth() const {
    if (parent_ == nullptr) {
      return 0u;
    } else {
      return parent_->Depth() + 1u;
    }
  }

  std::string PrettyClass() const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return path_to_root_;
  }

  int32_t GetField32Volatile(art::MemberOffset offset = art::MemberOffset(0u)) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    UNUSED(offset);
    int32_t field_32 = 0;
    memcpy(&field_32, &subtype_check_info_and_status_, sizeof(int32_t));
    return field_32;
  }

  template <bool kTransactionActive>
  bool CasFieldWeakSequentiallyConsistent32(art::MemberOffset offset,
                                            int32_t old_value,
                                            int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    UNUSED(offset);
    if (old_value == GetField32Volatile(offset)) {
      memcpy(&subtype_check_info_and_status_, &new_value, sizeof(int32_t));
      return true;
    }
    return false;
  }

  MemberOffset StatusOffset() const {
    return MemberOffset(0);  // Doesn't matter. We ignore offset.
  }

  ///////////////////////////////////////////////////////////////
  // Convenience functions to make the testing easier
  ///////////////////////////////////////////////////////////////

  size_t GetNumberOfChildren() const {
    return children_.size();
  }

  MockClass* GetParent() const {
    return parent_;
  }

  MockClass* GetMaxChild() const {
    if (GetNumberOfChildren() > 0u) {
      return GetChild(GetNumberOfChildren() - 1);
    }
    return nullptr;
  }

  MockClass* GetChild(size_t idx) const {
    if (idx >= GetNumberOfChildren()) {
      return nullptr;
    }
    return children_[idx];
  }

  // Traverse the sibling at "X" at each level.
  // Once we get to level==depth, return yourself.
  MockClass* FindChildAt(size_t x, size_t depth) {
    if (Depth() == depth) {
      return this;
    } else if (GetNumberOfChildren() > 0) {
      return GetChild(x)->FindChildAt(x, depth);
    }
    return nullptr;
  }

  template <typename T>
  MockClass* Visit(T visitor, bool recursive = true) {
    if (!visitor(this)) {
      return this;
    }

    if (!recursive) {
      return this;
    }

    for (MockClass* child : children_) {
      MockClass* visit_res = child->Visit(visitor);
      if (visit_res != nullptr) {
        return visit_res;
      }
    }

    return nullptr;
  }

  size_t GetX() const {
    return x_;
  }

  bool SlowIsSubtypeOf(const MockClass* target) const {
    DCHECK(target != nullptr);
    const MockClass* kls = this;
    while (kls != nullptr) {
      if (kls == target) {
        return true;
      }
      kls = kls->GetSuperClass();
    }

    return false;
  }

  std::string ToDotGraph() const {
    std::stringstream ss;
    ss << std::endl;
    ss << "digraph MockClass {" << std::endl;
    ss << "    node [fontname=\"Arial\"];" << std::endl;
    ToDotGraphImpl(ss);
    ss << "}" << std::endl;
    return ss.str();
  }

  void ToDotGraphImpl(std::ostream& os) const {
    for (MockClass* child : children_) {
      os << "    '" << path_to_root_ << "' -> '" << child->path_to_root_ << "';" << std::endl;
      child->ToDotGraphImpl(os);
    }
  }

  std::vector<MockClass*> children_;
  MockClass* parent_;
  SubtypeCheckBitsAndStatus subtype_check_info_and_status_;
  size_t x_;
  size_t y_;
  std::string path_to_root_;
};

std::ostream& operator<<(std::ostream& os, const MockClass& kls) {
  SubtypeCheckBits iod = kls.subtype_check_info_and_status_.subtype_check_info_;
  os << "MClass{D:" << kls.Depth() << ",W:" << kls.x_
     << ", OF:"
     << (iod.overflow_ ? "true" : "false")
     << ", bitstring: " << iod.bitstring_
     << ", mock_path: " << kls.path_to_root_
     << "}";
  return os;
}

struct MockSubtypeCheck {
  using SC = SubtypeCheck<MockClass*>;

  static MockSubtypeCheck Lookup(MockClass* klass) {
    MockSubtypeCheck mock;
    mock.klass_ = klass;
    return mock;
  }

  // Convenience functions to avoid using statics everywhere.
  //    static(class, args...) -> instance.method(args...)
  SubtypeCheckInfo::State EnsureInitialized()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::EnsureInitialized(klass_);
  }

  SubtypeCheckInfo::State EnsureAssigned()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::EnsureAssigned(klass_);
  }

  SubtypeCheckInfo::State ForceUninitialize()
    REQUIRES(Locks::subtype_check_lock_)
    REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::ForceUninitialize(klass_);
  }

  BitString::StorageType GetEncodedPathToRootForSource() const
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::GetEncodedPathToRootForSource(klass_);
  }

  BitString::StorageType GetEncodedPathToRootForTarget() const
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::GetEncodedPathToRootForTarget(klass_);
  }

  BitString::StorageType GetEncodedPathToRootMask() const
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::GetEncodedPathToRootMask(klass_);
  }

  SubtypeCheckInfo::Result IsSubtypeOf(const MockSubtypeCheck& target)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::IsSubtypeOf(klass_, target.klass_);
  }

  friend std::ostream& operator<<(std::ostream& os, const MockSubtypeCheck& tree)
      NO_THREAD_SAFETY_ANALYSIS {
    os << "(MockSubtypeCheck io:";
    SC::Dump(tree.klass_, os);
    os << ", class: " << tree.klass_->PrettyClass() << ")";
    return os;
  }

  // Additional convenience functions.
  SubtypeCheckInfo::State GetState() const
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return SC::GetSubtypeCheckInfo(klass_).GetState();
  }

  MockClass& GetClass() const {
    return *klass_;
  }

 private:
  MockClass* klass_;
};

struct MockScopedLockSubtypeCheck {
  MockScopedLockSubtypeCheck() ACQUIRE(*Locks::subtype_check_lock_) {}
  ~MockScopedLockSubtypeCheck() RELEASE(*Locks::subtype_check_lock_) {}
};

struct MockScopedLockMutator {
  MockScopedLockMutator() ACQUIRE_SHARED(*Locks::mutator_lock_) {}
  ~MockScopedLockMutator() RELEASE_SHARED(*Locks::mutator_lock_) {}
};

struct SubtypeCheckTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    android::base::InitLogging(/*argv*/nullptr);

    CreateRootedTree(BitString::kCapacity + 2u, BitString::kCapacity + 2u);
  }

  virtual void TearDown() {
  }

  void CreateRootedTree(size_t width, size_t height) {
    all_classes_.clear();
    root_ = CreateClassFor(/*parent*/nullptr, /*x*/0, /*y*/0);
    CreateTreeFor(root_, /*width*/width, /*depth*/height);
  }

  MockClass* CreateClassFor(MockClass* parent, size_t x, size_t y) {
    MockClass* kls = new MockClass(parent, x, y);
    all_classes_.push_back(std::unique_ptr<MockClass>(kls));
    return kls;
  }

  void CreateTreeFor(MockClass* parent, size_t width, size_t levels) {
    DCHECK(parent != nullptr);
    if (levels == 0) {
      return;
    }

    for (size_t i = 0; i < width; ++i) {
      MockClass* child = CreateClassFor(parent, i, parent->y_ + 1);
      CreateTreeFor(child, width, levels - 1);
    }
  }

  MockClass* root_ = nullptr;
  std::vector<std::unique_ptr<MockClass>> all_classes_;
};

TEST_F(SubtypeCheckTest, LookupAllChildren) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  root_->Visit([&](MockClass* kls) {
    MockScopedLockSubtypeCheck lock_a;
    MockScopedLockMutator lock_b;

    EXPECT_EQ(SubtypeCheckInfo::kUninitialized, SCTree::Lookup(kls).GetState());
    return true;  // Keep visiting.
  });
}

TEST_F(SubtypeCheckTest, LookupRoot) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree root = SCTree::Lookup(root_);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, root.EnsureInitialized());
  EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, root.IsSubtypeOf(root)) << root;
}

TEST_F(SubtypeCheckTest, EnsureInitializedFirstLevel) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree root = SCTree::Lookup(root_);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, root.EnsureInitialized());

  ASSERT_LT(0u, root_->GetNumberOfChildren());

  // Initialize root's children only.
  for (size_t i = 0; i < root_->GetNumberOfChildren(); ++i) {
    MockClass* child = root_->GetChild(i);
    SCTree child_tree = SCTree::Lookup(child);
    // Before: all unknown.
    EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child_tree)) << child_tree;
    EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(root)) << child_tree;
    // Transition.
    EXPECT_EQ(SubtypeCheckInfo::kInitialized, child_tree.EnsureInitialized());
    // After: "src instanceof target" known, but "target instanceof src" unknown.
    EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child_tree.IsSubtypeOf(root)) << child_tree;
    EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child_tree)) << child_tree;
  }
}

TEST_F(SubtypeCheckTest, EnsureAssignedFirstLevel) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree root = SCTree::Lookup(root_);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, root.EnsureInitialized());

  ASSERT_LT(0u, root_->GetNumberOfChildren());

  // Initialize root's children only.
  for (size_t i = 0; i < root_->GetNumberOfChildren(); ++i) {
    MockClass* child = root_->GetChild(i);
    SCTree child_tree = SCTree::Lookup(child);
    // Before: all unknown.
    EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child_tree)) << child_tree;
    EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(root)) << child_tree;
    // Transition.
    EXPECT_EQ(SubtypeCheckInfo::kAssigned, child_tree.EnsureAssigned());
    // After: "src instanceof target" known, and "target instanceof src" known.
    EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child_tree.IsSubtypeOf(root)) << child_tree;
    EXPECT_EQ(SubtypeCheckInfo::kNotSubtypeOf, root.IsSubtypeOf(child_tree)) << child_tree;
  }
}

TEST_F(SubtypeCheckTest, EnsureInitializedSecondLevelWithPreassign) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree root = SCTree::Lookup(root_);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, root.EnsureInitialized());

  ASSERT_LT(0u, root_->GetNumberOfChildren());

  // Initialize root's children.
  for (size_t i = 0; i < root_->GetNumberOfChildren(); ++i) {
    MockClass* child = root_->GetChild(i);
    SCTree child_tree = SCTree::Lookup(child);

    ASSERT_EQ(1u, child->Depth());

    EXPECT_EQ(SubtypeCheckInfo::kInitialized, child_tree.EnsureInitialized()) << *child;
    EXPECT_EQ(SubtypeCheckInfo::kAssigned, child_tree.EnsureAssigned())
              << *child << ", root:" << *root_;
    for (size_t j = 0; j < child->GetNumberOfChildren(); ++j) {
      MockClass* child2 = child->GetChild(j);
      ASSERT_EQ(2u, child2->Depth());
      SCTree child2_tree = SCTree::Lookup(child2);

      // Before: all unknown.
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child2_tree)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(child2_tree))
                << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child2_tree.IsSubtypeOf(root))
                << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child2_tree.IsSubtypeOf(child_tree))
                << child2_tree;

      EXPECT_EQ(SubtypeCheckInfo::kUninitialized, child2_tree.GetState()) << *child2;
      EXPECT_EQ(SubtypeCheckInfo::kInitialized, child2_tree.EnsureInitialized()) << *child2;

      // After: src=child2_tree is known, otherwise unknown.
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child2_tree)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(child2_tree))
                << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child2_tree.IsSubtypeOf(root)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child2_tree.IsSubtypeOf(child_tree)) << child2_tree;
    }

    // The child is "assigned" as a side-effect of initializing sub-children.
    EXPECT_EQ(SubtypeCheckInfo::kAssigned, child_tree.GetState());
  }
}

TEST_F(SubtypeCheckTest, EnsureInitializedSecondLevelDontPreassign) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree root = SCTree::Lookup(root_);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, root.EnsureInitialized());

  ASSERT_LT(0u, root_->GetNumberOfChildren());

  // Initialize root's children only.
  for (size_t i = 0; i < root_->GetNumberOfChildren(); ++i) {
    MockClass* child = root_->GetChild(i);
    SCTree child_tree = SCTree::Lookup(child);

    ASSERT_EQ(1u, child->Depth());

    for (size_t j = 0; j < child->GetNumberOfChildren(); ++j) {
      MockClass* child2 = child->GetChild(j);
      ASSERT_EQ(2u, child2->Depth());
      SCTree child2_tree = SCTree::Lookup(child2);
      // Before: all unknown.
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child2_tree)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(child2_tree))
                << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child2_tree.IsSubtypeOf(root)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child2_tree.IsSubtypeOf(child_tree))
                << child2_tree;
      // Transition.
      EXPECT_EQ(SubtypeCheckInfo::kUninitialized, child2_tree.GetState()) << *child2;
      EXPECT_EQ(SubtypeCheckInfo::kInitialized, child2_tree.EnsureInitialized()) << *child2;
      // After: src=child2_tree is known, otherwise unknown.
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, root.IsSubtypeOf(child2_tree)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, child_tree.IsSubtypeOf(child2_tree))
                << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child2_tree.IsSubtypeOf(root)) << child2_tree;
      EXPECT_EQ(SubtypeCheckInfo::kSubtypeOf, child2_tree.IsSubtypeOf(child_tree)) << child2_tree;
    }

    // The child is "assigned" as a side-effect of initializing sub-children.
    EXPECT_EQ(SubtypeCheckInfo::kAssigned, child_tree.GetState());
  }
}

void ApplyTransition(MockSubtypeCheck sc_tree,
                     SubtypeCheckInfo::State transition,
                     SubtypeCheckInfo::State expected) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;

  EXPECT_EQ(SubtypeCheckInfo::kUninitialized, sc_tree.GetState()) << sc_tree.GetClass();

  if (transition == SubtypeCheckInfo::kUninitialized) {
    EXPECT_EQ(expected, sc_tree.ForceUninitialize()) << sc_tree.GetClass();
  } else if (transition == SubtypeCheckInfo::kInitialized) {
    EXPECT_EQ(expected, sc_tree.EnsureInitialized()) << sc_tree.GetClass();
  } else if (transition == SubtypeCheckInfo::kAssigned) {
    EXPECT_EQ(expected, sc_tree.EnsureAssigned()) << sc_tree.GetClass();
  }
}

enum MockSubtypeOfTransition {
  kNone,
  kUninitialized,
  kInitialized,
  kAssigned,
};

std::ostream& operator<<(std::ostream& os, const MockSubtypeOfTransition& transition) {
  if (transition == MockSubtypeOfTransition::kUninitialized) {
    os << "kUninitialized";
  } else if (transition == MockSubtypeOfTransition::kInitialized) {
    os << "kInitialized";
  } else if (transition == MockSubtypeOfTransition::kAssigned) {
    os << "kAssigned";
  } else {
    os << "kNone";
  }
  return os;
}

SubtypeCheckInfo::State ApplyTransition(MockSubtypeCheck sc_tree,
                                        MockSubtypeOfTransition transition) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;

  if (transition ==  MockSubtypeOfTransition::kUninitialized) {
    return sc_tree.ForceUninitialize();
  } else if (transition == MockSubtypeOfTransition::kInitialized) {
    return sc_tree.EnsureInitialized();
  } else if (transition == MockSubtypeOfTransition::kAssigned) {
    return sc_tree.EnsureAssigned();
  }

  return sc_tree.GetState();
}

enum {
  kBeforeTransition = 0,
  kAfterTransition = 1,
  kAfterChildren = 2,
};

const char* StringifyTransition(int x) {
  if (x == kBeforeTransition) {
    return "kBeforeTransition";
  } else if (x == kAfterTransition) {
    return "kAfterTransition";
  } else if (x == kAfterChildren) {
    return "kAfterChildren";
  }

  return "<<Unknown>>";
}

struct TransitionHistory {
  void Record(int transition_label, MockClass* kls) {
    ss_ << "<<<" << StringifyTransition(transition_label) << ">>>";
    ss_ << "{Self}: " << *kls;

    if (kls->HasSuperClass()) {
      ss_ << "{Parent}: " << *(kls->GetSuperClass());
    }
    ss_ << "================== ";
  }

  friend std::ostream& operator<<(std::ostream& os, const TransitionHistory& t) {
    os << t.ss_.str();
    return os;
  }

  std::stringstream ss_;
};

template <typename T, typename T2>
void EnsureStateChangedTestRecursiveGeneric(MockClass* klass,
                                            size_t cur_depth,
                                            size_t total_depth,
                                            T2 transition_func,
                                            T expect_checks) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  SCTree sc_tree = SCTree::Lookup(klass);
  MockSubtypeOfTransition requested_transition = transition_func(klass);

  // FIXME: need to print before(self, parent) and after(self, parent)
  // to make any sense of what's going on.

  auto do_expect_checks = [&](int transition_label, TransitionHistory& transition_details) {
    MockScopedLockSubtypeCheck lock_a;
    MockScopedLockMutator lock_b;

    transition_details.Record(transition_label, klass);

    SCOPED_TRACE(transition_details);
    ASSERT_EQ(cur_depth, klass->Depth());

    ASSERT_NO_FATAL_FAILURE(expect_checks(klass,
                                          transition_label,
                                          sc_tree.GetState(),
                                          requested_transition));
  };

  TransitionHistory transition_history;
  do_expect_checks(kBeforeTransition, transition_history);
  SubtypeCheckInfo::State state = ApplyTransition(sc_tree, requested_transition);
  UNUSED(state);
  do_expect_checks(kAfterTransition, transition_history);

  if (total_depth == cur_depth) {
    return;
  }

  // Initialize root's children only.
  for (size_t i = 0; i < klass->GetNumberOfChildren(); ++i) {
    MockClass* child = klass->GetChild(i);
    EnsureStateChangedTestRecursiveGeneric(child,
                                           cur_depth + 1u,
                                           total_depth,
                                           transition_func,
                                           expect_checks);
  }

  do_expect_checks(kAfterChildren, transition_history);
}

void EnsureStateChangedTestRecursive(
    MockClass* klass,
    size_t cur_depth,
    size_t total_depth,
    std::vector<std::pair<SubtypeCheckInfo::State, SubtypeCheckInfo::State>> transitions) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  ASSERT_EQ(cur_depth, klass->Depth());
  ApplyTransition(SCTree::Lookup(klass), transitions[cur_depth].first, transitions[cur_depth].second);

  if (total_depth == cur_depth + 1) {
    return;
  }

  // Initialize root's children only.
  for (size_t i = 0; i < klass->GetNumberOfChildren(); ++i) {
    MockClass* child = klass->GetChild(i);
    EnsureStateChangedTestRecursive(child, cur_depth + 1u, total_depth, transitions);
  }
}

void EnsureStateChangedTest(
    MockClass* root,
    size_t depth,
    std::vector<std::pair<SubtypeCheckInfo::State, SubtypeCheckInfo::State>> transitions) {
  ASSERT_EQ(depth, transitions.size());

  EnsureStateChangedTestRecursive(root, /*cur_depth*/0u, depth, transitions);
}

TEST_F(SubtypeCheckTest, EnsureInitialized_NoOverflow) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kInitialized;
  };

  constexpr size_t kMaxDepthForThisTest = BitString::kCapacity;
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    if (expect_when == kBeforeTransition) {
      EXPECT_EQ(SubtypeCheckInfo::kUninitialized, actual_state);
      return;
    }

    if (expect_when == kAfterTransition) {
      // After explicit transition has been completed.
      switch (kls->Depth()) {
      case 0:
        if (transition >= MockSubtypeOfTransition::kInitialized) {
          EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
        }
        break;
      default:
        if (transition >= MockSubtypeOfTransition::kInitialized) {
          if (transition == MockSubtypeOfTransition::kInitialized) {
            EXPECT_EQ(SubtypeCheckInfo::kInitialized, actual_state);
          } else if (transition == MockSubtypeOfTransition::kAssigned) {
            EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
          }
        }
        break;
      }
    }

    if (expect_when == kAfterChildren) {
      if (transition >= MockSubtypeOfTransition::kInitialized) {
        ASSERT_NE(kls->Depth(), kMaxDepthForThisTest);
        EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
      }
    }
  };

  // Initialize every level 0-3.
  // Intermediate levels become "assigned", max levels become initialized.
  EnsureStateChangedTestRecursiveGeneric(root_, 0u, kMaxDepthForThisTest, transitions, expected);

  auto transitions_uninitialize = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kUninitialized;
  };

  auto expected_uninitialize = [](MockClass* kls,
                                  int expect_when,
                                  SubtypeCheckInfo::State actual_state,
                                  MockSubtypeOfTransition transition) {
    UNUSED(kls);
    UNUSED(transition);
    if (expect_when >= kAfterTransition) {
      EXPECT_EQ(SubtypeCheckInfo::kUninitialized, actual_state);
    }
  };

  // Uninitialize the entire tree after it was assigned.
  EnsureStateChangedTestRecursiveGeneric(root_,
                                         0u,
                                         kMaxDepthForThisTest,
                                         transitions_uninitialize,
                                         expected_uninitialize);
}

TEST_F(SubtypeCheckTest, EnsureAssigned_TooDeep) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kAssigned;
  };

  constexpr size_t kMaxDepthForThisTest = BitString::kCapacity + 1u;
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    UNUSED(transition);
    if (expect_when == kAfterTransition) {
      if (kls->Depth() > BitString::kCapacity) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      }
    }
  };

  // Assign every level 0-4.
  // We cannot assign 4th level, so it will overflow instead.
  EnsureStateChangedTestRecursiveGeneric(root_, 0u, kMaxDepthForThisTest, transitions, expected);
}

TEST_F(SubtypeCheckTest, EnsureAssigned_TooDeep_OfTooDeep) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kAssigned;
  };

  constexpr size_t kMaxDepthForThisTest = BitString::kCapacity + 2u;
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    UNUSED(transition);
    if (expect_when == kAfterTransition) {
      if (kls->Depth() > BitString::kCapacity) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      }
    }
  };

  // Assign every level 0-5.
  // We cannot assign 4th level, so it will overflow instead.
  // In addition, level 5th cannot be assigned (parent is overflowed), so it will also fail.
  EnsureStateChangedTestRecursiveGeneric(root_, 0u, kMaxDepthForThisTest, transitions, expected);
}

constexpr size_t MaxWidthCutOff(size_t depth) {
  if (depth == 0) {
    return 1;
  }
  if (depth > BitString::kCapacity) {
    return std::numeric_limits<size_t>::max();
  }
  return MaxInt<size_t>(BitString::kBitSizeAtPosition[depth - 1]);
}

// Either itself is too wide, or any of the parents were too wide.
bool IsTooWide(MockClass* kls) {
  if (kls == nullptr || kls->Depth() == 0u) {
    // Root is never too wide.
    return false;
  } else {
    if (kls->GetX() >= MaxWidthCutOff(kls->Depth())) {
      return true;
    }
  }
  return IsTooWide(kls->GetParent());
}

// Either itself is too deep, or any of the parents were too deep.
bool IsTooDeep(MockClass* kls) {
  if (kls == nullptr || kls->Depth() == 0u) {
    // Root is never too deep.
    return false;
  } else {
    if (kls->Depth() > BitString::kCapacity) {
      return true;
    }
  }
  return false;
}

TEST_F(SubtypeCheckTest, EnsureInitialized_TooWide) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kAssigned;
  };

  // Pick the 2nd level because has the most narrow # of bits.
  constexpr size_t kTargetDepth = 2;
  constexpr size_t kMaxWidthCutOff = MaxWidthCutOff(kTargetDepth);

  constexpr size_t kMaxDepthForThisTest = std::numeric_limits<size_t>::max();
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    UNUSED(transition);
    // Note: purposefuly ignore the too-deep children in the premade tree.
    if (expect_when == kAfterTransition && kls->Depth() <= BitString::kCapacity) {
      if (IsTooWide(kls)) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      } else {
        EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
      }
    }
  };

  {
    // Create too-wide siblings at the kTargetDepth level.
    MockClass* child = root_->FindChildAt(/*x*/0, kTargetDepth - 1u);
    CreateTreeFor(child, kMaxWidthCutOff*2, /*depth*/1);
    ASSERT_LE(kMaxWidthCutOff*2, child->GetNumberOfChildren());
    ASSERT_TRUE(IsTooWide(child->GetMaxChild())) << *(child->GetMaxChild());
    // Leave the rest of the tree as the default.
  }

  // Try to assign every level
  // It will fail once it gets to the "too wide" siblings and cause overflows.
  EnsureStateChangedTestRecursiveGeneric(root_,
                                         0u,
                                         kMaxDepthForThisTest,
                                         transitions,
                                         expected);
}

TEST_F(SubtypeCheckTest, EnsureInitialized_TooWide_TooWide) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kAssigned;
  };

  // Pick the 2nd level because has the most narrow # of bits.
  constexpr size_t kTargetDepth = 2;
  constexpr size_t kMaxWidthCutOff = MaxWidthCutOff(kTargetDepth);
  constexpr size_t kMaxWidthCutOffSub = MaxWidthCutOff(kTargetDepth+1u);

  constexpr size_t kMaxDepthForThisTest = std::numeric_limits<size_t>::max();
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    UNUSED(transition);
    // Note: purposefuly ignore the too-deep children in the premade tree.
    if (expect_when == kAfterTransition && kls->Depth() <= BitString::kCapacity) {
      if (IsTooWide(kls)) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      } else {
        EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
      }
    }
  };

  {
    // Create too-wide siblings at the kTargetDepth level.
    MockClass* child = root_->FindChildAt(/*x*/0, kTargetDepth - 1);
    CreateTreeFor(child, kMaxWidthCutOff*2, /*depth*/1);
    ASSERT_LE(kMaxWidthCutOff*2, child->GetNumberOfChildren()) << *child;
    ASSERT_TRUE(IsTooWide(child->GetMaxChild())) << *(child->GetMaxChild());
    // Leave the rest of the tree as the default.

    // Create too-wide children for a too-wide parent.
    MockClass* child_subchild = child->FindChildAt(/*x*/0, kTargetDepth);
    CreateTreeFor(child_subchild, kMaxWidthCutOffSub*2, /*depth*/1);
    ASSERT_LE(kMaxWidthCutOffSub*2, child_subchild->GetNumberOfChildren()) << *child_subchild;
    ASSERT_TRUE(IsTooWide(child_subchild->GetMaxChild())) << *(child_subchild->GetMaxChild());
  }

  // Try to assign every level
  // It will fail once it gets to the "too wide" siblings and cause overflows.
  // Furthermore, assigning any subtree whose ancestor is too wide will also fail.
  EnsureStateChangedTestRecursiveGeneric(root_, 0u, kMaxDepthForThisTest, transitions, expected);
}

void EnsureSubtypeOfCorrect(MockClass* a, MockClass* b) {
  MockScopedLockSubtypeCheck lock_a;
  MockScopedLockMutator lock_b;
  using SCTree = MockSubtypeCheck;

  auto IsAssigned = [](SCTree& tree) {
    MockScopedLockSubtypeCheck lock_a;
    MockScopedLockMutator lock_b;
    // This assumes that MockClass is always called with EnsureAssigned.
    EXPECT_NE(SubtypeCheckInfo::kInitialized, tree.GetState());
    EXPECT_NE(SubtypeCheckInfo::kUninitialized, tree.GetState());
    // Use our own test checks, so we are actually testing different logic than the impl.
    return !(IsTooDeep(&tree.GetClass()) || IsTooWide(&tree.GetClass()));
  };

  SCTree src_tree = SCTree::Lookup(a);
  SCTree target_tree = SCTree::Lookup(b);

  SCOPED_TRACE("class A");
  SCOPED_TRACE(*a);
  SCOPED_TRACE("class B");
  SCOPED_TRACE(*b);

  SubtypeCheckInfo::Result slow_result =
      a->SlowIsSubtypeOf(b) ? SubtypeCheckInfo::kSubtypeOf : SubtypeCheckInfo::kNotSubtypeOf;
  SubtypeCheckInfo::Result fast_result = src_tree.IsSubtypeOf(target_tree);

  // Target must be Assigned for this check to succeed.
  // Source is either Overflowed | Assigned (in this case).

  if (IsAssigned(src_tree) && IsAssigned(target_tree)) {
    ASSERT_EQ(slow_result, fast_result);
  } else if (IsAssigned(src_tree)) {
    // A is assigned. B is >= initialized.
    ASSERT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, fast_result);
  } else if (IsAssigned(target_tree)) {
    // B is assigned. A is >= initialized.
    ASSERT_EQ(slow_result, fast_result);
  } else {
    // Neither A,B are assigned.
    ASSERT_EQ(SubtypeCheckInfo::kUnknownSubtypeOf, fast_result);
  }

  // Use asserts,  not expects to immediately fail.
  // Otherwise the entire tree (very large) could potentially be broken.
}

void EnsureSubtypeOfRecursive(MockClass* kls_root) {
  MockScopedLockMutator mutator_lock_fake_;

  auto visit_func = [&](MockClass* kls) {
    kls->Visit([&](MockClass* inner_class) {
      EnsureSubtypeOfCorrect(kls, inner_class);
      EnsureSubtypeOfCorrect(inner_class, kls);

      if (::testing::Test::HasFatalFailure()) {
        return false;
      }

      return true;  // Keep visiting.
    });

    if (::testing::Test::HasFatalFailure()) {
        return false;
    }

    return true;  // Keep visiting.
  };

  ASSERT_NO_FATAL_FAILURE(kls_root->Visit(visit_func));
}

TEST_F(SubtypeCheckTest, EnsureInitialized_TooWide_TooDeep) {
  auto transitions = [](MockClass* kls) {
    UNUSED(kls);
    return MockSubtypeOfTransition::kAssigned;
  };

  // Pick the 2nd level because has the most narrow # of bits.
  constexpr size_t kTargetDepth = 2;
  constexpr size_t kTooDeepTargetDepth = BitString::kCapacity + 1;
  constexpr size_t kMaxWidthCutOff = MaxWidthCutOff(kTargetDepth);

  constexpr size_t kMaxDepthForThisTest = std::numeric_limits<size_t>::max();
  auto expected = [=](MockClass* kls,
                      int expect_when,
                      SubtypeCheckInfo::State actual_state,
                      MockSubtypeOfTransition transition) {
    UNUSED(transition);
    if (expect_when == kAfterTransition) {
      if (IsTooDeep(kls)) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      } else if (IsTooWide(kls)) {
        EXPECT_EQ(SubtypeCheckInfo::kOverflowed, actual_state);
      } else {
        EXPECT_EQ(SubtypeCheckInfo::kAssigned, actual_state);
      }
    }
  };

  {
    // Create too-wide siblings at the kTargetDepth level.
    MockClass* child = root_->FindChildAt(/*x*/0, kTargetDepth - 1u);
    CreateTreeFor(child, kMaxWidthCutOff*2, /*depth*/1);
    ASSERT_LE(kMaxWidthCutOff*2, child->GetNumberOfChildren());
    ASSERT_TRUE(IsTooWide(child->GetMaxChild())) << *(child->GetMaxChild());
    // Leave the rest of the tree as the default.

    // Create too-deep children for a too-wide parent.
    MockClass* child_subchild = child->GetMaxChild();
    ASSERT_TRUE(child_subchild != nullptr);
    ASSERT_EQ(0u, child_subchild->GetNumberOfChildren()) << *child_subchild;
    CreateTreeFor(child_subchild, /*width*/1, /*levels*/kTooDeepTargetDepth);
    MockClass* too_deep_child = child_subchild->FindChildAt(0, kTooDeepTargetDepth + 2);
    ASSERT_TRUE(too_deep_child != nullptr) << child_subchild->ToDotGraph();
    ASSERT_TRUE(IsTooWide(too_deep_child)) << *(too_deep_child);
    ASSERT_TRUE(IsTooDeep(too_deep_child)) << *(too_deep_child);
  }

  // Try to assign every level
  // It will fail once it gets to the "too wide" siblings and cause overflows.
  EnsureStateChangedTestRecursiveGeneric(root_, 0u, kMaxDepthForThisTest, transitions, expected);

  // Check every class against every class for "x instanceof y".
  EnsureSubtypeOfRecursive(root_);
}

// TODO: add dcheck for child-parent invariants (e.g. child < parent.next) and death tests

}  // namespace art
