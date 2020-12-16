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

#ifndef ART_LIBARTBASE_BASE_BIT_STRUCT_H_
#define ART_LIBARTBASE_BASE_BIT_STRUCT_H_

#include "base/bit_utils.h"
#include "bit_struct_detail.h"

//
// Zero-cost, type-safe, well-defined "structs" of bit fields.
//
// ---------------------------------------------
// Usage example:
// ---------------------------------------------
//
//   // Definition for type 'Example'
//   BITSTRUCT_DEFINE_START(Example, 10)
//     BitStructUint<0, 2> u2;     // Every field must be a BitStruct[*].
//     BitStructInt<2, 7>  i7;
//     BitStructUint<9, 1> i1;
//   BITSTRUCT_DEFINE_END(Example);
//
//  Would define a bit struct with this layout:
//   <- 1 ->    <--  7  -->  <- 2 ->
//  +--------+---------------+-----+
//  |   i1   |       i7      | u2  +
//  +--------+---------------+-----+
//  10       9               2     0
//
//   // Read-write just like regular values.
//   Example ex;
//   ex.u2 = 3;
//   ex.i7 = -25;
//   ex.i1 = true;
//   size_t u2 = ex.u2;
//   int i7 = ex.i7;
//   bool i1 = ex.i1;
//
//   // It's packed down to the smallest # of machine words.
//   assert(sizeof(Example) == 2);
//   // The exact bit pattern is well-defined by the template parameters.
//   uint16_t cast = *reinterpret_cast<uint16_t*>(ex);
//   assert(cast == ((3) | (0b100111 << 2) | (true << 9);
//
// ---------------------------------------------
// Why not just use C++ bitfields?
// ---------------------------------------------
//
// The layout is implementation-defined.
// We do not know whether the fields are packed left-to-right or
// right-to-left, so it makes it useless when the memory layout needs to be
// precisely controlled.
//
// ---------------------------------------------
// More info:
// ---------------------------------------------
// Currently uintmax_t is the largest supported underlying storage type,
// all (kBitOffset + kBitWidth) must fit into BitSizeOf<uintmax_t>();
//
// Using BitStruct[U]int will automatically select an underlying type
// that's the smallest to fit your (offset + bitwidth).
//
// BitStructNumber can be used to manually select an underlying type.
//
// BitStructField can be used with custom standard-layout structs,
// thus allowing for arbitrary nesting of bit structs.
//
namespace art {
// Zero-cost wrapper around a struct 'T', allowing it to be stored as a bitfield
// at offset 'kBitOffset' and width 'kBitWidth'.
// The storage is plain unsigned int, whose size is the smallest required  to fit
// 'kBitOffset + kBitWidth'. All operations to this become BitFieldExtract/BitFieldInsert
// operations to the underlying uint.
//
// Field memory representation:
//
// MSB      <-- width  -->      LSB
// +--------+------------+--------+
// | ?????? | u bitfield | ?????? +
// +--------+------------+--------+
//                       offset   0
//
// Reading/writing the bitfield (un)packs it into a temporary T:
//
// MSB               <-- width  --> LSB
// +-----------------+------------+
// | 0.............0 | T bitfield |
// +-----------------+------------+
//                                0
//
// It's the responsibility of the StorageType to ensure the bit representation
// of T can be represented by kBitWidth.
template <typename T,
          size_t kBitOffset,
          size_t kBitWidth = BitStructSizeOf<T>(),
          typename StorageType = typename detail::MinimumTypeUnsignedHelper<kBitOffset + kBitWidth>::type>
struct BitStructField {
  static_assert(std::is_standard_layout<T>::value, "T must be standard layout");

  operator T() const {
    return Get();
  }

  // Exclude overload when T==StorageType.
  template <typename _ = void,
            typename = std::enable_if_t<std::is_same<T, StorageType>::value, _>>
  explicit operator StorageType() const {
    return GetStorage();
  }

  BitStructField& operator=(T value) {
    return Assign(*this, value);
  }

  static constexpr size_t BitStructSizeOf() {
    return kBitWidth;
  }

  BitStructField& operator=(const BitStructField& other) {
    // Warning. The default operator= will overwrite the entire storage!
    return *this = static_cast<T>(other);
  }

  BitStructField(const BitStructField& other) {
    Assign(*this, static_cast<T>(other));
  }

  BitStructField() = default;
  ~BitStructField() = default;

 protected:
  template <typename T2>
  T2& Assign(T2& what, T value) {
    // Since C++ doesn't allow the type of operator= to change out
    // in the subclass, reimplement operator= in each subclass
    // manually and call this helper function.
    static_assert(std::is_base_of<BitStructField, T2>::value, "T2 must inherit BitStructField");
    what.Set(value);
    return what;
  }

  T Get() const {
    ValueStorage vs;
    vs.pod_.val_ = GetStorage();
    return vs.value_;
  }

