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

#ifndef ART_COMPILER_OPTIMIZING_LOCATIONS_H_
#define ART_COMPILER_OPTIMIZING_LOCATIONS_H_

#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "base/bit_vector.h"
#include "base/value_object.h"

namespace art {

class HConstant;
class HInstruction;
class Location;

std::ostream& operator<<(std::ostream& os, const Location& location);

/**
 * A Location is an abstraction over the potential location
 * of an instruction. It could be in register or stack.
 */
class Location : public ValueObject {
 public:
  enum OutputOverlap {
    // The liveness of the output overlaps the liveness of one or
    // several input(s); the register allocator cannot reuse an
    // input's location for the output's location.
    kOutputOverlap,
    // The liveness of the output does not overlap the liveness of any
    // input; the register allocator is allowed to reuse an input's
    // location for the output's location.
    kNoOutputOverlap
  };

  enum Kind {
    kInvalid = 0,
    kConstant = 1,
    kStackSlot = 2,  // 32bit stack slot.
    kDoubleStackSlot = 3,  // 64bit stack slot.

    kRegister = 4,  // Core register.

    // We do not use the value 5 because it conflicts with kLocationConstantMask.
    kDoNotUse5 = 5,

    kFpuRegister = 6,  // Float register.

    kRegisterPair = 7,  // Long register.

    kFpuRegisterPair = 8,  // Double register.

    // We do not use the value 9 because it conflicts with kLocationConstantMask.
    kDoNotUse9 = 9,

    kSIMDStackSlot = 10,  // 128bit stack slot. TODO: generalize with encoded #bytes?

    // Unallocated location represents a location that is not fixed and can be
    // allocated by a register allocator.  Each unallocated location has
    // a policy that specifies what kind of location is suitable. Payload
    // contains register allocation policy.
    kUnallocated = 11,
  };

  Location() : ValueObject(), value_(kInvalid) {
    // Verify that non-constant location kinds do not interfere with kConstant.
    static_assert((kInvalid & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kUnallocated & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kStackSlot & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kDoubleStackSlot & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kSIMDStackSlot & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kRegister & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kFpuRegister & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kRegisterPair & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kFpuRegisterPair & kLocationConstantMask) != kConstant, "TagError");
    static_assert((kConstant & kLocationConstantMask) == kConstant, "TagError");

    DCHECK(!IsValid());
  }

  Location(const Location& other) = default;

  Location& operator=(const Location& other) = default;

  bool IsConstant() const {
    return (value_ & kLocationConstantMask) == kConstant;
  }

  static Location ConstantLocation(HConstant* constant) {
    DCHECK(constant != nullptr);
    return Location(kConstant | reinterpret_cast<uintptr_t>(constant));
  }

  HConstant* GetConstant() const {
    DCHECK(IsConstant());
    return reinterpret_cast<HConstant*>(value_ & ~kLocationConstantMask);
  }

  bool IsValid() const {
    return value_ != kInvalid;
  }

  bool IsInvalid() const {
    return !IsValid();
  }

  // Empty location. Used if there the location should be ignored.
  static Location NoLocation() {
    return Location();
  }

  // Register locations.
  static Location RegisterLocation(int reg) {
    return Location(kRegister, reg);
  }

  static Location FpuRegisterLocation(int reg) {
    return Location(kFpuRegister, reg);
  }

  static Location RegisterPairLocation(int low, int high) {
    return Location(kRegisterPair, low << 16 | high);
  }

  static Location FpuRegisterPairLocation(int low, int high) {
    return Location(kFpuRegisterPair, low << 16 | high);
  }

  bool IsRegister() const {
    return GetKind() == kRegister;
  }

  bool IsFpuRegister() const {
    return GetKind() == kFpuRegister;
  }

  bool IsRegisterPair() const {
    return GetKind() == kRegisterPair;
  }

  bool IsFpuRegisterPair() const {
    return GetKind() == kFpuRegisterPair;
  }

  bool IsRegisterKind() const {
    return IsRegister() || IsFpuRegister() || IsRegisterPair() || IsFpuRegisterPair();
  }

