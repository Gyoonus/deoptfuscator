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

public final class Derived extends Base {
  public Derived() {
    this(0);
  }

  public Derived(int intValue) {
    super(intValue);
  }

  public Derived(String stringValue) {
    super(stringValue);
    stringField = null;   // Clear field set by Base.<init>(String).
  }

  public Derived(double doubleValue) {
    super(doubleValue, null);
  }

  public Derived(int intValue, double doubleValue, Object objectValue) {
    super(intValue, doubleValue, objectValue);
    objectField = null;   // Clear field set by Base.<init>(int, double, Object).
    intField = 0;         // Clear field set by Base.<init>(int, double, Object).
  }

  Derived(int intValue, double doubleValue, Object objectValue, String stringValue) {
    super(intValue, doubleValue, objectValue, stringValue);
    // Clearing fields here doesn't help because the superclass constructor must
    // satisfy the pattern constraints on its own and it doesn't (it has 4 IPUTs).
    intField = 0;
    doubleField = 0.0;
    objectField = null;
    stringField = null;
  }

  public Derived(float floatValue) {
    super();
    floatField = floatValue;
  }

  public Derived(int intValue, double doubleValue, Object objectValue, float floatValue) {
    super(intValue, doubleValue, objectValue);
    objectField = null;   // Clear field set by Base.<init>(int, double, Object).
    floatField = floatValue;
  }

  public float floatField;
}
