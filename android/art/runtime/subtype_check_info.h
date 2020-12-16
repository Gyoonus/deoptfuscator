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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_INFO_H_
#define ART_RUNTIME_SUBTYPE_CHECK_INFO_H_

#include "base/bit_string.h"
#include "subtype_check_bits.h"

// Forward-declare for testing purposes.
struct SubtypeCheckInfoTest;

namespace art {

/**
 * SubtypeCheckInfo is a logical label for the class SubtypeCheck data, which is necessary to
 * perform efficient O(1) subtype comparison checks. See also subtype_check.h
 * for the more general explanation of how the labels are used overall.
 *
 * For convenience, we also store the class depth within an SubtypeCheckInfo, since nearly all
 * calculations are dependent on knowing the depth of the class.
 *
 * A SubtypeCheckInfo logically has:
 *          * Depth - How many levels up to the root (j.l.Object)?
 *          * PathToRoot - Possibly truncated BitString that encodes path to root
 *          * Next - The value a newly inserted Child would get appended to its path.
 *          * Overflow - If this path can never become a full path.
 *
 * Depending on the values of the above, it can be in one of these logical states,
 * which are introduced in subtype_check.h:
 *
 *               Transient States                         Terminal States
 *
 *  +-----------------+     +--------------------+     +-------------------+
 *  |                 |     |                    |     |                   |
 *  |  Uninitialized  | +--->    Initialized     | +--->     Assigned      |
 *  |                 |     |                    |     |                   |
 *  +--------+--------+     +---------+----------+     +-------------------+
 *           |                        |
 *           |                        |
 *           |                        |                +-------------------+
 *           |                        +---------------->                   |
 *           |                                         |     Overflowed    |
 *           +----------------------------------------->                   |
 *                                                     +-------------------+
 *
 * Invariants:
 *
 *   Initialized      => Parent >= Initialized
 *
 *   Assigned         => Parent == Assigned
 *
 *   Overflowed       => Parent == Overflowed || Parent.Next == Overflowed
 *
 * Thread-safety invariants:
 *
 *   Initialized      => Parent == Assigned
 *   // For a class that has an Initialized bitstring, its superclass needs to have an
 *   // Assigned bitstring since if its super class's bitstring is not Assigned yet,
 *   // once it becomes Assigned, we cannot update its children's bitstrings to maintain
 *   // all the tree invariants (below) atomically.
 *
 * --------------------------------------------------------------------------------------------
 * Knowing these transitions above, we can more closely define the various terms and operations:
 *
 * Definitions:
 *   (see also base/bit_string.h definitions).
 *
 *           Depth :=  Distance(Root, Class)
 *     Safe(Depth) :=  Min(Depth, MaxBitstringLen)
 *      PathToRoot :=  Bitstring[0..Safe(Depth))
 *           Next  :=  Bitstring[Depth]
 *           OF    ∈   {False, True}
 *    TruncPath(D) :=  PathToRoot[0..D)
 *
 * Local Invariants:
 *
 *   Uninitialized <=> StrLen(PathToRoot) == 0
 *                     Next == 0
 *                     OF == False
 *   Initialized   <=> StrLen(PathToRoot) < Depth
 *                     Next == 1
 *                     OF == False
 *   Assigned      <=> StrLen(PathToRoot) == Depth
 *                     Next >= 1
 *                     OF == False
 *   Overflowed    <=> OF == True
 *
 * Tree Invariants:
 *
 *   Uninitialized =>
 *     forall child ∈ Children(Class):
 *       child.State == Uninitialized
 *
 *   Assigned       =>
 *     forall child ∈ Children(Class):
 *       Next > Child.PathToRoot[Child.Depth-1]
 *
 *   ! Uninitialized =>
 *     forall ancestor ∈ Ancestors(Class):
 *       TruncPath(ancestor.Depth) == ancestor.PathToRoot
 *     forall unrelated ∈ (Classes - Ancestors(Class))
 *         s.t. unrelated.State == Assigned:
 *       TruncPath(unrelated.Depth) != unrelated.PathToRoot
 *
 * Thread-safety invariants:
 *
 *   Initialized   <=> StrLen(PathToRoot) == Safe(Depth - 1)
 *   // Initialized State corresponds to exactly 1 bitstring.
 *   // Cannot transition from Initialized to Initialized.
 */
struct SubtypeCheckInfo {
  // See above documentation for possible state transitions.
  enum State {
    kUninitialized,
    kInitialized,
    kAssigned,
    kOverflowed
  };