  int reg() const {
    DCHECK(IsRegister() || IsFpuRegister());
    return GetPayload();
  }

  int low() const {
    DCHECK(IsPair());
    return GetPayload() >> 16;
  }

  int high() const {
    DCHECK(IsPair());
    return GetPayload() & 0xFFFF;
  }

  template <typename T>
  T AsRegister() const {
    DCHECK(IsRegister());
    return static_cast<T>(reg());
  }

  template <typename T>
  T AsFpuRegister() const {
    DCHECK(IsFpuRegister());
    return static_cast<T>(reg());
  }

  template <typename T>
  T AsRegisterPairLow() const {
    DCHECK(IsRegisterPair());
    return static_cast<T>(low());
  }

  template <typename T>
  T AsRegisterPairHigh() const {
    DCHECK(IsRegisterPair());
    return static_cast<T>(high());
  }

  template <typename T>
  T AsFpuRegisterPairLow() const {
    DCHECK(IsFpuRegisterPair());
    return static_cast<T>(low());
  }

  template <typename T>
  T AsFpuRegisterPairHigh() const {
    DCHECK(IsFpuRegisterPair());
    return static_cast<T>(high());
  }

  bool IsPair() const {
    return IsRegisterPair() || IsFpuRegisterPair();
  }

  Location ToLow() const {
    if (IsRegisterPair()) {
      return Location::RegisterLocation(low());
    } else if (IsFpuRegisterPair()) {
      return Location::FpuRegisterLocation(low());
    } else {
      DCHECK(IsDoubleStackSlot());
      return Location::StackSlot(GetStackIndex());
    }
  }

  Location ToHigh() const {
    if (IsRegisterPair()) {
      return Location::RegisterLocation(high());
    } else if (IsFpuRegisterPair()) {
      return Location::FpuRegisterLocation(high());
    } else {
      DCHECK(IsDoubleStackSlot());
      return Location::StackSlot(GetHighStackIndex(4));
    }
  }

  static uintptr_t EncodeStackIndex(intptr_t stack_index) {
    DCHECK(-kStackIndexBias <= stack_index);
    DCHECK(stack_index < kStackIndexBias);
    return static_cast<uintptr_t>(kStackIndexBias + stack_index);
  }

  static Location StackSlot(intptr_t stack_index) {
    uintptr_t payload = EncodeStackIndex(stack_index);
    Location loc(kStackSlot, payload);
    // Ensure that sign is preserved.
    DCHECK_EQ(loc.GetStackIndex(), stack_index);
    return loc;
  }

  bool IsStackSlot() const {
    return GetKind() == kStackSlot;
  }

  static Location DoubleStackSlot(intptr_t stack_index) {
    uintptr_t payload = EncodeStackIndex(stack_index);
    Location loc(kDoubleStackSlot, payload);
    // Ensure that sign is preserved.
    DCHECK_EQ(loc.GetStackIndex(), stack_index);
    return loc;
  }

  bool IsDoubleStackSlot() const {
    return GetKind() == kDoubleStackSlot;
  }

  static Location SIMDStackSlot(intptr_t stack_index) {
    uintptr_t payload = EncodeStackIndex(stack_index);
    Location loc(kSIMDStackSlot, payload);
    // Ensure that sign is preserved.
    DCHECK_EQ(loc.GetStackIndex(), stack_index);
    return loc;
  }

  bool IsSIMDStackSlot() const {
    return GetKind() == kSIMDStackSlot;
  }

  intptr_t GetStackIndex() const {
    DCHECK(IsStackSlot() || IsDoubleStackSlot() || IsSIMDStackSlot());
    // Decode stack index manually to preserve sign.
    return GetPayload() - kStackIndexBias;
  }

  intptr_t GetHighStackIndex(uintptr_t word_size) const {
    DCHECK(IsDoubleStackSlot());
    // Decode stack index manually to preserve sign.
    return GetPayload() - kStackIndexBias + word_size;
  }

  Kind GetKind() const {
    return IsConstant() ? kConstant : KindField::Decode(value_);
  }

  bool Equals(Location other) const {
    return value_ == other.value_;
  }

