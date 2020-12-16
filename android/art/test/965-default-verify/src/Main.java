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
class Main implements Iface {
  public static void main(String[] args) {
    System.out.println("Create Main instance");
    Main m = new Main();
    System.out.println("Calling functions on concrete Main");
    callMain(m);
    System.out.println("Calling functions on interface Iface");
    callIface(m);
  }

  public static void callMain(Main m) {
    System.out.println("Calling verifiable function on Main");
    System.out.println(m.sayHi());
    System.out.println("Calling unverifiable function on Main");
    try {
      m.verificationSoftFail();
      System.out.println("Unexpected no error Thrown on Main");
    } catch (NoSuchMethodError e) {
      System.out.println("Expected NSME Thrown on Main");
    } catch (Throwable e) {
      System.out.println("Unexpected Error Thrown on Main");
      e.printStackTrace(System.out);
    }
    System.out.println("Calling verifiable function on Main");
    System.out.println(m.sayHi());
    return;
  }

  public static void callIface(Iface m) {
    System.out.println("Calling verifiable function on Iface");
    System.out.println(m.sayHi());
    System.out.println("Calling unverifiable function on Iface");
    try {
      m.verificationSoftFail();
      System.out.println("Unexpected no error Thrown on Iface");
    } catch (NoSuchMethodError e) {
      System.out.println("Expected NSME Thrown on Iface");
    } catch (Throwable e) {
      System.out.println("Unexpected Error Thrown on Iface");
      e.printStackTrace(System.out);
    }
    System.out.println("Calling verifiable function on Iface");
    System.out.println(m.sayHi());
    return;
  }
}
