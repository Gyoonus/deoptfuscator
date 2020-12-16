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

#ifndef ART_LIBARTBASE_BASE_BIT_STRING_H_
#define ART_LIBARTBASE_BASE_BIT_STRING_H_

#include "base/bit_struct.h"
#include "base/bit_utils.h"

#include <ostream>

namespace art {

struct BitStringChar;
inline std::ostream& operator<<(std::ostream& os, const BitStringChar& bc);

/**
 * A BitStringChar is a light-weight wrapper to read/write a single character
 * into a BitString, while restricting the bitlength.
 *
 * This is only intended for reading/writing into temporaries, as the representation is
 * inefficient for memory (it uses a word for the character and another word for the bitlength).
 *
 * See also BitString below.
 */
struct BitStringChar {
  using StorageType = uint32_t;
  static_assert(std::is_unsigned<StorageType>::value, "BitStringChar::StorageType must be unsigned");

  // BitStringChars are always zero-initialized by default. Equivalent to BitStringChar(0,0).
  BitStringChar() : data_(0u), bitlength_(0u) { }

  // Create a new BitStringChar whose data bits can be at most bitlength.
  BitStringChar(StorageType data, size_t bitlength)
      : data_(data), bitlength_(bitlength) {
    // All bits higher than bitlength must be set to 0.
    DCHECK_EQ(0u, data & ~MaskLeastSignificant(bitlength_))
        << "BitStringChar data out of range, data: " << data << ", bitlength: " << bitlength;
  }

  // What is the bitlength constraint for this character?
  // (Data could use less bits, but this is the maximum bit capacity at that BitString position).
  size_t GetBitLength() const {
    return bitlength_;
  }

  // Is there any capacity in this BitStringChar to store any data?
  bool IsEmpty() const {
    return bitlength_ == 0;
  }

  explicit operator StorageType() const {
    return data_;
  }

  bool operator==(StorageType storage) const {
    return data_ == storage;
  }

  bool operator!=(StorageType storage) const {
    return !(*this == storage);
  }

  // Compare equality against another BitStringChar. Note: bitlength is ignored.
  bool operator==(const BitStringChar& other) const {
    return data_ == other.data_;
  }

  // Compare non-equality against another BitStringChar. Note: bitlength is ignored.
  bool operator!=(const BitStringChar& other) const {
    return !(*this == other);
  }

  // Add a BitStringChar with an integer. The resulting BitStringChar's data must still fit within
  // this BitStringChar's bit length.
  BitStringChar operator+(StorageType storage) const {
    DCHECK_LE(storage, MaximumValue().data_ - data_) << "Addition would overflow " << *this;
    return BitStringChar(data_ + storage, bitlength_);
  }

  // Get the maximum representible value with the same bitlength.
  // (Useful to figure out the maximum value for this BitString position.)
  BitStringChar MaximumValue() const {
    StorageType maximimum_data = MaxInt<StorageType>(bitlength_);
    return BitStringChar(maximimum_data, bitlength_);
  }

 private:
  StorageType data_;  // Unused bits (outside of bitlength) are 0.
  size_t bitlength_;
  // Logically const. Physically non-const so operator= still works.
};

// Print e.g. "BitStringChar<10>(123)" where 10=bitlength, 123=data.
inline std::ostream& operator<<(std::ostream& os, const BitStringChar& bc) {
  os << "BitStringChar<" << bc.GetBitLength() << ">("
     << static_cast<BitStringChar::StorageType>(bc) << ")";
  return os;
}

/**
 *                           BitString
 *
 * MSB (most significant bit)                                LSB
 *  +------------+-----+------------+------------+------------+
 *  |            |     |            |            |            |
 *  |   CharN    | ... |    Char2   |   Char1    |   Char0    |
 *  |            |     |            |            |            |
 *  +------------+-----+------------+------------+------------+
 *   <- len[N] ->  ...  <- len[2] -> <- len[1] -> <- len[0] ->
 *
 * Stores up to "N+1" characters in a subset of a machine word. Each character has a different
 * bitlength, as defined by len[pos]. This BitString can be nested inside of a BitStruct
 * (see e.g. SubtypeCheckBitsAndStatus).
 *
 * Definitions:
 *
 *  "ABCDE...K"       := [A,B,C,D,E, ... K] + [0]*(N-idx(K)) s.t. N >= K.
 *                    // Padded with trailing 0s to fit (N+1) bitstring chars.
 *  MaxBitstringLen   := N+1
 *  StrLen(Bitstring) := I s.t. (I == 0 OR Char(I-1) != 0)
 *                              AND forall char in CharI..CharN : char == 0
 *                    // = Maximum length - the # of consecutive trailing zeroes.
 *  Bitstring[N]      := CharN
 *  Bitstring[I..N)   := [CharI, CharI+1, ... CharN-1]
 *
 * (These are used by the SubtypeCheckInfo definitions and invariants, see subtype_check_info.h)
 */
struct BitString {
  using StorageType = BitStringChar::StorageType;