  bool Contains(Location other) const {
    if (Equals(other)) {
      return true;
    } else if (IsPair() || IsDoubleStackSlot()) {
      return ToLow().Equals(other) || ToHigh().Equals(other);
    }
    return false;
  }

  bool OverlapsWith(Location other) const {
    // Only check the overlapping case that can happen with our register allocation algorithm.
    bool overlap = Contains(other) || other.Contains(*this);
    if (kIsDebugBuild && !overlap) {
      // Note: These are also overlapping cases. But we are not able to handle them in
      // ParallelMoveResolverWithSwap. Make sure that we do not meet such case with our compiler.
      if ((IsPair() && other.IsPair()) || (IsDoubleStackSlot() && other.IsDoubleStackSlot())) {
        DCHECK(!Contains(other.ToLow()));
        DCHECK(!Contains(other.ToHigh()));
      }
    }
    return overlap;
  }

  const char* DebugString() const {
    switch (GetKind()) {
      case kInvalid: return "I";
      case kRegister: return "R";
      case kStackSlot: return "S";
      case kDoubleStackSlot: return "DS";
      case kSIMDStackSlot: return "SIMD";
      case kUnallocated: return "U";
      case kConstant: return "C";
      case kFpuRegister: return "F";
      case kRegisterPair: return "RP";
      case kFpuRegisterPair: return "FP";
      case kDoNotUse5:  // fall-through
      case kDoNotUse9:
        LOG(FATAL) << "Should not use this location kind";
    }
    UNREACHABLE();
  }

  // Unallocated locations.
  enum Policy {
    kAny,
    kRequiresRegister,
    kRequiresFpuRegister,
    kSameAsFirstInput,
  };

  bool IsUnallocated() const {
    return GetKind() == kUnallocated;
  }

  static Location UnallocatedLocation(Policy policy) {
    return Location(kUnallocated, PolicyField::Encode(policy));
  }

  // Any free register is suitable to replace this unallocated location.
  static Location Any() {
    return UnallocatedLocation(kAny);
  }

  static Location RequiresRegister() {
    return UnallocatedLocation(kRequiresRegister);
  }

  static Location RequiresFpuRegister() {
    return UnallocatedLocation(kRequiresFpuRegister);
  }

  static Location RegisterOrConstant(HInstruction* instruction);
  static Location RegisterOrInt32Constant(HInstruction* instruction);
  static Location ByteRegisterOrConstant(int reg, HInstruction* instruction);
  static Location FpuRegisterOrConstant(HInstruction* instruction);
  static Location FpuRegisterOrInt32Constant(HInstruction* instruction);

  // The location of the first input to the instruction will be
  // used to replace this unallocated location.
  static Location SameAsFirstInput() {
    return UnallocatedLocation(kSameAsFirstInput);
  }

  Policy GetPolicy() const {
    DCHECK(IsUnallocated());
    return PolicyField::Decode(GetPayload());
  }

  bool RequiresRegisterKind() const {
    return GetPolicy() == kRequiresRegister || GetPolicy() == kRequiresFpuRegister;
  }

  uintptr_t GetEncoding() const {
    return GetPayload();
  }

 private:
  // Number of bits required to encode Kind value.
  static constexpr uint32_t kBitsForKind = 4;
  static constexpr uint32_t kBitsForPayload = kBitsPerIntPtrT - kBitsForKind;
  static constexpr uintptr_t kLocationConstantMask = 0x3;

  explicit Location(uintptr_t value) : value_(value) {}

  Location(Kind kind, uintptr_t payload)
      : value_(KindField::Encode(kind) | PayloadField::Encode(payload)) {}

  uintptr_t GetPayload() const {
    return PayloadField::Decode(value_);
  }

  typedef BitField<Kind, 0, kBitsForKind> KindField;
  typedef BitField<uintptr_t, kBitsForKind, kBitsForPayload> PayloadField;

  // Layout for kUnallocated locations payload.
  typedef BitField<Policy, 0, 3> PolicyField;

  // Layout for stack slots.
  static const intptr_t kStackIndexBias =
      static_cast<intptr_t>(1) << (kBitsForPayload - 1);

