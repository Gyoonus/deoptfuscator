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
class Main implements Iface, Iface2, Iface3 {
  public static void main(String[] args) {
    System.out.println("Create Main instance");
    Main m = new Main();
    System.out.println("Calling functions on concrete Main");
    callMain(m);
    System.out.println("Calling functions on interface Iface");
    callIface(m);
    System.out.println("Calling functions on interface Iface2");
    callIface2(m);
  }
  public static void callMain(Main m) {
    System.out.println("Calling non-abstract function on Main");
    System.out.println(m.charge());
    System.out.println("Calling abstract function on Main");
    try {
      System.out.println(m.sayHi());
      System.out.println("Unexpected no error Thrown on Main");
    } catch (AbstractMethodError e) {
      System.out.println("Expected AME Thrown on Main");
    } catch (IncompatibleClassChangeError e) {
      System.out.println("Unexpected ICCE Thrown on Main");
    }
    System.out.println("Calling non-abstract function on Main");
    System.out.println(m.charge());
    return;
  }
  public static void callIface(Iface m) {
    System.out.println("Calling non-abstract function on Iface");
    System.out.println(m.charge());
    System.out.println("Calling abstract function on Iface");
    try {
      System.out.println(m.sayHi());
      System.out.println("Unexpected no error Thrown on Iface");
    } catch (AbstractMethodError e) {
      System.out.println("Expected AME Thrown on Iface");
    } catch (IncompatibleClassChangeError e) {
      System.out.println("Unexpected ICCE Thrown on Iface");
    }
    System.out.println("Calling non-abstract function on Iface");
    System.out.println(m.charge());
    return;
  }
  public static void callIface2(Iface2 m) {
    System.out.println("Calling abstract function on Iface2");
    try {
      System.out.println(m.sayHi());
      System.out.println("Unexpected no error Thrown on Iface2");
    } catch (AbstractMethodError e) {
      System.out.println("Expected AME Thrown on Iface2");
    } catch (IncompatibleClassChangeError e) {
      System.out.println("Unexpected ICCE Thrown on Iface2");
    }
    return;
  }
}