  // As this is meant to be used only with "SubtypeCheckInfo",
  // the bitlengths and the maximum string length is tuned by maximizing the coverage of "Assigned"
  // bitstrings for instance-of and check-cast targets during Optimizing compilation.
  static constexpr size_t kBitSizeAtPosition[] = {12, 4, 11};         // len[] from header docs.
  static constexpr size_t kCapacity = arraysize(kBitSizeAtPosition);  // MaxBitstringLen above.

  // How many bits are needed to represent BitString[0..position)?
  static constexpr size_t GetBitLengthTotalAtPosition(size_t position) {
    size_t idx = 0;
    size_t sum = 0;
    while (idx < position && idx < kCapacity) {
      sum += kBitSizeAtPosition[idx];
      ++idx;
    }
    // TODO: precompute with CreateArray helper.

    return sum;
  }

  // What is the least-significant-bit for a position?
  // (e.g. to use with BitField{Insert,Extract,Clear}.)
  static constexpr size_t GetLsbForPosition(size_t position) {
    DCHECK_GE(kCapacity, position);
    return GetBitLengthTotalAtPosition(position);
  }

  // How many bits are needed for a BitStringChar at the position?
  // Returns 0 if the position is out of range.
  static constexpr size_t MaybeGetBitLengthAtPosition(size_t position) {
    if (position >= kCapacity) {
      return 0;
    }
    return kBitSizeAtPosition[position];
  }

  // Read a bitchar at some index within the capacity.
  // See also "BitString[N]" in the doc header.
  BitStringChar operator[](size_t idx) const {
    DCHECK_LT(idx, kCapacity);

    StorageType data = BitFieldExtract(storage_, GetLsbForPosition(idx), kBitSizeAtPosition[idx]);

    return BitStringChar(data, kBitSizeAtPosition[idx]);
  }

  // Overwrite a bitchar at a position with a new one.
  //
  // The `bitchar` bitlength must be no more than the maximum bitlength for that position.
  void SetAt(size_t idx, BitStringChar bitchar) {
    DCHECK_LT(idx, kCapacity);
    DCHECK_LE(bitchar.GetBitLength(), kBitSizeAtPosition[idx]);

    // Read the bitchar: Bits > bitlength in bitchar are defined to be 0.
    storage_ = BitFieldInsert(storage_,
                              static_cast<StorageType>(bitchar),
                              GetLsbForPosition(idx),
                              kBitSizeAtPosition[idx]);
  }

  // How many characters are there in this bitstring?
  // Trailing 0s are ignored, but 0s in-between are counted.
  // See also "StrLen(BitString)" in the doc header.
  size_t Length() const {
    size_t num_trailing_zeros = 0;
    size_t i;
    for (i = kCapacity - 1u; ; --i) {
      BitStringChar bc = (*this)[i];
      if (bc != 0u) {
        break;  // Found first trailing non-zero.
      }

      ++num_trailing_zeros;
      if (i == 0u) {
        break;  // No more bitchars remaining: don't underflow.
      }
    }

    return kCapacity - num_trailing_zeros;
  }

  // Cast to the underlying integral storage type.
  explicit operator StorageType() const {
    return storage_;
  }

  // Get the # of bits this would use if it was nested inside of a BitStruct.
  static constexpr size_t BitStructSizeOf() {
    return GetBitLengthTotalAtPosition(kCapacity);
  }

  BitString() = default;

  // Efficient O(1) comparison: Equal if both bitstring words are the same.
  bool operator==(const BitString& other) const {
    return storage_ == other.storage_;
  }

  // Efficient O(1) negative comparison: Not-equal if both bitstring words are different.
  bool operator!=(const BitString& other) const {
    return !(*this == other);
  }

  // Does this bitstring contain exactly 0 characters?
  bool IsEmpty() const {
    return (*this) == BitString{};
  }

  // Remove all BitStringChars starting at end.
  // Returns the BitString[0..end) substring as a copy.
  // See also "BitString[I..N)" in the doc header.
  BitString Truncate(size_t end) {
    DCHECK_GE(kCapacity, end);
    BitString copy = *this;

    if (end < kCapacity) {
      size_t lsb = GetLsbForPosition(end);
      size_t bit_size = GetLsbForPosition(kCapacity) - lsb;
      StorageType data = BitFieldClear(copy.storage_, lsb, bit_size);
      copy.storage_ = data;
    }

    return copy;
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const BitString& bit_string);

  // Data is stored with the first character in the least-significant-bit.
  // Unused bits are zero.
  StorageType storage_;
};

static_assert(BitSizeOf<BitString::StorageType>() >=
                  BitString::GetBitLengthTotalAtPosition(BitString::kCapacity),
              "Storage type is too small for the # of bits requested");

// Print e.g. "BitString[1,0,3]". Trailing 0s are dropped.
inline std::ostream& operator<<(std::ostream& os, const BitString& bit_string) {
  const size_t length = bit_string.Length();

  os << "BitString[";
  for (size_t i = 0; i < length; ++i) {
    BitStringChar bc = bit_string[i];
    if (i != 0) {
      os << ",";
    }
    os << static_cast<BitString::StorageType>(bc);
  }
  os << "]";
  return os;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_STRING_H_