  // The result of a "src IsSubType target" check:
  enum Result {
    kUnknownSubtypeOf,  // Not enough data. Operand states weren't high enough.
    kNotSubtypeOf,      // Enough data. src is not a subchild of the target.
    kSubtypeOf          // Enough data. src is a subchild of the target.
  };

  // Get the raw depth.
  size_t GetDepth() const {
    return depth_;
  }

  // Chop off the depth, returning only the bitstring+of state.
  // (Used to store into memory, since storing the depth would be redundant.)
  SubtypeCheckBits GetSubtypeCheckBits() const {
    return bitstring_and_of_;
  }

  // Create from the depth and the bitstring+of state.
  // This is done for convenience to avoid passing in "depth" everywhere,
  // since our current state is almost always a function of depth.
  static SubtypeCheckInfo Create(SubtypeCheckBits compressed_value, size_t depth) {
    SubtypeCheckInfo io;
    io.depth_ = depth;
    io.bitstring_and_of_ = compressed_value;
    io.DcheckInvariants();
    return io;
  }

  // Is this a subtype of the target?
  //
  // The current state must be at least Initialized, and the target state
  // must be Assigned, otherwise the result will return kUnknownSubtypeOf.
  //
  // Normally, return kSubtypeOf or kNotSubtypeOf.
  Result IsSubtypeOf(const SubtypeCheckInfo& target) {
    if (target.GetState() != SubtypeCheckInfo::kAssigned) {
      return Result::kUnknownSubtypeOf;
    } else if (GetState() == SubtypeCheckInfo::kUninitialized) {
      return Result::kUnknownSubtypeOf;
    }

    BitString::StorageType source_value = GetEncodedPathToRoot();
    BitString::StorageType target_value = target.GetEncodedPathToRoot();
    BitString::StorageType target_mask = target.GetEncodedPathToRootMask();

    bool result = (source_value & target_mask) == (target_value);
    if (result) {
      DCHECK_EQ(GetPathToRoot().Truncate(target.GetSafeDepth()), target.GetPathToRoot())
          << "Source: " << *this << ", Target: " << target;
    } else {
      DCHECK_NE(GetPathToRoot().Truncate(target.GetSafeDepth()), target.GetPathToRoot())
          << "Source: " << *this << ", Target: " << target;
    }

    // Note: We could've also used shifts here, as described in subtype_check_bits.h,
    // but it doesn't make much of a difference in the Runtime since we aren't trying to optimize
    // for code size.

    return result ? Result::kSubtypeOf : Result::kNotSubtypeOf;
  }

  // Returns a new root SubtypeCheckInfo with a blank PathToRoot.
  // Post-condition: The return valued has an Assigned state.
  static SubtypeCheckInfo CreateRoot() {
    SubtypeCheckInfo io{};
    io.depth_ = 0u;
    io.SetNext(io.GetNext() + 1u);

    // The root is always considered assigned once it is no longer Initialized.
    DCHECK_EQ(SubtypeCheckInfo::kAssigned, io.GetState());
    return io;
  }

  // Copies the current PathToRoot into the child.
  //
  // If assign_next is true, then also assign a new SubtypeCheckInfo for a child by
  // assigning the current Next value to its PathToRoot[Depth] component.
  // Updates the current Next value as a side effect.
  //
  // Preconditions: State is either Assigned or Overflowed.
  // Returns: A new child >= Initialized state.
  SubtypeCheckInfo CreateChild(bool assign_next) {
    SubtypeCheckInfo child = *this;  // Copy everything (path, next, of).
    child.depth_ = depth_ + 1u;

    // Must be Assigned or Overflowed in order to create a subchild.
    DCHECK(GetState() == kAssigned || GetState() == kOverflowed)
        << "Unexpected bitstring state: " << GetState();

    // Begin transition to >= Initialized.

    // Always attempt to re-initialize Child's Next value.
    // Next must be non-0 to disambiguate it from Uninitialized.
    child.MaybeInitNext();

    // Always clear the inherited Parent's next Value, i.e. the child's last path entry.
    OverwriteNextValueFromParent(/*inout*/&child, BitStringChar{});

    // The state is now Initialized | Overflowed.
    DCHECK_NE(kAssigned, child.GetState()) << child.GetBitString();
    DCHECK_NE(kUninitialized, child.GetState()) << child.GetBitString();

    if (assign_next == false) {
      child.DcheckInvariants();
      return child;
    }

    // Begin transition to >= Assigned.

    // Assign attempt.
    if (HasNext() && !bitstring_and_of_.overflow_) {
      BitStringChar next = GetNext();
      if (next != next.MaximumValue()) {
        // The parent's "next" value is now the child's latest path element.
        OverwriteNextValueFromParent(/*inout*/&child, next);
        // Update self next value, so that future CreateChild calls
        // do not get the same path value.
        SetNext(next + 1u);
      } else {
        child.MarkOverflowed();  // Too wide.
      }
    } else {
      child.MarkOverflowed();  // Too deep, or parent was already overflowed.
    }

    // The state is now Assigned | Overflowed.
    DCHECK(child.GetState() == kAssigned || child.GetState() == kOverflowed);

    child.DcheckInvariants();
    return child;
  }

