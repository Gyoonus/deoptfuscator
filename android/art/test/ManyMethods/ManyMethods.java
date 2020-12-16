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

class ManyMethods {
  static class Strings {
    public static String msg0 = "Hello World";
    public static String msg1 = "Hello World1";
    public static String msg2 = "Hello World2";
    public static String msg3 = "Hello World3";
    public static String msg4 = "Hello World4";
    public static String msg5 = "Hello World5";
    public static String msg6 = "Hello World6";
    public static String msg7 = "Hello World7";
    public static String msg8 = "Hello World8";
    public static String msg9 = "Hello World9";
    public static String msg10 = "Hello World10";
    public static String msg11 = "Hello World11";
  }

  static class Printer {
    static void Print(String s) {
      System.out.println(s);
    }
  }

  static class Printer2 {
    static void Print(String s) {
      System.out.println("AAA" + s);
    }
  }

  public static void Print0() {
    Printer.Print(Strings.msg0);
  }

  public static void Print1() {
    Printer.Print(Strings.msg1);
  }

  public static void Print2() {
    Printer.Print(Strings.msg2);
  }

  public static void Print3() {
    Printer.Print(Strings.msg1);
  }

  public static void Print4() {
    Printer.Print(Strings.msg4);
  }

  public static void Print5() {
    Printer.Print(Strings.msg5);
  }

  public static void Print6() {
    Printer2.Print(Strings.msg6);
  }

  public static void Print7() {
    Printer.Print(Strings.msg7);
  }

  public static void Print8() {
    Printer.Print(Strings.msg8);
  }

  public static void Print9() {
    Printer2.Print(Strings.msg9);
  }

  public static void Print10() {
    Printer2.Print(Strings.msg10);
  }

  public static void Print11() {
    Printer.Print(Strings.msg11);
  }

  public static void main(String args[]) {
    Print0();
    Print1();
    Print2();
    Print3();
    Print4();
    Print5();
    Print6();
    Print7();
    Print8();
    Print9();
    Print10();
    Print11();
  }
}
