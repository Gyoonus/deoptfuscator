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

// Regression test for 22460222, the base class.
// The field gaps order was wrong. If there were two gaps of different sizes,
// and the larger one was needed, it wouldn't be found.

// This class has a size of 9 bytes: 8 for object plus 1 for the field 'b'.
class GapOrderBase {
  public byte b;
}
