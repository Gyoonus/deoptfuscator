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

#ifndef ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_
#define ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_

#include "base/arena_containers.h"
#include "base/value_object.h"
#include "data_type.h"
#include "locations.h"

namespace art {

class HParallelMove;
class MoveOperands;

// Helper classes to resolve a set of parallel moves. Architecture dependent code generator must
// have their own subclass that implements corresponding virtual functions.
class ParallelMoveResolver : public ValueObject {
 public:
  explicit ParallelMoveResolver(ArenaAllocator* allocator)
      : moves_(allocator->Adapter(kArenaAllocParallelMoveResolver)) {
    moves_.reserve(32);
  }
  virtual ~ParallelMoveResolver() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  virtual void EmitNativeCode(HParallelMove* parallel_move) = 0;

 protected:
  // Build the initial list of moves.
  void BuildInitialMoveList(HParallelMove* parallel_move);

  ArenaVector<MoveOperands*> moves_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolver);
};

// This helper class uses swap to resolve dependencies and may emit swap.
class ParallelMoveResolverWithSwap : public ParallelMoveResolver {
 public:
  explicit ParallelMoveResolverWithSwap(ArenaAllocator* allocator)
      : ParallelMoveResolver(allocator) {}
  virtual ~ParallelMoveResolverWithSwap() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  void EmitNativeCode(HParallelMove* parallel_move) OVERRIDE;

 protected:
  class ScratchRegisterScope : public ValueObject {
   public:
    ScratchRegisterScope(ParallelMoveResolverWithSwap* resolver,
                         int blocked,
                         int if_scratch,
                         int number_of_registers);
    ~ScratchRegisterScope();

    int GetRegister() const { return reg_; }
    bool IsSpilled() const { return spilled_; }

   private:
    ParallelMoveResolverWithSwap* resolver_;
    int reg_;
    bool spilled_;
  };

  // Return true if the location can be scratched.
  bool IsScratchLocation(Location loc);

  // Allocate a scratch register for performing a move. The method will try to use
  // a register that is the destination of a move, but that move has not been emitted yet.
  int AllocateScratchRegister(int blocked, int if_scratch, int register_count, bool* spilled);

  // Emit a move.
  virtual void EmitMove(size_t index) = 0;

  // Execute a move by emitting a swap of two operands.
  virtual void EmitSwap(size_t index) = 0;

  virtual void SpillScratch(int reg) = 0;
  virtual void RestoreScratch(int reg) = 0;

  static constexpr int kNoRegister = -1;

 private:
  // Perform the move at the moves_ index in question (possibly requiring
  // other moves to satisfy dependencies).
  //
  // Return whether another move in the dependency cycle needs to swap. This
  // is to handle 64bits swaps:
  // 1) In the case of register pairs, where we want the pair to swap first to avoid
  //    building pairs that are unexpected by the code generator. For example, if
  //    we were to swap R1 with R2, we would need to update all locations using
  //    R2 to R1. So a (R2,R3) pair register could become (R1,R3). We could make
  //    the code generator understand such pairs, but it's easier and cleaner to
  //    just not create such pairs and exchange pairs in priority.
  // 2) Even when the architecture does not have pairs, we must handle 64bits swaps
  //    first. Consider the case: (R0->R1) (R1->S) (S->R0), where 'S' is a single
  //    stack slot. If we end up swapping S and R0, S will only contain the low bits
  //    of R0. If R0->R1 is for a 64bits instruction, R1 will therefore not contain
  //    the right value.
  MoveOperands* PerformMove(size_t index);

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverWithSwap);
};

// This helper class uses additional scratch registers to resolve dependencies. It supports all kind
// of dependency cycles and does not care about the register layout.
class ParallelMoveResolverNoSwap : public ParallelMoveResolver {
 public:
  explicit ParallelMoveResolverNoSwap(ArenaAllocator* allocator)
      : ParallelMoveResolver(allocator),
        scratches_(allocator->Adapter(kArenaAllocParallelMoveResolver)),
        pending_moves_(allocator->Adapter(kArenaAllocParallelMoveResolver)),
        allocator_(allocator) {
    scratches_.reserve(32);
    pending_moves_.reserve(8);
  }
  virtual ~ParallelMoveResolverNoSwap() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  void EmitNativeCode(HParallelMove* parallel_move) OVERRIDE;

 protected:
  // Called at the beginning of EmitNativeCode(). A subclass may put some architecture dependent
  // initialization here.
  virtual void PrepareForEmitNativeCode() = 0;

  // Called at the end of EmitNativeCode(). A subclass may put some architecture dependent cleanup
  // here. All scratch locations will be removed after this call.
  virtual void FinishEmitNativeCode() = 0;

  // Allocate a scratch location to perform a move from input kind of location. A subclass should
  // implement this to get the best fit location. If there is no suitable physical register, it can
  // also return a stack slot.
  virtual Location AllocateScratchLocationFor(Location::Kind kind) = 0;

  // Called after a move which takes a scratch location as source. A subclass can defer the cleanup
  // to FinishEmitNativeCode().
  virtual void FreeScratchLocation(Location loc) = 0;

  // Emit a move.
  virtual void EmitMove(size_t index) = 0;

  // Return a scratch location from the moves which exactly matches the kind.
  // Return Location::NoLocation() if no matching scratch location can be found.
  Location GetScratchLocation(Location::Kind kind);

  // Add a location to the scratch list which can be returned from GetScratchLocation() to resolve
  // dependency cycles.
  void AddScratchLocation(Location loc);

  // Remove a location from the scratch list.
  void RemoveScratchLocation(Location loc);

  // List of scratch locations.
  ArenaVector<Location> scratches_;

 private:
  // Perform the move at the given index in `moves_` (possibly requiring other moves to satisfy
  // dependencies).
  void PerformMove(size_t index);

  void UpdateMoveSource(Location from, Location to);

  void AddPendingMove(Location source, Location destination, DataType::Type type);

  void DeletePendingMove(MoveOperands* move);

  // Find a move that may be unblocked after (loc -> XXX) is performed.
  MoveOperands* GetUnblockedPendingMove(Location loc);

  // Return true if the location is blocked by outstanding moves.
  bool IsBlockedByMoves(Location loc);

  // Return the number of pending moves.
  size_t GetNumberOfPendingMoves();

  // Additional pending moves which might be added to resolve dependency cycle.
  ArenaVector<MoveOperands*> pending_moves_;

  // Used to allocate pending MoveOperands.
  ArenaAllocator* const allocator_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverNoSwap);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_