  void Set(T value) {
    ValueStorage value_as_storage;
    value_as_storage.value_ = value;

    storage_.pod_.val_ = BitFieldInsert(storage_.pod_.val_,
                                        value_as_storage.pod_.val_,
                                        kBitOffset,
                                        kBitWidth);
  }

 private:
  StorageType GetStorage() const {
    return BitFieldExtract(storage_.pod_.val_, kBitOffset, kBitWidth);
  }

  // Underlying value must be wrapped in a separate standard-layout struct.
  // See below for more details.
  struct PodWrapper {
    StorageType val_;
  };

  union ValueStorage {
    // Safely alias pod_ and value_ together.
    //
    // See C++ 9.5.1 [class.union]:
    // If a standard-layout union contains several standard-layout structs that share a common
    // initial sequence ... it is permitted to inspect the common initial sequence of any of
    // standard-layout struct members.
    PodWrapper pod_;
    T value_;
  } storage_;

  // Future work: In theory almost non-standard layout can be supported here,
  // assuming they don't rely on the address of (this).
  // We just have to use memcpy since the union-aliasing would not work.
};

// Base class for number-like BitStruct fields.
// T is the type to store in as a bit field.
// kBitOffset, kBitWidth define the position and length of the bitfield.
//
// (Common usage should be BitStructInt, BitStructUint -- this
// intermediate template allows a user-defined integer to be used.)
template <typename T, size_t kBitOffset, size_t kBitWidth>
struct BitStructNumber : public BitStructField<T, kBitOffset, kBitWidth, /*StorageType*/T> {
  using StorageType = T;

  BitStructNumber& operator=(T value) {
    return BaseType::Assign(*this, value);
  }

  /*implicit*/ operator T() const {
    return Get();
  }

  explicit operator bool() const {
    return static_cast<bool>(Get());
  }

  BitStructNumber& operator++() {
    *this = Get() + 1u;
    return *this;
  }

  StorageType operator++(int) {
    return Get() + 1u;
  }

  BitStructNumber& operator--() {
    *this = Get() - 1u;
    return *this;
  }

  StorageType operator--(int) {
    return Get() - 1u;
  }

 private:
  using BaseType = BitStructField<T, kBitOffset, kBitWidth, /*StorageType*/T>;
  using BaseType::Get;
};

// Create a BitStruct field which uses the smallest underlying int storage type,
// in order to be large enough to fit (kBitOffset + kBitWidth).
//
// Values are sign-extended when they are read out.
template <size_t kBitOffset, size_t kBitWidth>
using BitStructInt =
    BitStructNumber<typename detail::MinimumTypeHelper<int, kBitOffset + kBitWidth>::type,
                    kBitOffset,
                    kBitWidth>;

// Create a BitStruct field which uses the smallest underlying uint storage type,
// in order to be large enough to fit (kBitOffset + kBitWidth).
//
// Values are zero-extended when they are read out.
template <size_t kBitOffset, size_t kBitWidth>
using BitStructUint =
    BitStructNumber<typename detail::MinimumTypeHelper<unsigned int, kBitOffset + kBitWidth>::type,
                    kBitOffset,
                    kBitWidth>;

// Start a definition for a bitstruct.
// A bitstruct is defined to be a union with a common initial subsequence
// that we call 'DefineBitStructSize<bitwidth>'.
//
// See top of file for usage example.
//
// This marker is required by the C++ standard in order to
// have a "common initial sequence".
//
// See C++ 9.5.1 [class.union]:
// If a standard-layout union contains several standard-layout structs that share a common
// initial sequence ... it is permitted to inspect the common initial sequence of any of
// standard-layout struct members.
#define BITSTRUCT_DEFINE_START(name, bitwidth)                                 \
    union name {                                                               \
      art::detail::DefineBitStructSize<(bitwidth)> _;                          \
      static constexpr size_t BitStructSizeOf() { return (bitwidth); }         \
      name& operator=(const name& other) { _ = other._; return *this; }        \
      name(const name& other) : _(other._) {}                                  \
      name() = default;                                                        \
      ~name() = default;

// End the definition of a bitstruct, and insert a sanity check
// to ensure that the bitstruct did not exceed the specified size.
//
// See top of file for usage example.
#define BITSTRUCT_DEFINE_END(name)                                             \
    };                                                                         \
    static_assert(art::detail::ValidateBitStructSize<name>(),                  \
                  #name "bitsize incorrect: "                                  \
                  "did you insert extra fields that weren't BitStructX, "      \
                  "and does the size match the sum of the field widths?")

// Determine the minimal bit size for a user-defined type T.
// Used by BitStructField to determine how small a custom type is.
template <typename T>
static constexpr size_t BitStructSizeOf() {
  return T::BitStructSizeOf();
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_STRUCT_H_
