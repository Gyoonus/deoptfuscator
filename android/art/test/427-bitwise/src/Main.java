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

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.
public class Main {

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    andInt();
    andLong();

    orInt();
    orLong();

    xorInt();
    xorLong();
  }

  private static void andInt() {
    expectEquals(1, $opt$And(5, 3));
    expectEquals(0, $opt$And(0, 0));
    expectEquals(0, $opt$And(0, 3));
    expectEquals(0, $opt$And(3, 0));
    expectEquals(1, $opt$And(1, -3));
    expectEquals(-12, $opt$And(-12, -3));

    expectEquals(1, $opt$AndLit8(1));
    expectEquals(0, $opt$AndLit8(0));
    expectEquals(0, $opt$AndLit8(0));
    expectEquals(3, $opt$AndLit8(3));
    expectEquals(4, $opt$AndLit8(-12));

    expectEquals(0, $opt$AndLit16(1));
    expectEquals(0, $opt$AndLit16(0));
    expectEquals(0, $opt$AndLit16(0));
    expectEquals(0, $opt$AndLit16(3));
    expectEquals(65280, $opt$AndLit16(-12));
  }

  private static void andLong() {
    expectEquals(1L, $opt$And(5L, 3L));
    expectEquals(0L, $opt$And(0L, 0L));
    expectEquals(0L, $opt$And(0L, 3L));
    expectEquals(0L, $opt$And(3L, 0L));
    expectEquals(1L, $opt$And(1L, -3L));
    expectEquals(-12L, $opt$And(-12L, -3L));

    expectEquals(1L, $opt$AndLit8(1L));
    expectEquals(0L, $opt$AndLit8(0L));
    expectEquals(0L, $opt$AndLit8(0L));
    expectEquals(3L, $opt$AndLit8(3L));
    expectEquals(4L, $opt$AndLit8(-12L));

    expectEquals(0L, $opt$AndLit16(1L));
    expectEquals(0L, $opt$AndLit16(0L));
    expectEquals(0L, $opt$AndLit16(0L));
    expectEquals(0L, $opt$AndLit16(3L));
    expectEquals(65280L, $opt$AndLit16(-12L));
  }

  static int $opt$And(int a, int b) {
    return a & b;
  }

  static int $opt$AndLit8(int a) {
    return a & 0xF;
  }

  static int $opt$AndLit16(int a) {
    return a & 0xFF00;
  }

  static long $opt$And(long a, long b) {
    return a & b;
  }

  static long $opt$AndLit8(long a) {
    return a & 0xF;
  }

  static long $opt$AndLit16(long a) {
    return a & 0xFF00;
  }

  private static void orInt() {
    expectEquals(7, $opt$Or(5, 3));
    expectEquals(0, $opt$Or(0, 0));
    expectEquals(3, $opt$Or(0, 3));
    expectEquals(3, $opt$Or(3, 0));
    expectEquals(-3, $opt$Or(1, -3));
    expectEquals(-3, $opt$Or(-12, -3));

    expectEquals(15, $opt$OrLit8(1));
    expectEquals(15, $opt$OrLit8(0));
    expectEquals(15, $opt$OrLit8(3));
    expectEquals(-1, $opt$OrLit8(-12));

    expectEquals(0xFF01, $opt$OrLit16(1));
    expectEquals(0xFF00, $opt$OrLit16(0));
    expectEquals(0xFF03, $opt$OrLit16(3));
    expectEquals(-12, $opt$OrLit16(-12));
  }

  private static void orLong() {
    expectEquals(7L, $opt$Or(5L, 3L));
    expectEquals(0L, $opt$Or(0L, 0L));
    expectEquals(3L, $opt$Or(0L, 3L));
    expectEquals(3L, $opt$Or(3L, 0L));
    expectEquals(-3L, $opt$Or(1L, -3L));
    expectEquals(-3L, $opt$Or(-12L, -3L));

    expectEquals(15L, $opt$OrLit8(1L));
    expectEquals(15L, $opt$OrLit8(0L));
    expectEquals(15L, $opt$OrLit8(3L));
    expectEquals(-1L, $opt$OrLit8(-12L));

    expectEquals(0xFF01L, $opt$OrLit16(1L));
    expectEquals(0xFF00L, $opt$OrLit16(0L));
    expectEquals(0xFF03L, $opt$OrLit16(3L));
    expectEquals(-12L, $opt$OrLit16(-12L));
  }

  static int $opt$Or(int a, int b) {
    return a | b;
  }

  static int $opt$OrLit8(int a) {
    return a | 0xF;
  }

  static int $opt$OrLit16(int a) {
    return a | 0xFF00;
  }

  static long $opt$Or(long a, long b) {
    return a | b;
  }

  static long $opt$OrLit8(long a) {
    return a | 0xF;
  }

  static long $opt$OrLit16(long a) {
    return a | 0xFF00;
  }

  private static void xorInt() {
    expectEquals(6, $opt$Xor(5, 3));
    expectEquals(0, $opt$Xor(0, 0));
    expectEquals(3, $opt$Xor(0, 3));
    expectEquals(3, $opt$Xor(3, 0));
    expectEquals(-4, $opt$Xor(1, -3));
    expectEquals(9, $opt$Xor(-12, -3));

    expectEquals(14, $opt$XorLit8(1));
    expectEquals(15, $opt$XorLit8(0));
    expectEquals(12, $opt$XorLit8(3));
    expectEquals(-5, $opt$XorLit8(-12));

    expectEquals(0xFF01, $opt$XorLit16(1));
    expectEquals(0xFF00, $opt$XorLit16(0));
    expectEquals(0xFF03, $opt$XorLit16(3));
    expectEquals(-0xFF0c, $opt$XorLit16(-12));
  }

  private static void xorLong() {
    expectEquals(6L, $opt$Xor(5L, 3L));
    expectEquals(0L, $opt$Xor(0L, 0L));
    expectEquals(3L, $opt$Xor(0L, 3L));
    expectEquals(3L, $opt$Xor(3L, 0L));
    expectEquals(-4L, $opt$Xor(1L, -3L));
    expectEquals(9L, $opt$Xor(-12L, -3L));

    expectEquals(14L, $opt$XorLit8(1L));
    expectEquals(15L, $opt$XorLit8(0L));
    expectEquals(12L, $opt$XorLit8(3L));
    expectEquals(-5L, $opt$XorLit8(-12L));

    expectEquals(0xFF01L, $opt$XorLit16(1L));
    expectEquals(0xFF00L, $opt$XorLit16(0L));
    expectEquals(0xFF03L, $opt$XorLit16(3L));
    expectEquals(-0xFF0cL, $opt$XorLit16(-12L));
  }

  static int $opt$Xor(int a, int b) {
    return a ^ b;
  }

  static int $opt$XorLit8(int a) {
    return a ^ 0xF;
  }

  static int $opt$XorLit16(int a) {
    return a ^ 0xFF00;
  }

  static long $opt$Xor(long a, long b) {
    return a ^ b;
  }

  static long $opt$XorLit8(long a) {
    return a ^ 0xF;
  }

  static long $opt$XorLit16(long a) {
    return a ^ 0xFF00;
  }
}