  // Location either contains kind and payload fields or a tagged handle for
  // a constant locations. Values of enumeration Kind are selected in such a
  // way that none of them can be interpreted as a kConstant tag.
  uintptr_t value_;
};
std::ostream& operator<<(std::ostream& os, const Location::Kind& rhs);
std::ostream& operator<<(std::ostream& os, const Location::Policy& rhs);

class RegisterSet : public ValueObject {
 public:
  static RegisterSet Empty() { return RegisterSet(); }
  static RegisterSet AllFpu() { return RegisterSet(0, -1); }

  void Add(Location loc) {
    if (loc.IsRegister()) {
      core_registers_ |= (1 << loc.reg());
    } else {
      DCHECK(loc.IsFpuRegister());
      floating_point_registers_ |= (1 << loc.reg());
    }
  }

  void Remove(Location loc) {
    if (loc.IsRegister()) {
      core_registers_ &= ~(1 << loc.reg());
    } else {
      DCHECK(loc.IsFpuRegister()) << loc;
      floating_point_registers_ &= ~(1 << loc.reg());
    }
  }

  bool ContainsCoreRegister(uint32_t id) const {
    return Contains(core_registers_, id);
  }

  bool ContainsFloatingPointRegister(uint32_t id) const {
    return Contains(floating_point_registers_, id);
  }

  static bool Contains(uint32_t register_set, uint32_t reg) {
    return (register_set & (1 << reg)) != 0;
  }

  size_t GetNumberOfRegisters() const {
    return POPCOUNT(core_registers_) + POPCOUNT(floating_point_registers_);
  }

  uint32_t GetCoreRegisters() const {
    return core_registers_;
  }

  uint32_t GetFloatingPointRegisters() const {
    return floating_point_registers_;
  }

 private:
  RegisterSet() : core_registers_(0), floating_point_registers_(0) {}
  RegisterSet(uint32_t core, uint32_t fp) : core_registers_(core), floating_point_registers_(fp) {}

  uint32_t core_registers_;
  uint32_t floating_point_registers_;
};

static constexpr bool kIntrinsified = true;

/**
 * The code generator computes LocationSummary for each instruction so that
 * the instruction itself knows what code to generate: where to find the inputs
 * and where to place the result.
 *
 * The intent is to have the code for generating the instruction independent of
 * register allocation. A register allocator just has to provide a LocationSummary.
 */
class LocationSummary : public ArenaObject<kArenaAllocLocationSummary> {
 public:
  enum CallKind {
    kNoCall,
    kCallOnMainAndSlowPath,
    kCallOnSlowPath,
    kCallOnMainOnly
  };

  explicit LocationSummary(HInstruction* instruction,
                           CallKind call_kind = kNoCall,
                           bool intrinsified = false);

  void SetInAt(uint32_t at, Location location) {
    inputs_[at] = location;
  }

  Location InAt(uint32_t at) const {
    return inputs_[at];
  }

  size_t GetInputCount() const {
    return inputs_.size();
  }

  // Set the output location.  Argument `overlaps` tells whether the
  // output overlaps any of the inputs (if so, it cannot share the
  // same register as one of the inputs); it is set to
  // `Location::kOutputOverlap` by default for safety.
  void SetOut(Location location, Location::OutputOverlap overlaps = Location::kOutputOverlap) {
    DCHECK(output_.IsInvalid());
    output_overlaps_ = overlaps;
    output_ = location;
  }

  void UpdateOut(Location location) {
    // There are two reasons for updating an output:
    // 1) Parameters, where we only know the exact stack slot after
    //    doing full register allocation.
    // 2) Unallocated location.
    DCHECK(output_.IsStackSlot() || output_.IsDoubleStackSlot() || output_.IsUnallocated());
    output_ = location;
  }

  void AddTemp(Location location) {
    temps_.push_back(location);
  }

  void AddRegisterTemps(size_t count) {
    for (size_t i = 0; i < count; ++i) {
      AddTemp(Location::RequiresRegister());
    }
  }

  Location GetTemp(uint32_t at) const {
    return temps_[at];
  }

  void SetTempAt(uint32_t at, Location location) {
    DCHECK(temps_[at].IsUnallocated() || temps_[at].IsInvalid());
    temps_[at] = location;
  }

