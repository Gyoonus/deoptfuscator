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

// A super class for DumpedStuff to test deobfuscation of methods inherited
// from the super class.
public class SuperDumpedStuff {

  public void allocateObjectAtObfSuperSite() {
    objectAllocatedAtObfSuperSite = new Object();
  }

  public void allocateObjectAtUnObfSuperSite() {
    objectAllocatedAtUnObfSuperSite = new Object();
  }

  public void allocateObjectAtOverriddenSite() {
    objectAllocatedAtOverriddenSite = new Object();
  }

  public Object objectAllocatedAtObfSuperSite;
  public Object objectAllocatedAtUnObfSuperSite;
  public Object objectAllocatedAtOverriddenSite;
}
