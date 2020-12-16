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

public class Base {
  Base() {
    intField = 0;               // Unnecessary IPUT.
    doubleField = 0.0;          // Unnecessary IPUT.
    objectField = null;         // Unnecessary IPUT.
  }

  Base(int intValue) {
    intField = intValue;
  }

  Base(String stringValue) {
    objectField = stringValue;  // Unnecessary IPUT.
    stringField = stringValue;
    objectField = null;         // Unnecessary IPUT.
  }

  Base(double doubleValue, Object objectValue) {
    doubleField = doubleValue;
    objectField = objectValue;
  }

  Base(int intValue, double doubleValue, Object objectValue) {
    intField = intValue;
    doubleField = doubleValue;
    objectField = objectValue;
  }

  Base(int intValue, double doubleValue, Object objectValue, String stringValue) {
    // Outside our limit of 3 IPUTs.
    intField = intValue;
    doubleField = doubleValue;
    objectField = objectValue;
    stringField = stringValue;
  }

  Base(double doubleValue) {
    this(doubleValue, null);
  }

  Base(Object objectValue) {
    // Unsupported forwarding of a value after a zero.
    this(0.0, objectValue);
  }

  Base(int intValue, long dummy) {
    this(intValue, 0.0, null);
  }

  public int intField;
  public double doubleField;
  public Object objectField;
  public String stringField;
}
