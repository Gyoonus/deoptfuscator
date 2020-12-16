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

  /// CHECK-START-X86: int Main.p(float) liveness (after)
  /// CHECK:         <<Arg:f\d+>>  ParameterValue uses:[<<UseInput:\d+>>]
  /// CHECK-DAG:     <<Five:f\d+>> FloatConstant 5 uses:[<<UseInput>>]
  /// CHECK-DAG:     <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:     <<MinusOne:i\d+>> IntConstant -1 uses:[<<UseInput>>]
  /// CHECK:         <<Base:i\d+>> X86ComputeBaseMethodAddress uses:[<<UseInput>>]
  /// CHECK-NEXT:    <<Load:f\d+>> X86LoadFromConstantTable [<<Base>>,<<Five>>]
  /// CHECK-NEXT:    <<Cond:z\d+>> LessThanOrEqual [<<Arg>>,<<Load>>]
  /// CHECK-NEXT:                  Select [<<Zero>>,<<MinusOne>>,<<Cond>>] liveness:<<LivSel:\d+>>
  /// CHECK-EVAL:    <<UseInput>> == <<LivSel>> + 1

  public static int p(float arg) {
    return (arg > 5.0f) ? 0 : -1;
  }

  /// CHECK-START: void Main.main(java.lang.String[]) liveness (after)
  /// CHECK:         <<X:i\d+>>    ArrayLength uses:[<<UseInput:\d+>>]
  /// CHECK:         <<Y:i\d+>>    StaticFieldGet uses:[<<UseInput>>]
  /// CHECK:         <<Cond:z\d+>> LessThanOrEqual [<<X>>,<<Y>>]
  /// CHECK-NEXT:                  If [<<Cond>>] liveness:<<LivIf:\d+>>
  /// CHECK-EVAL:    <<UseInput>> == <<LivIf>> + 1

  public static void main(String[] args) {
    int x = args.length;
    int y = field;
    if (x > y) {
      System.nanoTime();
    }
  }

  public static int field = 42;
}