  // Get the current state (Uninitialized, Initialized, Assigned, or Overflowed).
  // See the "SubtypeCheckInfo" documentation above which explains how a state is determined.
  State GetState() const {
    if (bitstring_and_of_.overflow_) {
      // Overflowed if and only if the OF bit was set.
      return kOverflowed;
    }

    if (GetBitString().IsEmpty()) {
      // Empty bitstring (all 0s) -> uninitialized.
      return kUninitialized;
    }

    // Either Assigned or Initialized.
    BitString path_to_root = GetPathToRoot();

    DCHECK(!HasNext() || GetNext() != 0u)
        << "Expected (Assigned|Initialized) state to have >0 Next value: "
        << GetNext() << " path: " << path_to_root;

    if (path_to_root.Length() == depth_) {
      return kAssigned;
    }

    return kInitialized;
  }

  // Retrieve the path to root bitstring as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  BitString::StorageType GetEncodedPathToRoot() const {
    BitString::StorageType data = static_cast<BitString::StorageType>(GetPathToRoot());
    // Bit strings are logically in the least-significant memory.
    return data;
  }

  // Retrieve the path to root bitstring mask as a plain uintN_t that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  BitString::StorageType GetEncodedPathToRootMask() const {
    size_t num_bitchars = GetSafeDepth();
    size_t bitlength = BitString::GetBitLengthTotalAtPosition(num_bitchars);
    return MaskLeastSignificant<BitString::StorageType>(bitlength);
  }

  // Get the "Next" bitchar, assuming that there is one to get.
  BitStringChar GetNext() const {
    DCHECK(HasNext());
    return GetBitString()[depth_];
  }

  // Try to get the Next value, if there is one.
  // Returns: Whether or not there was a Next value.
  bool MaybeGetNext(/*out*/BitStringChar* next) const {
    DCHECK(next != nullptr);

    if (HasNext()) {
      *next = GetBitString()[depth_];
      return true;
    }
    return false;
  }

 private:
  // Constructor intended for testing. Runs all invariant checks.
  SubtypeCheckInfo(BitString path_to_root, BitStringChar next, bool overflow, size_t depth) {
    SubtypeCheckBits iod;
    iod.bitstring_ = path_to_root;
    iod.overflow_ = overflow;

    bitstring_and_of_ = iod;
    depth_ = depth;

    // Len(Path-to-root) <= Depth.
    DCHECK_GE(depth_, path_to_root.Length())
        << "Path was too long for the depth, path: " << path_to_root;

    bool did_overlap = false;
    if (HasNext()) {
      if (kIsDebugBuild) {
        did_overlap = (GetNext() != 0u);
      }

      SetNext(next);

      DCHECK_EQ(next, GetNext());
    }
    // "Next" must be set before we can check the invariants.
    DcheckInvariants();
    DCHECK(!did_overlap)
          << "Path to root overlapped with Next value, path: " << path_to_root;
    DCHECK_EQ(path_to_root, GetPathToRoot());
  }

  // Factory intended for testing. Skips DcheckInvariants.
  static SubtypeCheckInfo MakeUnchecked(BitString bitstring, bool overflow, size_t depth) {
    SubtypeCheckBits iod;
    iod.bitstring_ = bitstring;
    iod.overflow_ = overflow;

    SubtypeCheckInfo io;
    io.depth_ = depth;
    io.bitstring_and_of_ = iod;

    return io;
  }

  void SetNext(BitStringChar next) {
    DCHECK(HasNext());
    BitString bs = GetBitString();
    bs.SetAt(depth_, next);
    SetBitString(bs);
  }

  void SetNextUnchecked(BitStringChar next) {
    BitString bs = GetBitString();
    bs.SetAt(depth_, next);
    SetBitStringUnchecked(bs);
  }

  // If there is a next field, set it to 1.
  void MaybeInitNext() {
    if (HasNext()) {
      // Clearing out the "Next" value like this
      // is often an intermediate operation which temporarily
      // violates the invariants. Do not do the extra dchecks.
      SetNextUnchecked(BitStringChar{});
      SetNextUnchecked(GetNext()+1u);
    }
  }

  BitString GetPathToRoot() const {
    size_t end = GetSafeDepth();
    return GetBitString().Truncate(end);
  }

  bool HasNext() const {
    return depth_ < BitString::kCapacity;
  }

  void MarkOverflowed() {
    bitstring_and_of_.overflow_ = true;
  }

  static constexpr bool HasBitStringCharStorage(size_t idx) {
    return idx < BitString::kCapacity;
  }

  size_t GetSafeDepth() const {
    return GetSafeDepth(depth_);
  }

  // Get a "safe" depth, one that is truncated to the bitstring max capacity.
  // Using a value larger than this will cause undefined behavior.
  static size_t GetSafeDepth(size_t depth) {
    return std::min(depth, BitString::kCapacity);
  }

  BitString GetBitString() const {
    return bitstring_and_of_.bitstring_;
  }

  void SetBitString(const BitString& val) {
    SetBitStringUnchecked(val);
    DcheckInvariants();
  }

  void SetBitStringUnchecked(const BitString& val) {
    bitstring_and_of_.bitstring_ = val;
  }

  void OverwriteNextValueFromParent(/*inout*/SubtypeCheckInfo* child, BitStringChar value) const {
    // Helper function for CreateChild.
    if (HasNext()) {
      // When we copied the "Next" value, it is now our
      // last path component in the child.
      // Always overwrite it with either a cleared value or the parent's Next value.
      BitString bs = child->GetBitString();

      // Safe write. This.Next always occupies same slot as Child[Depth_].
      DCHECK(child->HasBitStringCharStorage(depth_));

      bs.SetAt(depth_, value);

      // The child is temporarily in a bad state until it is fixed up further.
      // Do not do the normal dchecks which do not allow transient badness.
      child->SetBitStringUnchecked(bs);
    }
  }

  void DcheckInvariants() const {
    if (kIsDebugBuild) {
      CHECK_GE(GetSafeDepth(depth_ + 1u), GetBitString().Length())
          << "Bitstring too long for depth, bitstring: " << GetBitString() << ", depth: " << depth_;

      BitString path_to_root = GetPathToRoot();

      // A 'null' (\0) character in path-to-root must be followed only
      // by other null characters.
      size_t i;
      for (i = 0; i < BitString::kCapacity; ++i) {
        BitStringChar bc = path_to_root[i];
        if (bc == 0u) {
          break;
        }
      }

      // All characters following a 0 must also be 0.
      for (; i < BitString::kCapacity; ++i) {
        BitStringChar bc = path_to_root[i];
        if (bc != 0u) {
          LOG(FATAL) << "Path to root had non-0s following 0s: " << path_to_root;
        }
      }

       // Trigger any dchecks in GetState.
      (void)GetState();
    }
  }

  SubtypeCheckInfo() = default;
  size_t depth_;
  SubtypeCheckBits bitstring_and_of_;

  friend struct ::SubtypeCheckInfoTest;
  friend std::ostream& operator<<(std::ostream& os, const SubtypeCheckInfo& io);
};

// Prints the SubtypeCheckInfo::State, e.g. "kUnitialized".
inline std::ostream& operator<<(std::ostream& os, const SubtypeCheckInfo::State& state) {
  switch (state) {
    case SubtypeCheckInfo::kUninitialized:
      os << "kUninitialized";
      break;
    case SubtypeCheckInfo::kInitialized:
      os << "kInitialized";
      break;
    case SubtypeCheckInfo::kAssigned:
      os << "kAssigned";
      break;
    case SubtypeCheckInfo::kOverflowed:
      os << "kOverflowed";
      break;
    default:
      os << "(Invalid SubtypeCheckInfo::State " << static_cast<int>(state) << ")";
  }
  return os;
}

// Prints e.g. "SubtypeCheckInfo{BitString[1,2,3], depth: 3, of:1}"
inline std::ostream& operator<<(std::ostream& os, const SubtypeCheckInfo& io) {
  os << "SubtypeCheckInfo{" << io.GetBitString() << ", "
     << "depth: " << io.depth_ << ", of:" << io.bitstring_and_of_.overflow_ << "}";
  return os;
}

}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_INFO_H_
