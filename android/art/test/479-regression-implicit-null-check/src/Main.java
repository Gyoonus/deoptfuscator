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


public class Main {
  public int x = 0;

  public Main(Main c) {
    // After inlining the graph will look like:
    //     NullCheck c
    //     InstanceFieldGet c
    //     InstanceFieldSet this 3
    // The dead code will eliminate the InstanceFieldGet and we'll end up with:
    //     NullCheck c
    //     InstanceFieldSet this 3
    // At codegen, when verifying if we can move the null check to the user,
    // we should check that we actually have the same user (not only that the
    // next instruction can do implicit null checks).
    // In this case we should generate code for the NullCheck since the next
    // instruction checks a different object.
    c.willBeInlined();
    x = 3;
  }

  private int willBeInlined() {
    return x;
  }

  public static void main(String[] args) {
    try {
      new Main(null);
      throw new RuntimeException("Failed to throw NullPointerException");
    } catch (NullPointerException e) {
      // expected
    }
  }
}
