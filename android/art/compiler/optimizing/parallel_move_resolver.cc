/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "parallel_move_resolver.h"

#include "base/stl_util.h"
#include "nodes.h"

namespace art {

void ParallelMoveResolver::BuildInitialMoveList(HParallelMove* parallel_move) {
  // Perform a linear sweep of the moves to add them to the initial list of
  // moves to perform, ignoring any move that is redundant (the source is
  // the same as the destination, the destination is ignored and
  // unallocated, or the move was already eliminated).
  for (size_t i = 0; i < parallel_move->NumMoves(); ++i) {
    MoveOperands* move = parallel_move->MoveOperandsAt(i);
    if (!move->IsRedundant()) {
      moves_.push_back(move);
    }
  }
}

void ParallelMoveResolverWithSwap::EmitNativeCode(HParallelMove* parallel_move) {
  DCHECK(moves_.empty());
  // Build up a worklist of moves.
  BuildInitialMoveList(parallel_move);

  // Move stack/stack slot to take advantage of a free register on constrained machines.
  for (size_t i = 0; i < moves_.size(); ++i) {
    const MoveOperands& move = *moves_[i];
    // Ignore constants and moves already eliminated.
    if (move.IsEliminated() || move.GetSource().IsConstant()) {
      continue;
    }

    if ((move.GetSource().IsStackSlot() || move.GetSource().IsDoubleStackSlot()) &&
        (move.GetDestination().IsStackSlot() || move.GetDestination().IsDoubleStackSlot())) {
      PerformMove(i);
    }
  }

  for (size_t i = 0; i < moves_.size(); ++i) {
    const MoveOperands& move = *moves_[i];
    // Skip constants to perform them last.  They don't block other moves
    // and skipping such moves with register destinations keeps those
    // registers free for the whole algorithm.
    if (!move.IsEliminated() && !move.GetSource().IsConstant()) {
      PerformMove(i);
    }
  }

  // Perform the moves with constant sources.
  for (size_t i = 0; i < moves_.size(); ++i) {
    MoveOperands* move = moves_[i];
    if (!move->IsEliminated()) {
      DCHECK(move->GetSource().IsConstant());
      EmitMove(i);
      // Eliminate the move, in case following moves need a scratch register.
      move->Eliminate();
    }
  }

  moves_.clear();
}

Location LowOf(Location location) {
  if (location.IsRegisterPair()) {
    return Location::RegisterLocation(location.low());
  } else if (location.IsFpuRegisterPair()) {
    return Location::FpuRegisterLocation(location.low());
  } else if (location.IsDoubleStackSlot()) {
    return Location::StackSlot(location.GetStackIndex());
  } else {
    return Location::NoLocation();
  }
}

Location HighOf(Location location) {
  if (location.IsRegisterPair()) {
    return Location::RegisterLocation(location.high());
  } else if (location.IsFpuRegisterPair()) {
    return Location::FpuRegisterLocation(location.high());
  } else if (location.IsDoubleStackSlot()) {
    return Location::StackSlot(location.GetHighStackIndex(4));
  } else {
    return Location::NoLocation();
  }
}

// Update the source of `move`, knowing that `updated_location` has been swapped
// with `new_source`. Note that `updated_location` can be a pair, therefore if
// `move` is non-pair, we need to extract which register to use.
static void UpdateSourceOf(MoveOperands* move, Location updated_location, Location new_source) {
  Location source = move->GetSource();
  if (LowOf(updated_location).Equals(source)) {
    move->SetSource(LowOf(new_source));
  } else if (HighOf(updated_location).Equals(source)) {
    move->SetSource(HighOf(new_source));
  } else {
    DCHECK(updated_location.Equals(source)) << updated_location << " " << source;
    move->SetSource(new_source);
  }
}

MoveOperands* ParallelMoveResolverWithSwap::PerformMove(size_t index) {
  // Each call to this function performs a move and deletes it from the move
  // graph.  We first recursively perform any move blocking this one.  We
  // mark a move as "pending" on entry to PerformMove in order to detect
  // cycles in the move graph.  We use operand swaps to resolve cycles,
  // which means that a call to PerformMove could change any source operand
  // in the move graph.

  MoveOperands* move = moves_[index];
  DCHECK(!move->IsPending());
  if (move->IsRedundant()) {
    // Because we swap register pairs first, following, un-pending
    // moves may become redundant.
    move->Eliminate();
    return nullptr;
  }

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved in a stack-allocated local.  Recursion may allow
  // multiple moves to be pending.
  DCHECK(!move->GetSource().IsInvalid());
  Location destination = move->MarkPending();

  // Perform a depth-first traversal of the move graph to resolve
  // dependencies.  Any unperformed, unpending move with a source the same
  // as this one's destination blocks this one so recursively perform all
  // such moves.
  MoveOperands* required_swap = nullptr;
  for (size_t i = 0; i < moves_.size(); ++i) {
    const MoveOperands& other_move = *moves_[i];
    if (other_move.Blocks(destination) && !other_move.IsPending()) {
      // Though PerformMove can change any source operand in the move graph,
      // calling `PerformMove` cannot create a blocking move via a swap
      // (this loop does not miss any).
      // For example, assume there is a non-blocking move with source A
      // and this move is blocked on source B and there is a swap of A and
      // B.  Then A and B must be involved in the same cycle (or they would
      // not be swapped).  Since this move's destination is B and there is
      // only a single incoming edge to an operand, this move must also be
      // involved in the same cycle.  In that case, the blocking move will
      // be created but will be "pending" when we return from PerformMove.
      required_swap = PerformMove(i);

      if (required_swap == move) {
        // If this move is required to swap, we do so without looking
        // at the next moves. Swapping is not blocked by anything, it just
        // updates other moves's source.
        break;
      } else if (required_swap == moves_[i]) {
        // If `other_move` was swapped, we iterate again to find a new
        // potential cycle.
        required_swap = nullptr;
        i = -1;
      } else if (required_swap != nullptr) {
        // A move is required to swap. We walk back the cycle to find the
        // move by just returning from this `PerformMove`.
        moves_[index]->ClearPending(destination);
        return required_swap;
      }
    }
  }

  // We are about to resolve this move and don't need it marked as
  // pending, so restore its destination.
  move->ClearPending(destination);

  // This move's source may have changed due to swaps to resolve cycles and
  // so it may now be the last move in the cycle.  If so remove it.
  if (move->GetSource().Equals(destination)) {
    move->Eliminate();
    DCHECK(required_swap == nullptr);
    return nullptr;
  }

  // The move may be blocked on a (at most one) pending move, in which case
  // we have a cycle.  Search for such a blocking move and perform a swap to
  // resolve it.
  bool do_swap = false;
  if (required_swap != nullptr) {
    DCHECK_EQ(required_swap, move);
    do_swap = true;
  } else {
    for (MoveOperands* other_move : moves_) {
      if (other_move->Blocks(destination)) {
        DCHECK(other_move->IsPending()) << "move=" << *move << " other_move=" << *other_move;
        if (!move->Is64BitMove() && other_move->Is64BitMove()) {
          // We swap 64bits moves before swapping 32bits moves. Go back from the
          // cycle by returning the move that must be swapped.
          return other_move;
        }
        do_swap = true;
        break;
      }
    }
  }

  if (do_swap) {
    EmitSwap(index);
    // Any unperformed (including pending) move with a source of either
    // this move's source or destination needs to have their source
    // changed to reflect the state of affairs after the swap.
    Location source = move->GetSource();
    Location swap_destination = move->GetDestination();
    move->Eliminate();
    for (MoveOperands* other_move : moves_) {
      if (other_move->Blocks(source)) {
        UpdateSourceOf(other_move, source, swap_destination);
      } else if (other_move->Blocks(swap_destination)) {
        UpdateSourceOf(other_move, swap_destination, source);
      }
    }
    // If the swap was required because of a 64bits move in the middle of a cycle,
    // we return the swapped move, so that the caller knows it needs to re-iterate
    // its dependency loop.
    return required_swap;
  } else {
    // This move is not blocked.
    EmitMove(index);
    move->Eliminate();
    DCHECK(required_swap == nullptr);
    return nullptr;
  }
}

bool ParallelMoveResolverWithSwap::IsScratchLocation(Location loc) {
  for (MoveOperands* move : moves_) {
    if (move->Blocks(loc)) {
      return false;
    }
  }

  for (MoveOperands* move : moves_) {
    if (move->GetDestination().Equals(loc)) {
      return true;
    }
  }

  return false;
}

int ParallelMoveResolverWithSwap::AllocateScratchRegister(int blocked,
                                                          int register_count,
                                                          int if_scratch,
                                                          bool* spilled) {
  DCHECK_NE(blocked, if_scratch);
  int scratch = -1;
  for (int reg = 0; reg < register_count; ++reg) {
    if ((blocked != reg) && IsScratchLocation(Location::RegisterLocation(reg))) {
      scratch = reg;
      break;
    }
  }

  if (scratch == -1) {
    *spilled = true;
    scratch = if_scratch;
  } else {
    *spilled = false;
  }

  return scratch;
}


ParallelMoveResolverWithSwap::ScratchRegisterScope::ScratchRegisterScope(
    ParallelMoveResolverWithSwap* resolver, int blocked, int if_scratch, int number_of_registers)
    : resolver_(resolver),
      reg_(kNoRegister),
      spilled_(false) {
  reg_ = resolver_->AllocateScratchRegister(blocked, number_of_registers, if_scratch, &spilled_);

  if (spilled_) {
    resolver->SpillScratch(reg_);
  }
}


ParallelMoveResolverWithSwap::ScratchRegisterScope::~ScratchRegisterScope() {
  if (spilled_) {
    resolver_->RestoreScratch(reg_);
  }
}

void ParallelMoveResolverNoSwap::EmitNativeCode(HParallelMove* parallel_move) {
  DCHECK_EQ(GetNumberOfPendingMoves(), 0u);
  DCHECK(moves_.empty());
  DCHECK(scratches_.empty());

  // Backend dependent initialization.
  PrepareForEmitNativeCode();

  // Build up a worklist of moves.
  BuildInitialMoveList(parallel_move);

  for (size_t i = 0; i < moves_.size(); ++i) {
    const MoveOperands& move = *moves_[i];
    // Skip constants to perform them last. They don't block other moves and
    // skipping such moves with register destinations keeps those registers
    // free for the whole algorithm.
    if (!move.IsEliminated() && !move.GetSource().IsConstant()) {
      PerformMove(i);
    }
  }

  // Perform the moves with constant sources and register destinations with UpdateMoveSource()
  // to reduce the number of literal loads. Stack destinations are skipped since we won't be benefit
  // from changing the constant sources to stack locations.
  for (size_t i = 0; i < moves_.size(); ++i) {
    MoveOperands* move = moves_[i];
    Location destination = move->GetDestination();
    if (!move->IsEliminated() && !destination.IsStackSlot() && !destination.IsDoubleStackSlot()) {
      Location source = move->GetSource();
      EmitMove(i);
      move->Eliminate();
      // This may introduce additional instruction dependency, but reduce number
      // of moves and possible literal loads. For example,
      // Original moves:
      //   1234.5678 -> D0
      //   1234.5678 -> D1
      // Updated moves:
      //   1234.5678 -> D0
      //   D0 -> D1
      UpdateMoveSource(source, destination);
    }
  }

  // Perform the rest of the moves.
  for (size_t i = 0; i < moves_.size(); ++i) {
    MoveOperands* move = moves_[i];
    if (!move->IsEliminated()) {
      EmitMove(i);
      move->Eliminate();
    }
  }

  // All pending moves that we have added for resolve cycles should be performed.
  DCHECK_EQ(GetNumberOfPendingMoves(), 0u);

  // Backend dependent cleanup.
  FinishEmitNativeCode();

  moves_.clear();
  scratches_.clear();
}

Location ParallelMoveResolverNoSwap::GetScratchLocation(Location::Kind kind) {
  for (Location loc : scratches_) {
    if (loc.GetKind() == kind && !IsBlockedByMoves(loc)) {
      return loc;
    }
  }
  for (MoveOperands* move : moves_) {
    Location loc = move->GetDestination();
    if (loc.GetKind() == kind && !IsBlockedByMoves(loc)) {
      return loc;
    }
  }
  return Location::NoLocation();
}

void ParallelMoveResolverNoSwap::AddScratchLocation(Location loc) {
  if (kIsDebugBuild) {
    for (Location scratch : scratches_) {
      CHECK(!loc.Equals(scratch));
    }
  }
  scratches_.push_back(loc);
}

void ParallelMoveResolverNoSwap::RemoveScratchLocation(Location loc) {
  DCHECK(!IsBlockedByMoves(loc));
  for (auto it = scratches_.begin(), end = scratches_.end(); it != end; ++it) {
    if (loc.Equals(*it)) {
      scratches_.erase(it);
      break;
    }
  }
}

void ParallelMoveResolverNoSwap::PerformMove(size_t index) {
  // Each call to this function performs a move and deletes it from the move
  // graph. We first recursively perform any move blocking this one. We mark
  // a move as "pending" on entry to PerformMove in order to detect cycles
  // in the move graph. We use scratch location to resolve cycles, also
  // additional pending moves might be added. After move has been performed,
  // we will update source operand in the move graph to reduce dependencies in
  // the graph.

  MoveOperands* move = moves_[index];
  DCHECK(!move->IsPending());
  DCHECK(!move->IsEliminated());
  if (move->IsRedundant()) {
    // Previous operations on the list of moves have caused this particular move
    // to become a no-op, so we can safely eliminate it. Consider for example
    // (0 -> 1) (1 -> 0) (1 -> 2). There is a cycle (0 -> 1) (1 -> 0), that we will
    // resolve as (1 -> scratch) (0 -> 1) (scratch -> 0). If, by chance, '2' is
    // used as the scratch location, the move (1 -> 2) will occur while resolving
    // the cycle. When that move is emitted, the code will update moves with a '1'
    // as their source to use '2' instead (see `UpdateMoveSource()`. In our example
    // the initial move (1 -> 2) would then become the no-op (2 -> 2) that can be
    // eliminated here.
    move->Eliminate();
    return;
  }

  // Clear this move's destination to indicate a pending move. The actual
  // destination is saved in a stack-allocated local. Recursion may allow
  // multiple moves to be pending.
  DCHECK(!move->GetSource().IsInvalid());
  Location destination = move->MarkPending();

  // Perform a depth-first traversal of the move graph to resolve
  // dependencies. Any unperformed, unpending move with a source the same
  // as this one's destination blocks this one so recursively perform all
  // such moves.
  for (size_t i = 0; i < moves_.size(); ++i) {
    const MoveOperands& other_move = *moves_[i];
    if (other_move.Blocks(destination) && !other_move.IsPending()) {
      PerformMove(i);
    }
  }

  // We are about to resolve this move and don't need it marked as
  // pending, so restore its destination.
  move->ClearPending(destination);

  // No one else should write to the move destination when the it is pending.
  DCHECK(!move->IsRedundant());

  Location source = move->GetSource();
  // The move may be blocked on several pending moves, in case we have a cycle.
  if (IsBlockedByMoves(destination)) {
    // For a cycle like: (A -> B) (B -> C) (C -> A), we change it to following
    // sequence:
    // (C -> scratch)     # Emit right now.
    // (A -> B) (B -> C)  # Unblocked.
    // (scratch -> A)     # Add to pending_moves_, blocked by (A -> B).
    Location::Kind kind = source.GetKind();
    DCHECK_NE(kind, Location::kConstant);
    Location scratch = AllocateScratchLocationFor(kind);
    // We only care about the move size.
    DataType::Type type = move->Is64BitMove() ? DataType::Type::kInt64 : DataType::Type::kInt32;
    // Perform (C -> scratch)
    move->SetDestination(scratch);
    EmitMove(index);
    move->Eliminate();
    UpdateMoveSource(source, scratch);
    // Add (scratch -> A).
    AddPendingMove(scratch, destination, type);
  } else {
    // This move is not blocked.
    EmitMove(index);
    move->Eliminate();
    UpdateMoveSource(source, destination);
  }

  // Moves in the pending list should not block any other moves. But performing
  // unblocked moves in the pending list can free scratch registers, so we do this
  // as early as possible.
  MoveOperands* pending_move;
  while ((pending_move = GetUnblockedPendingMove(source)) != nullptr) {
    Location pending_source = pending_move->GetSource();
    Location pending_destination = pending_move->GetDestination();
    // We do not depend on the pending move index. So just delete the move instead
    // of eliminating it to make the pending list cleaner.
    DeletePendingMove(pending_move);
    move->SetSource(pending_source);
    move->SetDestination(pending_destination);
    EmitMove(index);
    move->Eliminate();
    UpdateMoveSource(pending_source, pending_destination);
    // Free any unblocked locations in the scratch location list.
    // Note: Fetch size() on each iteration because scratches_ can be modified inside the loop.
    // FIXME: If FreeScratchLocation() removes the location from scratches_,
    // we skip the next location. This happens for arm64.
    for (size_t i = 0; i < scratches_.size(); ++i) {
      Location scratch = scratches_[i];
      // Only scratch overlapping with performed move source can be unblocked.
      if (scratch.OverlapsWith(pending_source) && !IsBlockedByMoves(scratch)) {
        FreeScratchLocation(pending_source);
      }
    }
  }
}

void ParallelMoveResolverNoSwap::UpdateMoveSource(Location from, Location to) {
  // This function is used to reduce the dependencies in the graph after
  // (from -> to) has been performed. Since we ensure there is no move with the same
  // destination, (to -> X) cannot be blocked while (from -> X) might still be
  // blocked. Consider for example the moves (0 -> 1) (1 -> 2) (1 -> 3). After
  // (1 -> 2) has been performed, the moves left are (0 -> 1) and (1 -> 3). There is
  // a dependency between the two. If we update the source location from 1 to 2, we
  // will get (0 -> 1) and (2 -> 3). There is no dependency between the two.
  //
  // This is not something we must do, but we can use fewer scratch locations with
  // this trick. For example, we can avoid using additional scratch locations for
  // moves (0 -> 1), (1 -> 2), (1 -> 0).
  for (MoveOperands* move : moves_) {
    if (move->GetSource().Equals(from)) {
      move->SetSource(to);
    }
  }
}

void ParallelMoveResolverNoSwap::AddPendingMove(Location source,
                                                Location destination,
                                                DataType::Type type) {
  pending_moves_.push_back(new (allocator_) MoveOperands(source, destination, type, nullptr));
}

void ParallelMoveResolverNoSwap::DeletePendingMove(MoveOperands* move) {
  RemoveElement(pending_moves_, move);
}

MoveOperands* ParallelMoveResolverNoSwap::GetUnblockedPendingMove(Location loc) {
  for (MoveOperands* move : pending_moves_) {
    Location destination = move->GetDestination();
    // Only moves with destination overlapping with input loc can be unblocked.
    if (destination.OverlapsWith(loc) && !IsBlockedByMoves(destination)) {
      return move;
    }
  }
  return nullptr;
}

bool ParallelMoveResolverNoSwap::IsBlockedByMoves(Location loc) {
  for (MoveOperands* move : pending_moves_) {
    if (move->Blocks(loc)) {
      return true;
    }
  }
  for (MoveOperands* move : moves_) {
    if (move->Blocks(loc)) {
      return true;
    }
  }
  return false;
}

// So far it is only used for debugging purposes to make sure all pending moves
// have been performed.
size_t ParallelMoveResolverNoSwap::GetNumberOfPendingMoves() {
  return pending_moves_.size();
}

}  // namespace art
