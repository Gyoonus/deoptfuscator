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

public class Main {
  /// CHECK-START: void Main.staticNop() inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.staticNop() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static void staticNop() {
    Second.staticNop(11);
  }

  /// CHECK-START: void Main.nop(Second) inliner (before)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: void Main.nop(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static void nop(Second s) {
    s.nop();
  }

  /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (before)
  /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Ignored:i\d+>>    IntConstant 77
  /// CHECK-DAG:  <<ClinitCk:l\d+>>   ClinitCheck
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:  <<Invoke:l\d+>>     InvokeStaticOrDirect [<<Ignored>>,<<Value>>{{(,[ij]\d+)?}},<<ClinitCk>>]
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (after)
  /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
  /// CHECK-DAG:                      Return [<<Value>>]

  /// CHECK-START: java.lang.Object Main.staticReturnArg2(java.lang.String) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static Object staticReturnArg2(String value) {
    return Second.staticReturnArg2(77, value);
  }

  /// CHECK-START: long Main.returnArg1(Second, long) inliner (before)
  /// CHECK-DAG:  <<Second:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
  /// CHECK-DAG:  <<NullCk:l\d+>>     NullCheck [<<Second>>]
  /// CHECK-DAG:  <<Invoke:j\d+>>     InvokeVirtual [<<NullCk>>,<<Value>>]
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: long Main.returnArg1(Second, long) inliner (after)
  /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
  /// CHECK-DAG:                      Return [<<Value>>]

  /// CHECK-START: long Main.returnArg1(Second, long) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static long returnArg1(Second s, long value) {
    return s.returnArg1(value);
  }

  /// CHECK-START: int Main.staticReturn9() inliner (before)
  /// CHECK:      {{i\d+}}            InvokeStaticOrDirect

  /// CHECK-START: int Main.staticReturn9() inliner (before)
  /// CHECK-NOT:                      IntConstant 9

  /// CHECK-START: int Main.staticReturn9() inliner (after)
  /// CHECK-DAG:  <<Const9:i\d+>>     IntConstant 9
  /// CHECK-DAG:                      Return [<<Const9>>]

  /// CHECK-START: int Main.staticReturn9() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static int staticReturn9() {
    return Second.staticReturn9();
  }

  /// CHECK-START: int Main.return7(Second) inliner (before)
  /// CHECK:      {{i\d+}}            InvokeVirtual

  /// CHECK-START: int Main.return7(Second) inliner (before)
  /// CHECK-NOT:                      IntConstant 7

  /// CHECK-START: int Main.return7(Second) inliner (after)
  /// CHECK-DAG:  <<Const7:i\d+>>     IntConstant 7
  /// CHECK-DAG:                      Return [<<Const7>>]

  /// CHECK-START: int Main.return7(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static int return7(Second s) {
    return s.return7(null);
  }

  /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (before)
  /// CHECK:      {{l\d+}}            InvokeStaticOrDirect

  /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (before)
  /// CHECK-NOT:                      NullConstant

  /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (after)
  /// CHECK-DAG:  <<Null:l\d+>>       NullConstant
  /// CHECK-DAG:                      Return [<<Null>>]

  /// CHECK-START: java.lang.String Main.staticReturnNull() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static String staticReturnNull() {
    return Second.staticReturnNull();
  }

  /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (before)
  /// CHECK:      {{l\d+}}            InvokeVirtual

  /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (before)
  /// CHECK-NOT:                      NullConstant

  /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (after)
  /// CHECK-DAG:  <<Null:l\d+>>       NullConstant
  /// CHECK-DAG:                      Return [<<Null>>]

  /// CHECK-START: java.lang.Object Main.returnNull(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static Object returnNull(Second s) {
    return s.returnNull();
  }

  /// CHECK-START: int Main.getInt(Second) inliner (before)
  /// CHECK:      {{i\d+}}            InvokeVirtual

  /// CHECK-START: int Main.getInt(Second) inliner (after)
  /// CHECK:      {{i\d+}}            InstanceFieldGet

  /// CHECK-START: int Main.getInt(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static int getInt(Second s) {
    return s.getInstanceIntField();
  }

  /// CHECK-START: double Main.getDouble(Second) inliner (before)
  /// CHECK:      {{d\d+}}            InvokeVirtual

  /// CHECK-START: double Main.getDouble(Second) inliner (after)
  /// CHECK:      {{d\d+}}            InstanceFieldGet

  /// CHECK-START: double Main.getDouble(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static double getDouble(Second s) {
    return s.getInstanceDoubleField(22);
  }

  /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (before)
  /// CHECK:      {{l\d+}}            InvokeVirtual

  /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (after)
  /// CHECK:      {{l\d+}}            InstanceFieldGet

  /// CHECK-START: java.lang.Object Main.getObject(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static Object getObject(Second s) {
    return s.getInstanceObjectField(-1L);
  }

  /// CHECK-START: java.lang.String Main.getString(Second) inliner (before)
  /// CHECK:      {{l\d+}}            InvokeVirtual

  /// CHECK-START: java.lang.String Main.getString(Second) inliner (after)
  /// CHECK:      {{l\d+}}            InstanceFieldGet

  /// CHECK-START: java.lang.String Main.getString(Second) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static String getString(Second s) {
    return s.getInstanceStringField(null, "whatever", 1234L);
  }

  /// CHECK-START: int Main.staticGetInt(Second) inliner (before)
  /// CHECK:      {{i\d+}}            InvokeStaticOrDirect

  /// CHECK-START: int Main.staticGetInt(Second) inliner (after)
  /// CHECK:      {{i\d+}}            InvokeStaticOrDirect

  /// CHECK-START: int Main.staticGetInt(Second) inliner (after)
  /// CHECK-NOT:                      InstanceFieldGet

  public static int staticGetInt(Second s) {
    return Second.staticGetInstanceIntField(s);
  }

  /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (before)
  /// CHECK:      {{d\d+}}            InvokeVirtual

  /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (after)
  /// CHECK:      {{d\d+}}            InvokeVirtual

  /// CHECK-START: double Main.getDoubleFromParam(Second) inliner (after)
  /// CHECK-NOT:                      InstanceFieldGet

  public static double getDoubleFromParam(Second s) {
    return s.getInstanceDoubleFieldFromParam(s);
  }

  /// CHECK-START: int Main.getStaticInt(Second) inliner (before)
  /// CHECK:      {{i\d+}}            InvokeVirtual

  /// CHECK-START: int Main.getStaticInt(Second) inliner (after)
  /// CHECK:      {{i\d+}}            InvokeVirtual

  /// CHECK-START: int Main.getStaticInt(Second) inliner (after)
  /// CHECK-NOT:                      InstanceFieldGet
  /// CHECK-NOT:                      StaticFieldGet

  public static int getStaticInt(Second s) {
    return s.getStaticIntField();
  }

  /// CHECK-START: long Main.setLong(Second, long) inliner (before)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: long Main.setLong(Second, long) inliner (after)
  /// CHECK:                          InstanceFieldSet

  /// CHECK-START: long Main.setLong(Second, long) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static long setLong(Second s, long value) {
    s.setInstanceLongField(-1, value);
    return s.instanceLongField;
  }

  /// CHECK-START: long Main.setLongReturnArg2(Second, long, int) inliner (before)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: long Main.setLongReturnArg2(Second, long, int) inliner (after)
  /// CHECK-DAG:  <<Second:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
  /// CHECK-DAG:  <<Arg2:i\d+>>       ParameterValue
  /// CHECK-DAG:  <<NullCk:l\d+>>     NullCheck [<<Second>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<NullCk>>,<<Value>>]
  /// CHECK-DAG:  <<NullCk2:l\d+>>    NullCheck [<<Second>>]
  /// CHECK-DAG:  <<IGet:j\d+>>       InstanceFieldGet [<<NullCk2>>]
  /// CHECK-DAG:  <<Conv:j\d+>>       TypeConversion [<<Arg2>>]
  /// CHECK-DAG:  <<Add:j\d+>>        Add [<<IGet>>,<<Conv>>]
  /// CHECK-DAG:                      Return [<<Add>>]

  /// CHECK-START: long Main.setLongReturnArg2(Second, long, int) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual

  public static long setLongReturnArg2(Second s, long value, int arg2) {
    int result = s.setInstanceLongFieldReturnArg2(value, arg2);
    return s.instanceLongField + result;
  }

  /// CHECK-START: long Main.staticSetLong(Second, long) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: long Main.staticSetLong(Second, long) inliner (after)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: long Main.staticSetLong(Second, long) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static long staticSetLong(Second s, long value) {
    Second.staticSetInstanceLongField(s, value);
    return s.instanceLongField;
  }

  /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (before)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (after)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: long Main.setLongThroughParam(Second, long) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static long setLongThroughParam(Second s, long value) {
    s.setInstanceLongFieldThroughParam(s, value);
    return s.instanceLongField;
  }

  /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (before)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (after)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: float Main.setStaticFloat(Second, float) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet
  /// CHECK-NOT:                      StaticFieldSet

  public static float setStaticFloat(Second s, float value) {
    s.setStaticFloatField(value);
    return s.staticFloatField;
  }

  /// CHECK-START: java.lang.Object Main.newObject() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:java.lang.Object.<init>

  /// CHECK-START: java.lang.Object Main.newObject() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static Object newObject() {
    return new Object();
  }

  /// CHECK-START: double Main.constructBase() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase() {
    Base b = new Base();
    return b.intField + b.doubleField;
  }

  /// CHECK-START: double Main.constructBase(int) inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(int) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBase(int) inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: double Main.constructBase(int) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(int intValue) {
    Base b = new Base(intValue);
    return b.intField + b.doubleField;
  }

  /// CHECK-START: double Main.constructBaseWith0() inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBaseWith0() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBaseWith0() {
    Base b = new Base(0);
    return b.intField + b.doubleField;
  }

  /// CHECK-START: java.lang.String Main.constructBase(java.lang.String) inliner (before)
  /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: java.lang.String Main.constructBase(java.lang.String) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: java.lang.String Main.constructBase(java.lang.String) inliner (after)
  /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: java.lang.String Main.constructBase(java.lang.String) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static String constructBase(String stringValue) {
    Base b = new Base(stringValue);
    return b.stringField;
  }

  /// CHECK-START: java.lang.String Main.constructBaseWithNullString() inliner (before)
  /// CHECK-DAG:  <<Null:l\d+>>       NullConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Null>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: java.lang.String Main.constructBaseWithNullString() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: java.lang.String Main.constructBaseWithNullString() inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static String constructBaseWithNullString() {
    String stringValue = null;
    Base b = new Base(stringValue);
    return b.stringField;
  }

  /// CHECK-START: double Main.constructBase(double, java.lang.Object) inliner (before)
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<DValue>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(double, java.lang.Object) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBase(double, java.lang.Object) inliner (after)
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<DValue>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<OValue>>]

  /// CHECK-START: double Main.constructBase(double, java.lang.Object) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(double doubleValue, Object objectValue) {
    Base b = new Base(doubleValue, objectValue);
    return (b.objectField != null) ? b.doubleField : -b.doubleField;
  }

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object) inliner (after)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<IValue>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<DValue>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<OValue>>]

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(int intValue, double doubleValue, Object objectValue) {
    Base b = new Base(intValue, doubleValue, objectValue);
    double tmp = b.intField + b.doubleField;
    return (b.objectField != null) ? tmp : -tmp;
  }

  /// CHECK-START: double Main.constructBaseWith0DoubleNull(double) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     IntConstant 0
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     NullConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBaseWith0DoubleNull(double) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBaseWith0DoubleNull(double) inliner (after)
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<DValue>>]

  /// CHECK-START: double Main.constructBaseWith0DoubleNull(double) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBaseWith0DoubleNull(double doubleValue) {
    Base b = new Base(0, doubleValue, null);
    double tmp = b.intField + b.doubleField;
    return (b.objectField != null) ? tmp : -tmp;
  }

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object, java.lang.String) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,{{l\d+}},{{l\d+}}{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object, java.lang.String) inliner (after)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,{{l\d+}},{{l\d+}}{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(int, double, java.lang.Object, java.lang.String) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(
      int intValue, double doubleValue, Object objectValue, String stringValue) {
    Base b = new Base(intValue, doubleValue, objectValue, stringValue);
    double tmp = b.intField + b.doubleField;
    tmp = (b.objectField != null) ? tmp : -tmp;
    return (b.stringField != null) ? 2.0 * tmp : 0.5 * tmp;
  }

  /// CHECK-START: double Main.constructBase(double) inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(double) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBase(double) inliner (after)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: double Main.constructBase(double) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(double doubleValue) {
    Base b = new Base(doubleValue);
    return b.intField + b.doubleField;
  }

  /// CHECK-START: double Main.constructBaseWith0d() inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      DoubleConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBaseWith0d() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBaseWith0d() {
    Base b = new Base(0.0);
    return b.intField + b.doubleField;
  }

  /// CHECK-START: double Main.constructBase(java.lang.Object) inliner (before)
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(java.lang.Object) inliner (after)
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(java.lang.Object) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(Object objectValue) {
    Base b = new Base(objectValue);
    double tmp = b.intField + b.doubleField;
    return (b.objectField != null) ? tmp + 1.0 : tmp - 1.0;
  }

  /// CHECK-START: double Main.constructBase(int, long) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<JValue:j\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<JValue>>{{(,[ij]\d+)?}}] method_name:Base.<init>

  /// CHECK-START: double Main.constructBase(int, long) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructBase(int, long) inliner (after)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<IValue>>]

  /// CHECK-START: double Main.constructBase(int, long) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructBase(int intValue, long dummy) {
    Base b = new Base(intValue, dummy);
    return b.intField + b.doubleField;
  }

  /// CHECK-START: double Main.constructDerived() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived() {
    Derived d = new Derived();
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerived(int) inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(int) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructDerived(int) inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: double Main.constructDerived(int) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(int intValue) {
    Derived d = new Derived(intValue);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWith0() inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerivedWith0() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWith0() {
    Derived d = new Derived(0);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: java.lang.String Main.constructDerived(java.lang.String) inliner (before)
  /// CHECK-DAG:  <<Value:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: java.lang.String Main.constructDerived(java.lang.String) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: java.lang.String Main.constructDerived(java.lang.String) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static String constructDerived(String stringValue) {
    Derived d = new Derived(stringValue);
    return d.stringField;
  }

  /// CHECK-START: double Main.constructDerived(double) inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(double) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructDerived(double) inliner (after)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: double Main.constructDerived(double) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(double doubleValue) {
    Derived d = new Derived(doubleValue);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWith0d() inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      DoubleConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerivedWith0d() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWith0d() {
    Derived d = new Derived(0.0);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,<<OValue>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object) inliner (after)
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<DValue>>]

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(int intValue, double doubleValue, Object objectValue) {
    Derived d = new Derived(intValue, doubleValue, objectValue);
    double tmp = d.intField + d.doubleField;
    return (d.objectField != null) ? tmp : -tmp;
  }

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, java.lang.String) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,{{l\d+}},{{l\d+}}{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, java.lang.String) inliner (after)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,{{l\d+}},{{l\d+}}{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, java.lang.String) inliner (after)
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(
      int intValue, double doubleValue, Object objectValue, String stringValue) {
    Derived d = new Derived(intValue, doubleValue, objectValue, stringValue);
    double tmp = d.intField + d.doubleField;
    tmp = (d.objectField != null) ? tmp : -tmp;
    return (d.stringField != null) ? 2.0 * tmp : 0.5 * tmp;
  }

  /// CHECK-START: double Main.constructDerived(float) inliner (before)
  /// CHECK-DAG:  <<Value:f\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(float) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructDerived(float) inliner (after)
  /// CHECK-DAG:  <<Value:f\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]

  /// CHECK-START: double Main.constructDerived(float) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(float floatValue) {
    Derived d = new Derived(floatValue);
    return d.intField + d.doubleField + d.floatField;
  }

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, float) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<OValue:l\d+>>     ParameterValue
  /// CHECK-DAG:  <<FValue:f\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>,<<OValue>>,<<FValue>>{{(,[ij]\d+)?}}] method_name:Derived.<init>

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, float) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, float) inliner (after)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<FValue:f\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<IValue>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<DValue>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<FValue>>]

  /// CHECK-START: double Main.constructDerived(int, double, java.lang.Object, float) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerived(
      int intValue, double doubleValue, Object objectValue, float floatValue) {
    Derived d = new Derived(intValue, doubleValue, objectValue, floatValue);
    double tmp = d.intField + d.doubleField + d.floatField;
    return (d.objectField != null) ? tmp : -tmp;
  }

  /// CHECK-START: int Main.constructBaseWithFinalField() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:BaseWithFinalField.<init>

  /// CHECK-START: int Main.constructBaseWithFinalField() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructBaseWithFinalField() {
    BaseWithFinalField b = new BaseWithFinalField();
    return b.intField;
  }

  /// CHECK-START: int Main.constructBaseWithFinalField(int) inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:BaseWithFinalField.<init>

  /// CHECK-START: int Main.constructBaseWithFinalField(int) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.constructBaseWithFinalField(int) inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]
  /// CHECK-DAG:                      ConstructorFence

  /// CHECK-START: int Main.constructBaseWithFinalField(int) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructBaseWithFinalField(int intValue) {
    BaseWithFinalField b = new BaseWithFinalField(intValue);
    return b.intField;
  }

  /// CHECK-START: int Main.constructBaseWithFinalFieldWith0() inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:BaseWithFinalField.<init>

  /// CHECK-START: int Main.constructBaseWithFinalFieldWith0() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructBaseWithFinalFieldWith0() {
    BaseWithFinalField b = new BaseWithFinalField(0);
    return b.intField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalField() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalField() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalField() {
    DerivedWithFinalField d = new DerivedWithFinalField();
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalField(int) inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalField(int) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: double Main.constructDerivedWithFinalField(int) inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]
  /// CHECK-DAG:                      ConstructorFence

  /// CHECK-START: double Main.constructDerivedWithFinalField(int) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalField(int intValue) {
    DerivedWithFinalField d = new DerivedWithFinalField(intValue);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0() inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalFieldWith0() {
    DerivedWithFinalField d = new DerivedWithFinalField(0);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalField(double) inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalField(double) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: double Main.constructDerivedWithFinalField(double) inliner (after)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]
  /// CHECK-DAG:                      ConstructorFence

  /// CHECK-START: double Main.constructDerivedWithFinalField(double) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalField(double doubleValue) {
    DerivedWithFinalField d = new DerivedWithFinalField(doubleValue);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0d() inliner (before)
  /// CHECK-DAG:  <<Value:d\d+>>      DoubleConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0d() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalFieldWith0d() {
    DerivedWithFinalField d = new DerivedWithFinalField(0.0);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalField(int, double) inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     ParameterValue
  /// CHECK-DAG:  <<DValue:d\d+>>     ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalField(int, double) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: double Main.constructDerivedWithFinalField(int, double) inliner (after)
  /// CHECK-DAG:  <<Value:d\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-DAG:                      InstanceFieldSet [<<Obj>>,<<Value>>]
  /// CHECK-DAG:                      ConstructorFence

  /// CHECK-START: double Main.constructDerivedWithFinalField(int, double) inliner (after)
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK-NOT:                      InstanceFieldSet

  /// CHECK-START: double Main.constructDerivedWithFinalField(int, double) inliner (after)
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-DAG:                      ConstructorFence
  /// CHECK-NOT:                      ConstructorFence

  public static double constructDerivedWithFinalField(int intValue, double doubleValue) {
    DerivedWithFinalField d = new DerivedWithFinalField(intValue, doubleValue);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0And0d() inliner (before)
  /// CHECK-DAG:  <<IValue:i\d+>>     IntConstant 0
  /// CHECK-DAG:  <<DValue:d\d+>>     DoubleConstant
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<IValue>>,<<DValue>>{{(,[ij]\d+)?}}] method_name:DerivedWithFinalField.<init>

  /// CHECK-START: double Main.constructDerivedWithFinalFieldWith0And0d() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static double constructDerivedWithFinalFieldWith0And0d() {
    DerivedWithFinalField d = new DerivedWithFinalField(0, 0.0);
    return d.intField + d.doubleField;
  }

  /// CHECK-START: int Main.constructDerivedInSecondDex() inliner (before)
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDex() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructDerivedInSecondDex() {
    DerivedInSecondDex d = new DerivedInSecondDex();
    return d.intField;
  }

  /// CHECK-START: int Main.constructDerivedInSecondDex(int) inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDex(int) inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDex(int) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructDerivedInSecondDex(int intValue) {
    DerivedInSecondDex d = new DerivedInSecondDex(intValue);
    return d.intField;
  }

  /// CHECK-START: int Main.constructDerivedInSecondDexWith0() inliner (before)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDexWith0() inliner (after)
  /// CHECK-DAG:  <<Value:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDexWith0() inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructDerivedInSecondDexWith0() {
    DerivedInSecondDex d = new DerivedInSecondDex(0);
    return d.intField;
  }

  /// CHECK-START: int Main.constructDerivedInSecondDex(long) inliner (before)
  /// CHECK-DAG:  <<Value:j\d+>>      ParameterValue
  /// CHECK-DAG:  <<Obj:l\d+>>        NewInstance
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Obj>>,<<Value>>{{(,[ij]\d+)?}}] method_name:DerivedInSecondDex.<init>

  /// CHECK-START: int Main.constructDerivedInSecondDex(long) inliner (after)
  /// CHECK:                          ConstructorFence
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-NOT:                      InstanceFieldSet

  public static int constructDerivedInSecondDex(long dummy) {
    DerivedInSecondDex d = new DerivedInSecondDex(dummy);
    return d.intField;
  }

  public static void main(String[] args) throws Exception {
    Second s = new Second();

    // Replaced NOP pattern.
    staticNop();
    nop(s);
    // Replaced "return arg" pattern.
    assertEquals("arbitrary string", staticReturnArg2("arbitrary string"));
    assertEquals(4321L, returnArg1(s, 4321L));
    // Replaced "return const" pattern.
    assertEquals(9, staticReturn9());
    assertEquals(7, return7(s));
    assertEquals(null, staticReturnNull());
    assertEquals(null, returnNull(s));
    // Replaced IGET pattern.
    assertEquals(42, getInt(s));
    assertEquals(-42.0, getDouble(s));
    assertEquals(null, getObject(s));
    assertEquals("dummy", getString(s));
    // Not replaced IGET pattern.
    assertEquals(42, staticGetInt(s));
    assertEquals(-42.0, getDoubleFromParam(s));
    // SGET.
    assertEquals(4242, getStaticInt(s));
    // Replaced IPUT pattern.
    assertEquals(111L, setLong(s, 111L));
    assertEquals(345L, setLongReturnArg2(s, 222L, 123));
    // Not replaced IPUT pattern.
    assertEquals(222L, staticSetLong(s, 222L));
    assertEquals(333L, setLongThroughParam(s, 333L));
    // SPUT.
    assertEquals(-11.5f, setStaticFloat(s, -11.5f));

    if (newObject() == null) {
      throw new AssertionError("new Object() cannot be null.");
    }

    assertEquals(0.0, constructBase());
    assertEquals(42.0, constructBase(42));
    assertEquals(0.0, constructBaseWith0());
    assertEquals("something", constructBase("something"));
    assertEquals(null, constructBaseWithNullString());
    assertEquals(11.0, constructBase(11.0, new Object()));
    assertEquals(-12.0, constructBase(12.0, null));
    assertEquals(30.0, constructBase(17, 13.0, new Object()));
    assertEquals(-34.0, constructBase(19, 15.0, null));
    assertEquals(-22.5, constructBaseWith0DoubleNull(22.5));
    assertEquals(-8.0, constructBase(2, 14.0, null, null));
    assertEquals(-64.0, constructBase(4, 28.0, null, "dummy"));
    assertEquals(13.0, constructBase(24, 2.0, new Object(), null));
    assertEquals(30.0, constructBase(11, 4.0, new Object(), "dummy"));
    assertEquals(43.0, constructBase(43.0));
    assertEquals(0.0, constructBaseWith0d());
    assertEquals(1.0, constructBase(new Object()));
    assertEquals(-1.0, constructBase((Object) null));
    assertEquals(123.0, constructBase(123, 65L));

    assertEquals(0.0, constructDerived());
    assertEquals(73.0, constructDerived(73));
    assertEquals(0.0, constructDerivedWith0());
    assertEquals(null, constructDerived("something else"));
    assertEquals(18.0, constructDerived(18.0));
    assertEquals(0.0, constructDerivedWith0d());
    assertEquals(-7.0, constructDerived(5, 7.0, new Object()));
    assertEquals(-4.0, constructDerived(9, 4.0, null));
    assertEquals(0.0, constructDerived(1, 9.0, null, null));
    assertEquals(0.0, constructDerived(2, 8.0, null, "dummy"));
    assertEquals(0.0, constructDerived(3, 7.0, new Object(), null));
    assertEquals(0.0, constructDerived(4, 6.0, new Object(), "dummy"));
    assertEquals(17.0, constructDerived(17.0f));
    assertEquals(-5.5, constructDerived(6, -7.0, new Object(), 6.5f));

    assertEquals(0, constructBaseWithFinalField());
    assertEquals(77, constructBaseWithFinalField(77));
    assertEquals(0, constructBaseWithFinalFieldWith0());
    assertEquals(0.0, constructDerivedWithFinalField());
    assertEquals(-33.0, constructDerivedWithFinalField(-33));
    assertEquals(0.0, constructDerivedWithFinalFieldWith0());
    assertEquals(-44.0, constructDerivedWithFinalField(-44.0));
    assertEquals(0.0, constructDerivedWithFinalFieldWith0d());
    assertEquals(88, constructDerivedWithFinalField(22, 66.0));
    assertEquals(0.0, constructDerivedWithFinalFieldWith0And0d());

    assertEquals(0, constructDerivedInSecondDex());
    assertEquals(123, constructDerivedInSecondDex(123));
    assertEquals(0, constructDerivedInSecondDexWith0());
    assertEquals(0, constructDerivedInSecondDex(7L));
  }

  private static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Wrong result: " + expected + " != " + actual);
    }
  }

  private static void assertEquals(double expected, double actual) {
    if (expected != actual) {
      throw new AssertionError("Wrong result: " + expected + " != " + actual);
    }
  }

  private static void assertEquals(Object expected, Object actual) {
    if (expected != actual && (expected == null || !expected.equals(actual))) {
      throw new AssertionError("Wrong result: " + expected + " != " + actual);
    }
  }
}
