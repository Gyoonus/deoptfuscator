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

package com.android.ahat.heapdump;

/**
 * Enum corresponding to basic types from the binary heap dump format.
 */
public enum Type {
  /**
   * Type used for any Java object.
   */
  OBJECT("Object", 4),

  /**
   * The primitive boolean type.
   */
  BOOLEAN("boolean", 1),

  /**
   * The primitive char type.
   */
  CHAR("char", 2),

  /**
   * The primitive float type.
   */
  FLOAT("float", 4),

  /**
   * The primitive double type.
   */
  DOUBLE("double", 8),

  /**
   * The primitive byte type.
   */
  BYTE("byte", 1),

  /**
   * The primitive short type.
   */
  SHORT("short", 2),

  /**
   * The primitive int type.
   */
  INT("int", 4),

  /**
   * The primitive long type.
   */
  LONG("long", 8);

  /**
   * The name of the type.
   */
  public final String name;

  /**
   * The number of bytes taken up by values of this type in the Java heap.
   */
  final int size;

  Type(String name, int size) {
    this.name = name;
    this.size = size;
  }

  @Override
  public String toString() {
    return name;
  }
}
