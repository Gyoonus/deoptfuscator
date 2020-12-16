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

/**
 * A Java instance or primitive value from a parsed heap dump.
 * Note: To save memory, a null Value is used to represent a null Java
 * instance from the heap dump.
 */
public abstract class Value {
  /**
   * Constructs a Value for an AhatInstance.
   * Note: returns null for null <code>value</code>.
   *
   * @param value the AhatInstance to make into a value
   * @return the constructed value.
   */
  public static Value pack(AhatInstance value) {
    return value == null ? null : new InstanceValue(value);
  }

  /**
   * Constructs a Value for a boolean.
   *
   * @param value the boolean to make into a value
   * @return the constructed value.
   */
  public static Value pack(boolean value) {
    return new BooleanValue(value);
  }

  /**
   * Constructs a Value for a char.
   *
   * @param value the char to make into a value
   * @return the constructed value.
   */
  public static Value pack(char value) {
    return new CharValue(value);
  }

  /**
   * Constructs a Value for a float.
   *
   * @param value the float to make into a value
   * @return the constructed value.
   */
  public static Value pack(float value) {
    return new FloatValue(value);
  }

  /**
   * Constructs a Value for a double.
   *
   * @param value the double to make into a value
   * @return the constructed value.
   */
  public static Value pack(double value) {
    return new DoubleValue(value);
  }

  /**
   * Constructs a Value for a byte.
   *
   * @param value the byte to make into a value
   * @return the constructed value.
   */
  public static Value pack(byte value) {
    return new ByteValue(value);
  }

  /**
   * Constructs a Value for a short.
   *
   * @param value the short to make into a value
   * @return the constructed value.
   */
  public static Value pack(short value) {
    return new ShortValue(value);
  }

  /**
   * Constructs a Value for a int.
   *
   * @param value the int to make into a value
   * @return the constructed value.
   */
  public static Value pack(int value) {
    return new IntValue(value);
  }

  /**
   * Constructs a Value for a long.
   *
   * @param value the long to make into a value
   * @return the constructed value.
   */
  public static Value pack(long value) {
    return new LongValue(value);
  }

  /**
   * Returns the type of the given value.
   *
   * @param value the value to get the type of
   * @return the value's type
   */
  public static Type getType(Value value) {
    return value == null ? Type.OBJECT : value.getType();
  }

  /**
   * Return the type of the given value.
   */
  abstract Type getType();

  /**
   * Returns true if the Value is an AhatInstance rather than a primitive
   * value.
   *
   * @return true if the value is an AhatInstance
   */
  public boolean isAhatInstance() {
    return false;
  }

  /**
   * Returns the Value as an AhatInstance if it is one.
   * Returns null if the Value represents a Java primitive value.
   *
   * @return the AhatInstance packed into this value
   */
  public AhatInstance asAhatInstance() {
    return null;
  }

  /**
   * Returns true if the Value is an int.
   *
   * @return true if the value is an int.
   */
  public boolean isInteger() {
    return false;
  }

  /**
   * Returns the Value as an int if it is one.
   * Returns null if the Value does not represent an int.
   *
   * @return the int packed into this value
   */
  public Integer asInteger() {
    return null;
  }

  /**
   * Returns true if the Value is an long.
   *
   * @return true if the value is an long.
   */
  public boolean isLong() {
    return false;
  }

  /**
   * Returns the Value as an long if it is one.
   * Returns null if the Value does not represent an long.
   *
   * @return the long packed into this value
   */
  public Long asLong() {
    return null;
  }

  /**
   * Returns the Value as an byte if it is one.
   * Returns null if the Value does not represent an byte.
   *
   * @return the byte packed into this value
   */
  public Byte asByte() {
    return null;
  }

  /**
   * Returns the Value as an char if it is one.
   * Returns null if the Value does not represent an char.
   *
   * @return the char packed into this value
   */
  public Character asChar() {
    return null;
  }

  @Override
  public abstract String toString();

  private Value getBaseline() {
    return this;
  }

  /**
   * Returns the baseline of the given value for the purposes of diff.
   * This method can be used to handle the case when the Value is null.
   *
   * @param value the value to get the baseline of
   * @return the baseline of the value
   * @see Diffable#getBaseline
   */
  public static Value getBaseline(Value value) {
    return value == null ? null : value.getBaseline();
  }

