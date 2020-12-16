/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.io.*;

/**
 * Regression test for b/75971227 (code sinking with exceptions).
 */
public class Main {

  public static class N {
    int x;
  }

  private int f;

  public int doit(N n1) throws FileNotFoundException {
    int x = 1;
    N n3 = new N();
    try {
      if (n1.x == 0) {
        f = 11;
        x = 3;
      } else {
        f = x;
      }
      throw new FileNotFoundException("n3" + n3.x);
    } catch (NullPointerException e) {
    }
    return x;
  }


  public static void main(String[] args) {
    N n = new N();
    Main t = new Main();
    int x = 0;

    // Main 1, null pointer argument.
    t.f = 0;
    try {
      x = t.doit(null);
    } catch (FileNotFoundException e) {
      x = -1;
    }
    if (x != 1 || t.f != 0) {
      throw new Error("Main 1: x=" + x + " f=" + t.f);
    }

    // Main 2, n.x is 0.
    n.x = 0;
    try {
      x = t.doit(n);
    } catch (FileNotFoundException e) {
      x = -1;
    }
    if (x != -1 || t.f != 11) {
      throw new Error("Main 2: x=" + x + " f=" + t.f);
    }

    // Main 3, n.x is not 0.
    n.x = 1;
    try {
      x = t.doit(n);
    } catch (FileNotFoundException e) {
      x = -1;
    }
    if (x != -1 || t.f != 1) {
      throw new Error("Main 3: x=" + x + " f=" + t.f);
    }

    System.out.println("passed");
  }
}
