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
 * A description and value of a field from a heap dump.
 */
public class FieldValue {
  /**
   * The name of the field.
   */
  public final String name;

  /**
   * The type of the field.
   */
  public final Type type;

  /**
   * The value of the field.
   */
  public final Value value;

  /**
   * Constructs an instance of FieldValue.
   *
   * @param name name of the field
   * @param type type of the field
   * @param value value of the field
   */
  public FieldValue(String name, Type type, Value value) {
    this.name = name;
    this.type = type;
    this.value = value;
  }
}
