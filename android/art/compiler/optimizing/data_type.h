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

#ifndef ART_COMPILER_OPTIMIZING_DATA_TYPE_H_
#define ART_COMPILER_OPTIMIZING_DATA_TYPE_H_

#include <iosfwd>

#include <android-base/logging.h>

#include "base/bit_utils.h"

namespace art {

class DataType {
 public:
  enum class Type : uint8_t {
    kReference = 0,
    kBool,
    kUint8,
    kInt8,
    kUint16,
    kInt16,
    kUint32,
    kInt32,
    kUint64,
    kInt64,
    kFloat32,
    kFloat64,
    kVoid,
    kLast = kVoid
  };

  static constexpr Type FromShorty(char type);
  static constexpr char TypeId(DataType::Type type);

  static constexpr size_t SizeShift(Type type) {
    switch (type) {
      case Type::kVoid:
      case Type::kBool:
      case Type::kUint8:
      case Type::kInt8:
        return 0;
      case Type::kUint16:
      case Type::kInt16:
        return 1;
      case Type::kUint32:
      case Type::kInt32:
      case Type::kFloat32:
        return 2;
      case Type::kUint64:
      case Type::kInt64:
      case Type::kFloat64:
        return 3;
      case Type::kReference:
        return WhichPowerOf2(kObjectReferenceSize);
      default:
        LOG(FATAL) << "Invalid type " << static_cast<int>(type);
        return 0;
    }
  }

  static constexpr size_t Size(Type type) {
    switch (type) {
      case Type::kVoid:
        return 0;
      case Type::kBool:
      case Type::kUint8:
      case Type::kInt8:
        return 1;
      case Type::kUint16:
      case Type::kInt16:
        return 2;
      case Type::kUint32:
      case Type::kInt32:
      case Type::kFloat32:
        return 4;
      case Type::kUint64:
      case Type::kInt64:
      case Type::kFloat64:
        return 8;
      case Type::kReference:
        return kObjectReferenceSize;
      default:
        LOG(FATAL) << "Invalid type " << static_cast<int>(type);
        return 0;
    }
  }

  static bool IsFloatingPointType(Type type) {
    return type == Type::kFloat32 || type == Type::kFloat64;
  }

  static bool IsIntegralType(Type type) {
    // The Java language does not allow treating boolean as an integral type but
    // our bit representation makes it safe.
    switch (type) {
      case Type::kBool:
      case Type::kUint8:
      case Type::kInt8:
      case Type::kUint16:
      case Type::kInt16:
      case Type::kUint32:
      case Type::kInt32:
      case Type::kUint64:
      case Type::kInt64:
        return true;
      default:
        return false;
    }
  }

  static bool IsIntOrLongType(Type type) {
    return type == Type::kInt32 || type == Type::kInt64;
  }

  static bool Is64BitType(Type type) {
    return type == Type::kUint64 || type == Type::kInt64 || type == Type::kFloat64;
  }

  static bool IsUnsignedType(Type type) {
    return type == Type::kBool || type == Type::kUint8 || type == Type::kUint16 ||
        type == Type::kUint32 || type == Type::kUint64;
  }

  // Return the general kind of `type`, fusing integer-like types as Type::kInt.
  static Type Kind(Type type) {
    switch (type) {
      case Type::kBool:
      case Type::kUint8:
      case Type::kInt8:
      case Type::kUint16:
      case Type::kInt16:
      case Type::kUint32:
      case Type::kInt32:
        return Type::kInt32;
      case Type::kUint64:
      case Type::kInt64:
        return Type::kInt64;
      default:
        return type;
    }
  }

  static int64_t MinValueOfIntegralType(Type type) {
    switch (type) {
      case Type::kBool:
        return std::numeric_limits<bool>::min();
      case Type::kUint8:
        return std::numeric_limits<uint8_t>::min();
      case Type::kInt8:
        return std::numeric_limits<int8_t>::min();
      case Type::kUint16:
        return std::numeric_limits<uint16_t>::min();
      case Type::kInt16:
        return std::numeric_limits<int16_t>::min();
      case Type::kUint32:
        return std::numeric_limits<uint32_t>::min();
      case Type::kInt32:
        return std::numeric_limits<int32_t>::min();
      case Type::kUint64:
        return std::numeric_limits<uint64_t>::min();
      case Type::kInt64:
        return std::numeric_limits<int64_t>::min();
      default:
        LOG(FATAL) << "non integral type";
    }
    return 0;
  }

  static int64_t MaxValueOfIntegralType(Type type) {
    switch (type) {
      case Type::kBool:
        return std::numeric_limits<bool>::max();
      case Type::kUint8:
        return std::numeric_limits<uint8_t>::max();
      case Type::kInt8:
        return std::numeric_limits<int8_t>::max();
      case Type::kUint16:
        return std::numeric_limits<uint16_t>::max();
      case Type::kInt16:
        return std::numeric_limits<int16_t>::max();
      case Type::kUint32:
        return std::numeric_limits<uint32_t>::max();
      case Type::kInt32:
        return std::numeric_limits<int32_t>::max();
      case Type::kUint64:
        return std::numeric_limits<uint64_t>::max();
      case Type::kInt64:
        return std::numeric_limits<int64_t>::max();
      default:
        LOG(FATAL) << "non integral type";
    }
    return 0;
  }

  static bool IsTypeConversionImplicit(Type input_type, Type result_type);
  static bool IsTypeConversionImplicit(int64_t value, Type result_type);

  static const char* PrettyDescriptor(Type type);

 private:
  static constexpr size_t kObjectReferenceSize = 4u;
};
std::ostream& operator<<(std::ostream& os, DataType::Type data_type);

// Defined outside DataType to have the operator<< available for DCHECK_NE().
inline bool DataType::IsTypeConversionImplicit(Type input_type, Type result_type) {
  DCHECK_NE(DataType::Type::kVoid, result_type);
  DCHECK_NE(DataType::Type::kVoid, input_type);

  // Invariant: We should never generate a conversion to a Boolean value.
  DCHECK_NE(DataType::Type::kBool, result_type);

  // Besides conversion to the same type, integral conversions to non-Int64 types
  // are implicit if the result value range covers the input value range, i.e.
  // widening conversions that do not need to trim the sign bits.
  return result_type == input_type ||
         (result_type != Type::kInt64 &&
          IsIntegralType(input_type) &&
          IsIntegralType(result_type) &&
          MinValueOfIntegralType(input_type) >= MinValueOfIntegralType(result_type) &&
          MaxValueOfIntegralType(input_type) <= MaxValueOfIntegralType(result_type));
}

inline bool DataType::IsTypeConversionImplicit(int64_t value, Type result_type) {
  if (IsIntegralType(result_type) && result_type != Type::kInt64) {
    // If the constant value falls in the range of the result_type, type
    // conversion isn't needed.
    return value >= MinValueOfIntegralType(result_type) &&
           value <= MaxValueOfIntegralType(result_type);
  }
  // Conversion isn't implicit if it's into non-integer types, or 64-bit int
  // which may have different number of registers.
  return false;
}

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DATA_TYPE_H_