  size_t GetTempCount() const {
    return temps_.size();
  }

  bool HasTemps() const { return !temps_.empty(); }

  Location Out() const { return output_; }

  bool CanCall() const {
    return call_kind_ != kNoCall;
  }

  bool WillCall() const {
    return call_kind_ == kCallOnMainOnly || call_kind_ == kCallOnMainAndSlowPath;
  }

  bool CallsOnSlowPath() const {
    return call_kind_ == kCallOnSlowPath || call_kind_ == kCallOnMainAndSlowPath;
  }

  bool OnlyCallsOnSlowPath() const {
    return call_kind_ == kCallOnSlowPath;
  }

  bool CallsOnMainAndSlowPath() const {
    return call_kind_ == kCallOnMainAndSlowPath;
  }

  bool NeedsSafepoint() const {
    return CanCall();
  }

  void SetCustomSlowPathCallerSaves(const RegisterSet& caller_saves) {
    DCHECK(OnlyCallsOnSlowPath());
    has_custom_slow_path_calling_convention_ = true;
    custom_slow_path_caller_saves_ = caller_saves;
  }

  bool HasCustomSlowPathCallingConvention() const {
    return has_custom_slow_path_calling_convention_;
  }

  const RegisterSet& GetCustomSlowPathCallerSaves() const {
    DCHECK(HasCustomSlowPathCallingConvention());
    return custom_slow_path_caller_saves_;
  }

  void SetStackBit(uint32_t index) {
    stack_mask_->SetBit(index);
  }

  void ClearStackBit(uint32_t index) {
    stack_mask_->ClearBit(index);
  }

  void SetRegisterBit(uint32_t reg_id) {
    register_mask_ |= (1 << reg_id);
  }

  uint32_t GetRegisterMask() const {
    return register_mask_;
  }

  bool RegisterContainsObject(uint32_t reg_id) {
    return RegisterSet::Contains(register_mask_, reg_id);
  }

  void AddLiveRegister(Location location) {
    live_registers_.Add(location);
  }

  BitVector* GetStackMask() const {
    return stack_mask_;
  }

  RegisterSet* GetLiveRegisters() {
    return &live_registers_;
  }

  size_t GetNumberOfLiveRegisters() const {
    return live_registers_.GetNumberOfRegisters();
  }

  bool OutputUsesSameAs(uint32_t input_index) const {
    return (input_index == 0)
        && output_.IsUnallocated()
        && (output_.GetPolicy() == Location::kSameAsFirstInput);
  }

  bool IsFixedInput(uint32_t input_index) const {
    Location input = inputs_[input_index];
    return input.IsRegister()
        || input.IsFpuRegister()
        || input.IsPair()
        || input.IsStackSlot()
        || input.IsDoubleStackSlot();
  }

  bool OutputCanOverlapWithInputs() const {
    return output_overlaps_ == Location::kOutputOverlap;
  }

  bool Intrinsified() const {
    return intrinsified_;
  }

 private:
  LocationSummary(HInstruction* instruction,
                  CallKind call_kind,
                  bool intrinsified,
                  ArenaAllocator* allocator);

  ArenaVector<Location> inputs_;
  ArenaVector<Location> temps_;
  const CallKind call_kind_;
  // Whether these are locations for an intrinsified call.
  const bool intrinsified_;
  // Whether the slow path has default or custom calling convention.
  bool has_custom_slow_path_calling_convention_;
  // Whether the output overlaps with any of the inputs. If it overlaps, then it cannot
  // share the same register as the inputs.
  Location::OutputOverlap output_overlaps_;
  Location output_;

  // Mask of objects that live in the stack.
  BitVector* stack_mask_;

  // Mask of objects that live in register.
  uint32_t register_mask_;

  // Registers that are in use at this position.
  RegisterSet live_registers_;

  // Custom slow path caller saves. Valid only if indicated by slow_path_calling_convention_.
  RegisterSet custom_slow_path_caller_saves_;

  friend class RegisterAllocatorTest;
  DISALLOW_COPY_AND_ASSIGN(LocationSummary);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LOCATIONS_H_
