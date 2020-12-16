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

public class UnresolvedClass extends UnresolvedSuperClass implements UnresolvedInterface {
  static public void staticMethod() {
    System.out.println("UnresolvedClass.staticMethod()");
  }

  public UnresolvedClass() {
    System.out.println("UnresolvedClass.directCall()");
  }

  public void virtualMethod() {
    System.out.println("UnresolvedClass.virtualMethod()");
  }

  public void interfaceMethod() {
    System.out.println("UnresolvedClass.interfaceMethod()");
  }

  public static byte staticByte;
  public static char staticChar;
  public static int staticInt;
  public static long staticLong;
  public static float staticFloat;
  public static double staticDouble;
  public static Object staticObject;

  public byte instanceByte;
  public char instanceChar;
  public int instanceInt;
  public long instanceLong;
  public float instanceFloat;
  public double instanceDouble;
  public Object instanceObject;
}

