/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.ahat.heapdump;

import java.nio.charset.StandardCharsets;
import java.util.AbstractList;
import java.util.Collections;
import java.util.List;

/**
 * An array instance from a parsed heap dump.
 * It is used for both object and primitive arrays. The class provides methods
 * for accessing the length and elements of the array in addition to those
 * methods inherited from {@link AhatInstance}.
 */
public class AhatArrayInstance extends AhatInstance {
  // To save space, we store arrays as primitive arrays or AhatInstance arrays
  // and provide a wrapper over the arrays to expose a list of Values.
  // This is especially important for large byte arrays, such as bitmaps.
  // We keep a separate pointer to the underlying array in the case of byte or
  // char arrays because they are sometimes useful to have.
  // TODO: Have different subtypes of AhatArrayInstance to avoid the overhead
  // of these extra pointers and cost in getReferences when the array type is
  // not relevant?
  private List<Value> mValues;
  private byte[] mByteArray;    // null if not a byte array.
  private char[] mCharArray;    // null if not a char array.

  AhatArrayInstance(long id) {
    super(id);
  }

  /**
   * Initialize the array elements for a primitive boolean array.
   */
  void initialize(final boolean[] bools) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return bools.length;
      }

      @Override public Value get(int index) {
        return Value.pack(bools[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive char array.
   */
  void initialize(final char[] chars) {
    mCharArray = chars;
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return chars.length;
      }

      @Override public Value get(int index) {
        return Value.pack(chars[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive float array.
   */
  void initialize(final float[] floats) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return floats.length;
      }

      @Override public Value get(int index) {
        return Value.pack(floats[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive double array.
   */
  void initialize(final double[] doubles) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return doubles.length;
      }

      @Override public Value get(int index) {
        return Value.pack(doubles[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive byte array.
   */
  void initialize(final byte[] bytes) {
    mByteArray = bytes;
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return bytes.length;
      }

      @Override public Value get(int index) {
        return Value.pack(bytes[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive short array.
   */
  void initialize(final short[] shorts) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return shorts.length;
      }

      @Override public Value get(int index) {
        return Value.pack(shorts[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive int array.
   */
  void initialize(final int[] ints) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return ints.length;
      }

      @Override public Value get(int index) {
        return Value.pack(ints[index]);
      }
    };
  }

  /**
   * Initialize the array elements for a primitive long array.
   */
  void initialize(final long[] longs) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return longs.length;
      }

      @Override public Value get(int index) {
        return Value.pack(longs[index]);
      }
    };
  }

  /**
   * Initialize the array elements for an instance array.
   */
  void initialize(final AhatInstance[] insts) {
    mValues = new AbstractList<Value>() {
      @Override public int size() {
        return insts.length;
      }

      @Override public Value get(int index) {
        return Value.pack(insts[index]);
      }
    };
  }

  @Override
  long getExtraJavaSize() {
    int length = getLength();
    if (length == 0) {
      return 0;
    }

    return Value.getType(mValues.get(0)).size * getLength();
  }

  /**
   * Returns the number of elements in the array.
   *
   * @return number of elements in the array.
   */
  public int getLength() {
    return mValues.size();
  }

  /**
   * Returns a list of all of the array's elements in order.
   * The returned list does not support modification.
   *
   * @return list of the array's elements.
   */
  public List<Value> getValues() {
    return mValues;
  }

  /**
   * Returns the value at the given index of this array.
   *
   * @param index the index of the value to retrieve
   * @return the value at the given index
   * @throws IndexOutOfBoundsException if the index is out of range
   */
  public Value getValue(int index) {
    return mValues.get(index);
  }

  @Override
  Iterable<Reference> getReferences() {
    // The list of references will be empty if this is a primitive array.
    List<Reference> refs = Collections.emptyList();
    if (!mValues.isEmpty()) {
      Value first = mValues.get(0);
      if (first == null || first.isAhatInstance()) {
        refs = new AbstractList<Reference>() {
          @Override
          public int size() {
            return mValues.size();
          }

          @Override
          public Reference get(int index) {
            Value value = mValues.get(index);
            if (value != null) {
              assert value.isAhatInstance();
              String field = "[" + Integer.toString(index) + "]";
              return new Reference(AhatArrayInstance.this, field, value.asAhatInstance(), true);
            }
            return null;
          }
        };
      }
    }
    return new SkipNullsIterator(refs);
  }

  @Override public boolean isArrayInstance() {
    return true;
  }

  @Override public AhatArrayInstance asArrayInstance() {
    return this;
  }

  @Override public String asString(int maxChars) {
    return asString(0, getLength(), maxChars);
  }

  /**
   * Returns the String value associated with this array.
   * Only char arrays are considered as having an associated String value.
   */
  String asString(int offset, int count, int maxChars) {
    if (mCharArray == null) {
      return null;
    }

    if (count == 0) {
      return "";
    }
    int numChars = mCharArray.length;
    if (0 <= maxChars && maxChars < count) {
      count = maxChars;
    }

    int end = offset + count - 1;
    if (offset >= 0 && offset < numChars && end >= 0 && end < numChars) {
      return new String(mCharArray, offset, count);
    }
    return null;
  }

  /**
   * Returns the ascii String value associated with this array.
   * Only byte arrays are considered as having an associated ascii String value.
   */
  String asAsciiString(int offset, int count, int maxChars) {
    if (mByteArray == null) {
      return null;
    }

    if (count == 0) {
      return "";
    }
    int numChars = mByteArray.length;
    if (0 <= maxChars && maxChars < count) {
      count = maxChars;
    }

    int end = offset + count - 1;
    if (offset >= 0 && offset < numChars && end >= 0 && end < numChars) {
      return new String(mByteArray, offset, count, StandardCharsets.US_ASCII);
    }
    return null;
  }

  /**
   * Returns the String value associated with this array. Byte arrays are
   * considered as ascii encoded strings.
   */
  String asMaybeCompressedString(int offset, int count, int maxChars) {
    String str = asString(offset, count, maxChars);
    if (str == null) {
      str = asAsciiString(offset, count, maxChars);
    }
    return str;
  }

  @Override public AhatInstance getAssociatedBitmapInstance() {
    if (mByteArray != null) {
      List<AhatInstance> refs = getHardReverseReferences();
      if (refs.size() == 1) {
        AhatInstance ref = refs.get(0);
        return ref.getAssociatedBitmapInstance();
      }
    }
    return null;
  }

  @Override public String toString() {
    String className = getClassName();
    if (className.endsWith("[]")) {
      className = className.substring(0, className.length() - 2);
    }
    return String.format("%s[%d]@%08x", className, mValues.size(), getId());
  }

  byte[] asByteArray() {
    return mByteArray;
  }
}
