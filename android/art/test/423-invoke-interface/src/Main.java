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
  static interface Itf {
    public int return1();
    public int return2();
    public int return3();
    public int return4();
    public int return5();
    public int return6();
    public int return7();
    public int return8();
    public int return9();
    public int return10();
    public int return11();
    public int return12();
    public int return13();
    public int return14();
    public int return15();
    public int return16();
    public int return17();
    public int return18();
    public int return19();
    public int return20();
  }

  static class ItfImpl1 implements Itf {
    public int return1() { return 1; }
    public int return2() { return 2; }
    public int return3() { return 3; }
    public int return4() { return 4; }
    public int return5() { return 5; }
    public int return6() { return 6; }
    public int return7() { return 7; }
    public int return8() { return 8; }
    public int return9() { return 9; }
    public int return10() { return 10; }
    public int return11() { return 11; }
    public int return12() { return 12; }
    public int return13() { return 13; }
    public int return14() { return 14; }
    public int return15() { return 15; }
    public int return16() { return 16; }
    public int return17() { return 17; }
    public int return18() { return 18; }
    public int return19() { return 19; }
    public int return20() { return 20; }
  }

  static class ItfImpl2 implements Itf {
    public int return1() { return -1; }
    public int return2() { return -2; }
    public int return3() { return -3; }
    public int return4() { return -4; }
    public int return5() { return -5; }
    public int return6() { return -6; }
    public int return7() { return -7; }
    public int return8() { return -8; }
    public int return9() { return -9; }
    public int return10() { return -10; }
    public int return11() { return -11; }
    public int return12() { return -12; }
    public int return13() { return -13; }
    public int return14() { return -14; }
    public int return15() { return -15; }
    public int return16() { return -16; }
    public int return17() { return -17; }
    public int return18() { return -18; }
    public int return19() { return -19; }
    public int return20() { return -20; }
  }

  public static void main(String[] args) {
    $opt$InvokeInterface(new ItfImpl1(), 1);
    $opt$InvokeInterface(new ItfImpl2(), -1);
  }

  public static void assertEquals(int expected, int value) {
    if (expected != value) {
      throw new Error("Expected " + expected +  ", got " + value);
    }
  }

  public static void $opt$InvokeInterface(Itf object, int factor) {
    assertEquals(factor * 1, object.return1());
    assertEquals(factor * 2, object.return2());
    assertEquals(factor * 3, object.return3());
    assertEquals(factor * 4, object.return4());
    assertEquals(factor * 5, object.return5());
    assertEquals(factor * 6, object.return6());
    assertEquals(factor * 7, object.return7());
    assertEquals(factor * 8, object.return8());
    assertEquals(factor * 9, object.return9());
    assertEquals(factor * 10, object.return10());
    assertEquals(factor * 11, object.return11());
    assertEquals(factor * 12, object.return12());
    assertEquals(factor * 13, object.return13());
    assertEquals(factor * 14, object.return14());
    assertEquals(factor * 15, object.return15());
    assertEquals(factor * 16, object.return16());
    assertEquals(factor * 17, object.return17());
    assertEquals(factor * 18, object.return18());
    assertEquals(factor * 19, object.return19());
    assertEquals(factor * 20, object.return20());
  }
}
