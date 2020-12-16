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

public class Main {

  // The following tests make sure that we inline methods used inside try and catch
  // blocks, provided they meet other inlining criteria. To do that, we rely on
  // the compiler recognizing and enforcing the $inline$ and $noinline$ markers.

  // We expect a single block to always be inlined.

  private static int $inline$SingleBlock(String str) throws NumberFormatException {
    return Integer.parseInt(str);
  }

  // We expect a "simple" method with multiple blocks to always be inlined.

  private static int $inline$MultipleBlocks(String str, boolean is_hex)
      throws NumberFormatException {
    return is_hex ? Integer.parseInt(str, 16) : Integer.parseInt(str);
  }

  // We expect methods with try/catch to not be inlined. Inlined try/catch
  // blocks are not supported at the moment.

  private static int $noinline$TryCatch(String str) {
    try {
      return Integer.parseInt(str);
    } catch (NumberFormatException ex) {
      return -1;
    }
  }

  public static void testSingleBlockFromTry() {
    int val = 0;

    try {
      val = $inline$SingleBlock("42");
    } catch (NumberFormatException ex) {
      unreachable();
    }
    assertEquals(42, val);

    try {
      $inline$SingleBlock("xyz");
      unreachable();
    } catch (NumberFormatException ex) {}
  }

  public static void testSingleBlockFromCatch() {
    int val = 0;

    try {
      throwException();
    } catch (Exception ex) {
      val = $inline$SingleBlock("42");
    }
    assertEquals(42, val);
  }

  public static void testMultipleBlocksFromTry() {
    int val = 0;

    try {
      val = $inline$MultipleBlocks("42", false);
    } catch (NumberFormatException ex) {
      unreachable();
    }
    assertEquals(42, val);

    try {
      val = $inline$MultipleBlocks("20", true);
    } catch (NumberFormatException ex) {
      unreachable();
    }
    assertEquals(32, val);

    try {
      $inline$MultipleBlocks("xyz", false);
      unreachable();
    } catch (NumberFormatException ex) {}

    try {
      $inline$MultipleBlocks("xyz", true);
      unreachable();
    } catch (NumberFormatException ex) {}
  }

  public static void testMultipleBlocksFromCatch() {
    int val = 0;

    try {
      throwException();
    } catch (Exception ex) {
      val = $inline$MultipleBlocks("42", false);
    }
    assertEquals(42, val);

    try {
      throwException();
    } catch (Exception ex) {
      val = $inline$MultipleBlocks("20", true);
    }
    assertEquals(32, val);
  }

  public static void testTryCatchFromTry() {
    int val = 0;

    try {
      val = $noinline$TryCatch("42");
    } catch (NumberFormatException ex) {
      unreachable();
    }
    assertEquals(42, val);

    try {
      val = $noinline$TryCatch("xyz");
    } catch (NumberFormatException ex) {
      unreachable();
    }
    assertEquals(-1, val);
  }

  public static void testTryCatchFromCatch() {
    int val = 0;

    try {
      throwException();
    } catch (Exception ex) {
      val = $noinline$TryCatch("42");
    }
    assertEquals(42, val);

    try {
      throwException();
    } catch (Exception ex) {
      val = $noinline$TryCatch("xyz");
    }
    assertEquals(-1, val);
  }

  public static void main(String[] args) {
    testSingleBlockFromTry();
    testSingleBlockFromCatch();
    testMultipleBlocksFromTry();
    testMultipleBlocksFromCatch();
    testTryCatchFromTry();
    testTryCatchFromCatch();
  }

  private static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Wrong result: " + expected + " != " + actual);
    }
  }

  private static void unreachable() {
    throw new Error("Unreachable");
  }

  private static void throwException() throws Exception {
    throw new Exception();
  }
}
