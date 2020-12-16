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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_H_
#define ART_RUNTIME_SUBTYPE_CHECK_H_

#include "subtype_check_bits_and_status.h"
#include "subtype_check_info.h"

#include "base/mutex.h"
#include "mirror/class.h"
#include "runtime.h"

// Build flag for the bitstring subtype check runtime hooks.
constexpr bool kBitstringSubtypeCheckEnabled = false;

/**
 * Any node in a tree can have its path (from the root to the node) represented as a string by
 * concatenating the path of the parent to that of the current node.
 *
 * We can annotate each node with a `sibling-label` which is some value unique amongst all of the
 * node's siblings. As a special case, the root is empty.
 *
 *           (none)
 *        /    |     \
 *       A     B      C
 *     /   \
 *    A’    B’
 *          |
 *          A’’
 *          |
 *          A’’’
 *          |
 *          A’’’’
 *
 * Given these sibling-labels, we can now encode the path from any node to the root by starting at
 * the node and going up to the root, marking each node with this `path-label`. The special
 * character $ means "end of path".
 *
 *             $
 *        /    |      \
 *       A$    B$     C$
 *     /    \
 *   A’A$   B’A$
 *           |
 *           A’’B’A$
 *           |
 *           A’’’A’’B’A$
 *           |
 *           A’’’’A’’B’A$
 *
 * Given the above `path-label` we can express if any two nodes are an offspring of the other
 * through a O(1) expression:
 *
 *    x <: y :=
 *      suffix(x, y) == y
 *
 * In the above example suffix(x,y) means the suffix of x that is as long as y (right-padded with
 * $s if x is shorter than y) :
 *
 *    suffix(x,y) := x(x.length - y.length .. 0]
 *                     + repeat($, max(y.length - x.length, 0))
 *
 * A few generalities here to elaborate:
 *
 * - There can be at most D levels in the tree.
 * - Each level L has an alphabet A, and the maximum number of
 *   nodes is determined by |A|
 * - The alphabet A can be a subset, superset, equal, or unique with respect to the other alphabets
 *   without loss of generality. (In practice it would almost always be a subset of the previous
 *   level’s alphabet as we assume most classes have less children the deeper they are.)
 * - The `sibling-label` doesn’t need to be stored as an explicit value. It can a temporary when
 *   visiting every immediate child of a node. Only the `path-label` needs to be actually stored for
 *   every node.
 *
 * The path can also be reversed, and use a prefix instead of a suffix to define the subchild
 * relation.
 *
 *             $
 *        /    |      \    \
 *       A$    B$     C$    D$
 *     /    \
 *   AA’$   AB’$
 *            |
 *            AB’A’’$
 *            |
 *            AB’A’’A’’’$
 *            |
 *            AB’A’’A’’’A’’’’$
 *
 *    x <: y :=
 *      prefix(x, y) == y
 *
 *    prefix(x,y) := x[0 .. y.length)
 *                     + repeat($, max(y.length - x.length, 0))
 *
 * In a dynamic tree, new nodes can be inserted at any time. This means if a minimal alphabet is
 * selected to contain the initial tree hierarchy, later node insertions will be illegal because
 * there is no more room to encode the path.
 *
 * In this simple example with an alphabet A,B,C and max level 1:
 *
 *     Level
 *     0:               $
 *              /     |     \     \
 *     1:      A$     B$     C$    D$   (illegal)
 *              |
 *     2:      AA$  (illegal)
 *
 * Attempting to insert the sibling “D” at Level 1 would be illegal because the Alphabet(1) is
 * {A,B,C} and inserting an extra node would mean the `sibling-label` is no longer unique.
 * Attempting to insert “AA$” is illegal because the level 2 is more than the max level 1.
 *
 * One solution to this would be to revisit the entire graph, select a larger alphabet to that
 * every `sibling-label` is unique, pick a larger max level count, and then store the updated
 * `path-label` accordingly.
 *
 * The more common approach would instead be to select a set of alphabets and max levels statically,
 * with large enough sizes, for example:
 *
 *     Alphabets = {{A,B,C,D}, {A,B,C}, {A,B}, {A}}
 *     Max Levels = |Alphabets|
 *
 * Which would allow up to 4 levels with each successive level having 1 less max siblings.
 *
 * Attempting to insert a new node into the graph which does not fit into that level’s alphabet
 * would be represented by re-using the `path-label` of the parent. Such a `path_label` would be
 * considered truncated (because it would only have a prefix of the full path from the root to the
 * node).
 *
 *    Level
 *    0:             $
 *             /     |     \     \
 *    1:      A$     B$     C$    $   (same as parent)
 *             |
 *    2:      A$ (same as parent)
 *
 * The updated relation for offspring is then:
 *
 *    x <: y :=
 *      if !truncated_path(y):
 *        return prefix(x, y) == y               // O(1)
 *      else:
 *        return slow_check_is_offspring(x, y)   // worse than O(1)
 *
 * (Example definition of truncated_path -- any semantically equivalent way to check that the
 *  sibling's `sibling-label` is not unique will do)
 *
 *    truncated_path(y) :=
 *      return y == parent(y)
 *
 * (Example definition. Any slower-than-O(1) definition will do here. This is the traversing
 * superclass hierarchy solution)
 *
 *    slow_check_is_offspring(x, y) :=
 *      if not x: return false
 *      else: return x == y || recursive_is_offspring(parent(x), y)
 *
 * In which case slow_check_is_offspring is some non-O(1) way to check if x and is an offspring of y.
 *
 * In addition, note that it doesn’t matter if the "x" from above is a unique sibling or not; the
 * relation will still be correct.
 *
 * ------------------------------------------------------------------------------------------------
 *
 * Leveraging truncated paths to minimize path lengths.
 *
 * As observed above, for any x <: y, it is sufficient to have a full path only for y,
 * and x can be truncated (to its nearest ancestor's full path).
 *
 * We call a node that stores a full path "Assigned", and a node that stores a truncated path
 * either "Initialized" or "Overflowed."
 *
 * "Initialized" means it is still possible to assign a full path to the node, and "Overflowed"
 * means there is insufficient characters in the alphabet left.
 *
 * In this example, assume that we attempt to "Assign" all non-leafs if possible. Leafs
 * always get truncated (as either Initialized or Overflowed).
 *
 *     Alphabets = {{A,B,C,D}, {A,B}}
 *     Max Levels = |Alphabets|
 *
 *    Level
 *    0:             $
 *             /     |     \     \     \
 *    1:      A$     B$     C$    D$    $ (Overflowed: Too wide)
 *            |             |
 *    2:     AA$            C$ (Initialized)
 *            |
 *    3:     AA$ (Overflowed: Too deep)
 *
 * (All un-annotated nodes are "Assigned").
 * Above, the node at level 3 becomes overflowed because it exceeds the max levels. The
 * right-most node at level 1 becomes overflowed because there's no characters in the alphabet
 * left in that level.
 *
 * The "C$" node is Initialized at level 2, but it can still be promoted to "Assigned" later on
 * if we wanted to.
 *
 * In particular, this is the strategy we use in our implementation
 * (SubtypeCheck::EnsureInitialized, SubtypeCheck::EnsureAssigned).
 *
 * Since the # of characters in our alphabet (BitString) is very limited, we want to avoid
 * allocating a character to a node until its absolutely necessary.
 *
 * All node targets (in `src <: target`) get Assigned, and any parent of an Initialized
 * node also gets Assigned.
 */
