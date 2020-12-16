/*
 * Copyright (C) 2015 The Android Open Source Project
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

package p1;

// Specific class for putting the 'entered' marker. If we were to put the marker
// in InP1 or in OtherInP1, the code in MyClassLoader using that marker would load
// InP1 or OtherInP1 in the system class loader, and not in MyClassLoader.
public class PlaceHolder {
  public static boolean entered = false;
}
