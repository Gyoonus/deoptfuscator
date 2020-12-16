/*
 * Copyright (C) 2016 The Android Open Source Project
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

public final class Second {
  public static void staticNop(int unused) { }

  public void nop() { }

  public static Object staticReturnArg2(int unused1, String arg2) {
    return arg2;
  }

  public long returnArg1(long arg1) {
    return arg1;
  }

  public static int staticReturn9() {
    return 9;
  }

  public int return7(Object unused) {
    return 7;
  }

  public static String staticReturnNull() {
    return null;
  }

  public Object returnNull() {
    return null;
  }

  public int getInstanceIntField() {
    return instanceIntField;
  }

  public double getInstanceDoubleField(int unused1) {
    return instanceDoubleField;
  }

  public Object getInstanceObjectField(long unused1) {
    return instanceObjectField;
  }

  public String getInstanceStringField(Object unused1, String unused2, long unused3) {
    return instanceStringField;
  }

  public static int staticGetInstanceIntField(Second s) {
    return s.instanceIntField;
  }

  public double getInstanceDoubleFieldFromParam(Second s) {
    return s.instanceDoubleField;
  }

  public int getStaticIntField() {
    return staticIntField;
  }

  public void setInstanceLongField(int ignored, long value) {
    instanceLongField = value;
  }

  public int setInstanceLongFieldReturnArg2(long value, int arg2) {
    instanceLongField = value;
    return arg2;
  }

  public static void staticSetInstanceLongField(Second s, long value) {
    s.instanceLongField = value;
  }

  public void setInstanceLongFieldThroughParam(Second s, long value) {
    s.instanceLongField = value;
  }

  public void setStaticFloatField(float value) {
    staticFloatField = value;
  }

  public int instanceIntField = 42;
  public double instanceDoubleField = -42.0;
  public Object instanceObjectField = null;
  public String instanceStringField = "dummy";
  public long instanceLongField = 0;  // Overwritten by setters.

  public static int staticIntField = 4242;
  public static float staticFloatField = 0.0f;  // Overwritten by setters.
}