  @Override
  public abstract boolean equals(Object other);

  private static class BooleanValue extends Value {
    private boolean mBool;

    BooleanValue(boolean bool) {
      mBool = bool;
    }

    @Override
    Type getType() {
      return Type.BOOLEAN;
    }

    @Override
    public String toString() {
      return Boolean.toString(mBool);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof BooleanValue) {
        BooleanValue value = (BooleanValue)other;
        return mBool == value.mBool;
      }
      return false;
    }
  }

  private static class ByteValue extends Value {
    private byte mByte;

    ByteValue(byte b) {
      mByte = b;
    }

    @Override
    public Byte asByte() {
      return mByte;
    }

    @Override
    Type getType() {
      return Type.BYTE;
    }

    @Override
    public String toString() {
      return Byte.toString(mByte);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof ByteValue) {
        ByteValue value = (ByteValue)other;
        return mByte == value.mByte;
      }
      return false;
    }
  }

  private static class CharValue extends Value {
    private char mChar;

    CharValue(char c) {
      mChar = c;
    }

    @Override
    public Character asChar() {
      return mChar;
    }

    @Override
    Type getType() {
      return Type.CHAR;
    }

    @Override
    public String toString() {
      return Character.toString(mChar);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof CharValue) {
        CharValue value = (CharValue)other;
        return mChar == value.mChar;
      }
      return false;
    }
  }

  private static class DoubleValue extends Value {
    private double mDouble;

    DoubleValue(double d) {
      mDouble = d;
    }

    @Override
    Type getType() {
      return Type.DOUBLE;
    }

    @Override
    public String toString() {
      return Double.toString(mDouble);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof DoubleValue) {
        DoubleValue value = (DoubleValue)other;
        return mDouble == value.mDouble;
      }
      return false;
    }
  }

  private static class FloatValue extends Value {
    private float mFloat;

    FloatValue(float f) {
      mFloat = f;
    }

    @Override
    Type getType() {
      return Type.FLOAT;
    }

    @Override
    public String toString() {
      return Float.toString(mFloat);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof FloatValue) {
        FloatValue value = (FloatValue)other;
        return mFloat == value.mFloat;
      }
      return false;
    }
  }

  private static class InstanceValue extends Value {
    private AhatInstance mInstance;

    InstanceValue(AhatInstance inst) {
      assert(inst != null);
      mInstance = inst;
    }

    @Override
    public boolean isAhatInstance() {
      return true;
    }

    @Override
    public AhatInstance asAhatInstance() {
      return mInstance;
    }

    @Override
    Type getType() {
      return Type.OBJECT;
    }

    @Override
    public String toString() {
      return mInstance.toString();
    }

    public Value getBaseline() {
      return InstanceValue.pack(mInstance.getBaseline());
    }

    @Override public boolean equals(Object other) {
      if (other instanceof InstanceValue) {
        InstanceValue value = (InstanceValue)other;
        return mInstance.equals(value.mInstance);
      }
      return false;
    }
  }

  private static class IntValue extends Value {
    private int mInt;

    IntValue(int i) {
      mInt = i;
    }

    @Override
    public boolean isInteger() {
      return true;
    }

    @Override
    public Integer asInteger() {
      return mInt;
    }

    @Override
    Type getType() {
      return Type.INT;
    }

    @Override
    public String toString() {
      return Integer.toString(mInt);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof IntValue) {
        IntValue value = (IntValue)other;
        return mInt == value.mInt;
      }
      return false;
    }
  }

  private static class LongValue extends Value {
    private long mLong;

    LongValue(long l) {
      mLong = l;
    }

    @Override
    public boolean isLong() {
      return true;
    }

    @Override
    public Long asLong() {
      return mLong;
    }

    @Override
    Type getType() {
      return Type.LONG;
    }

    @Override
    public String toString() {
      return Long.toString(mLong);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof LongValue) {
        LongValue value = (LongValue)other;
        return mLong == value.mLong;
      }
      return false;
    }
  }

  private static class ShortValue extends Value {
    private short mShort;

    ShortValue(short s) {
      mShort = s;
    }

    @Override
    Type getType() {
      return Type.SHORT;
    }

    @Override
    public String toString() {
      return Short.toString(mShort);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof ShortValue) {
        ShortValue value = (ShortValue)other;
        return mShort == value.mShort;
      }
      return false;
    }
  }
}
