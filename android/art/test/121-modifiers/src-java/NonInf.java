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

public abstract class NonInf {

  public int publicField;
  private int privateField;
  protected int protectedField;
  static int staticField;
  transient int transientField;
  volatile int volatileField;
  final int finalField;

  public NonInf() {
    publicField = 0;
    privateField = 1;
    protectedField = 2;
    staticField = 3;
    transientField = 4;
    volatileField = 5;
    finalField = 6;
  }

  public native void nativeMethod();

  private int privateMethod() {
    return 0;
  }

  protected int protectedMethod() {
    return 0;
  }

  public int publicMethod() {
    return 0;
  }

  public abstract int abstractMethod();

  public synchronized int synchronizedMethod() {
    return 0;
  }

  public static int staticMethod() {
    return 0;
  }

  public strictfp double strictfpMethod() {
    return 0.0;
  }

  public int varargsMethod(Object... args) {
    return 0;
  }

  public final int finalMethod() {
    return 0;
  }
}