namespace art {

struct MockSubtypeCheck;  // Forward declaration for testing.

// This class is using a template parameter to enable testability without losing performance.
// ClassPtr is almost always `mirror::Class*` or `ObjPtr<mirror::Class>`.
template <typename ClassPtr /* Pointer-like type to Class */>
struct SubtypeCheck {
  // Force this class's SubtypeCheckInfo state into at least Initialized.
  // As a side-effect, all parent classes also become Assigned|Overflowed.
  //
  // Cost: O(Depth(Class))
  //
  // Post-condition: State is >= Initialized.
  // Returns: The precise SubtypeCheckInfo::State.
  static SubtypeCheckInfo::State EnsureInitialized(ClassPtr klass)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return InitializeOrAssign(klass, /*assign*/false).GetState();
  }

  // Force this class's SubtypeCheckInfo state into Assigned|Overflowed.
  // As a side-effect, all parent classes also become Assigned|Overflowed.
  //
  // Cost: O(Depth(Class))
  //
  // Post-condition: State is Assigned|Overflowed.
  // Returns: The precise SubtypeCheckInfo::State.
  static SubtypeCheckInfo::State EnsureAssigned(ClassPtr klass)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return InitializeOrAssign(klass, /*assign*/true).GetState();
  }

  // Resets the SubtypeCheckInfo into the Uninitialized state.
  //
  // Intended only for the AOT image writer.
  // This is a static function to avoid calling klass.Depth(), which is unsupported
  // in some portions of the image writer.
  //
  // Cost: O(1).
  //
  // Returns: A state that is always Uninitialized.
  static SubtypeCheckInfo::State ForceUninitialize(ClassPtr klass)
    REQUIRES(Locks::subtype_check_lock_)
    REQUIRES_SHARED(Locks::mutator_lock_) {
    // Trying to do this in a real runtime will break thread safety invariants
    // of existing live objects in the class hierarchy.
    // This is only safe as the last step when the classes are about to be
    // written out as an image and IsSubClass is never used again.
    DCHECK(Runtime::Current() == nullptr || Runtime::Current()->IsAotCompiler())
      << "This only makes sense when compiling an app image.";

    // Directly read/write the class field here.
    // As this method is used by image_writer on a copy,
    // the Class* there is not a real class and using it for anything
    // more complicated (e.g. ObjPtr or Depth call) will fail dchecks.

    // OK. zero-initializing subtype_check_info_ puts us into the kUninitialized state.
    SubtypeCheckBits scb_uninitialized = SubtypeCheckBits{};
    WriteSubtypeCheckBits(klass, scb_uninitialized);

    // Do not use "SubtypeCheckInfo" API here since that requires Depth()
    // which would cause a dcheck failure.
    return SubtypeCheckInfo::kUninitialized;
  }

  // Retrieve the path to root bitstring as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the encoded_src value. Must be >= Initialized (EnsureInitialized).
  static BitString::StorageType GetEncodedPathToRootForSource(ClassPtr klass)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_NE(SubtypeCheckInfo::kUninitialized, GetSubtypeCheckInfo(klass).GetState());
    return GetSubtypeCheckInfo(klass).GetEncodedPathToRoot();
  }

  // Retrieve the path to root bitstring as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the encoded_target value. Must be Assigned (EnsureAssigned).
  static BitString::StorageType GetEncodedPathToRootForTarget(ClassPtr klass)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo(klass).GetState());
    return GetSubtypeCheckInfo(klass).GetEncodedPathToRoot();
  }

  // Retrieve the path to root bitstring mask as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the mask_target value. Must be Assigned (EnsureAssigned).
  static BitString::StorageType GetEncodedPathToRootMask(ClassPtr klass)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo(klass).GetState());
    return GetSubtypeCheckInfo(klass).GetEncodedPathToRootMask();
  }

  // Is the source class a subclass of the target?
  //
  // The source state must be at least Initialized, and the target state
  // must be Assigned, otherwise the result will return kUnknownSubtypeOf.
  //
  // See EnsureInitialized and EnsureAssigned. Ideally,
  // EnsureInitialized will be called previously on all possible sources,
  // and EnsureAssigned will be called previously on all possible targets.
  //
  // Runtime cost: O(Depth(Class)), but would be O(1) if depth was known.
  //
  // If the result is known, return kSubtypeOf or kNotSubtypeOf.
  static SubtypeCheckInfo::Result IsSubtypeOf(ClassPtr source, ClassPtr target)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckInfo sci = GetSubtypeCheckInfo(source);
    SubtypeCheckInfo target_sci = GetSubtypeCheckInfo(target);

    return sci.IsSubtypeOf(target_sci);
  }

  // Print SubtypeCheck bitstring and overflow to a stream (e.g. for oatdump).
  static std::ostream& Dump(ClassPtr klass, std::ostream& os)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return os << GetSubtypeCheckInfo(klass);
  }

  static void WriteStatus(ClassPtr klass, ClassStatus status)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    WriteStatusImpl(klass, status);
  }

 private:
  static ClassPtr GetParentClass(ClassPtr klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(klass->HasSuperClass());
    return ClassPtr(klass->GetSuperClass());
  }

  static SubtypeCheckInfo InitializeOrAssign(ClassPtr klass, bool assign)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (UNLIKELY(!klass->HasSuperClass())) {
      // Object root always goes directly from Uninitialized -> Assigned.

      const SubtypeCheckInfo root_sci = GetSubtypeCheckInfo(klass);
      if (root_sci.GetState() != SubtypeCheckInfo::kUninitialized) {
        return root_sci;  // No change needed.
      }

      const SubtypeCheckInfo new_root_sci = root_sci.CreateRoot();
      SetSubtypeCheckInfo(klass, new_root_sci);

      // The object root is always in the Uninitialized|Assigned state.
      DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo(klass).GetState())
          << "Invalid object root state, must be Assigned";
      return new_root_sci;
    }

    // Force all ancestors to Assigned | Overflowed.
    ClassPtr parent_klass = GetParentClass(klass);
    size_t parent_depth = InitializeOrAssign(parent_klass, /*assign*/true).GetDepth();
    if (kIsDebugBuild) {
      SubtypeCheckInfo::State parent_state = GetSubtypeCheckInfo(parent_klass).GetState();
      DCHECK(parent_state == SubtypeCheckInfo::kAssigned ||
          parent_state == SubtypeCheckInfo::kOverflowed)
          << "Expected parent Assigned|Overflowed, but was: " << parent_state;
    }

    // Read.
    SubtypeCheckInfo sci = GetSubtypeCheckInfo(klass, parent_depth + 1u);
    SubtypeCheckInfo parent_sci = GetSubtypeCheckInfo(parent_klass, parent_depth);

    // Modify.
    const SubtypeCheckInfo::State sci_state = sci.GetState();
    // Skip doing any work if the state is already up-to-date.
    //   - assign == false -> Initialized or higher.
    //   - assign == true  -> Assigned or higher.
    if (sci_state == SubtypeCheckInfo::kUninitialized ||
        (sci_state == SubtypeCheckInfo::kInitialized && assign)) {
      // Copy parent path into the child.
      //
      // If assign==true, this also appends Parent.Next value to the end.
      // Then the Parent.Next value is incremented to avoid allocating
      // the same value again to another node.
      sci = parent_sci.CreateChild(assign);  // Note: Parent could be mutated.
    } else {
      // Nothing to do, already >= Initialized.
      return sci;
    }

    // Post-condition: EnsureAssigned -> Assigned|Overflowed.
    // Post-condition: EnsureInitialized -> Not Uninitialized.
    DCHECK_NE(sci.GetState(), SubtypeCheckInfo::kUninitialized);

    if (assign) {
      DCHECK_NE(sci.GetState(), SubtypeCheckInfo::kInitialized);
    }

    // Write.
    SetSubtypeCheckInfo(klass, sci);                     // self
    SetSubtypeCheckInfo(parent_klass, parent_sci);       // parent

    return sci;
  }

  static SubtypeCheckBitsAndStatus ReadField(ClassPtr klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckBitsAndStatus current_bits_and_status;

    int32_t int32_data = klass->GetField32Volatile(klass->StatusOffset());
    current_bits_and_status.int32_alias_ = int32_data;

    if (kIsDebugBuild) {
      SubtypeCheckBitsAndStatus tmp;
      memcpy(&tmp, &int32_data, sizeof(tmp));
      DCHECK_EQ(0, memcmp(&tmp, &current_bits_and_status, sizeof(tmp))) << int32_data;
    }
    return current_bits_and_status;
  }

  static void WriteSubtypeCheckBits(ClassPtr klass, const SubtypeCheckBits& new_bits)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Use a "CAS" to write the SubtypeCheckBits in the class.
    // Although we have exclusive access to the bitstrings, because
    // ClassStatus and SubtypeCheckBits share the same word, another thread could
    // potentially overwrite that word still.

    SubtypeCheckBitsAndStatus new_value;
    ClassStatus old_status;
    SubtypeCheckBitsAndStatus full_old;
    while (true) {
      // TODO: Atomic compare-and-swap does not update the 'expected' parameter,
      // so we have to read it as a separate step instead.
      SubtypeCheckBitsAndStatus old_value = ReadField(klass);

      {
        SubtypeCheckBits old_bits = old_value.subtype_check_info_;
        if (memcmp(&old_bits, &new_bits, sizeof(old_bits)) == 0) {
          // Avoid dirtying memory when the data hasn't changed.
          return;
        }
      }

      full_old = old_value;
      old_status = old_value.status_;

      new_value = old_value;
      new_value.subtype_check_info_ = new_bits;

      if (kIsDebugBuild) {
        int32_t int32_data = 0;
        memcpy(&int32_data, &new_value, sizeof(int32_t));
        DCHECK_EQ(int32_data, new_value.int32_alias_) << int32_data;

        DCHECK_EQ(old_status, new_value.status_)
          << "full new: " << bit_cast<uint32_t>(new_value)
          << ", full old: " << bit_cast<uint32_t>(full_old);
      }

      if (CasFieldWeakSequentiallyConsistent32(klass,
                                               klass->StatusOffset(),
                                               old_value.int32_alias_,
                                               new_value.int32_alias_)) {
        break;
      }
    }
  }

  static void WriteStatusImpl(ClassPtr klass, ClassStatus status)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Despite not having a lock annotation, this is done with mutual exclusion.
    // See Class::SetStatus for more details.
    SubtypeCheckBitsAndStatus new_value;
    ClassStatus old_status;
    while (true) {
      // TODO: Atomic compare-and-swap does not update the 'expected' parameter,
      // so we have to read it as a separate step instead.
      SubtypeCheckBitsAndStatus old_value = ReadField(klass);
      old_status = old_value.status_;

      if (memcmp(&old_status, &status, sizeof(status)) == 0) {
        // Avoid dirtying memory when the data hasn't changed.
        return;
      }

      new_value = old_value;
      new_value.status_ = status;

      if (CasFieldWeakSequentiallyConsistent32(klass,
                                               klass->StatusOffset(),
                                               old_value.int32_alias_,
                                               new_value.int32_alias_)) {
        break;
      }
    }
  }

  static bool CasFieldWeakSequentiallyConsistent32(ClassPtr klass,
                                                   MemberOffset offset,
                                                   int32_t old_value,
                                                   int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Runtime::Current() != nullptr && Runtime::Current()->IsActiveTransaction()) {
      return klass->template
          CasFieldWeakSequentiallyConsistent32</*kTransactionActive*/true>(offset,
                                                                           old_value,
                                                                           new_value);
    } else {
      return klass->template
          CasFieldWeakSequentiallyConsistent32</*kTransactionActive*/false>(offset,
                                                                            old_value,
                                                                            new_value);
    }
  }

  // Get the SubtypeCheckInfo for a klass. O(Depth(Class)) since
  // it also requires calling klass->Depth.
  //
  // Anything calling this function will also be O(Depth(Class)).
  static SubtypeCheckInfo GetSubtypeCheckInfo(ClassPtr klass)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetSubtypeCheckInfo(klass, klass->Depth());
  }

  // Get the SubtypeCheckInfo for a klass with known depth.
  static SubtypeCheckInfo GetSubtypeCheckInfo(ClassPtr klass, size_t depth)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(depth, klass->Depth());
    SubtypeCheckBitsAndStatus current_bits_and_status = ReadField(klass);

    const SubtypeCheckInfo current =
        SubtypeCheckInfo::Create(current_bits_and_status.subtype_check_info_, depth);
    return current;
  }

  static void SetSubtypeCheckInfo(ClassPtr klass, const SubtypeCheckInfo& new_sci)
        REQUIRES(Locks::subtype_check_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckBits new_bits = new_sci.GetSubtypeCheckBits();
    WriteSubtypeCheckBits(klass, new_bits);
  }

  // Tests can inherit this class. Normal code should use static methods.
  SubtypeCheck() = default;
  SubtypeCheck(const SubtypeCheck& other) = default;
  SubtypeCheck(SubtypeCheck&& other) = default;
  ~SubtypeCheck() = default;

  friend struct MockSubtypeCheck;
};

}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_H_
