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


public class Main {

  /// CHECK-START: void Main.loop1(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>>  ranges:{[<<ArgLiv>>,<<ArgLoopUse:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse>>]
  /// CHECK:                       If [<<Arg>>]    liveness:<<IfLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv:\d+>>
  /// CHECK:                       Exit
  /// CHECK-EVAL:    <<IfLiv>> + 1 == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv>> + 2 == <<ArgLoopUse>>

  public static void loop1(boolean incoming) {
    while (incoming) {}
  }

  /// CHECK-START: void Main.loop2(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse2:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse1:\d+>>,<<ArgLoopUse2>>]
  /// CHECK:                       If [<<Arg>>]    liveness:<<IfLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK-EVAL:    <<IfLiv>> + 1 == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv1>> + 2 == <<ArgLoopUse1>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse2>>

  public static void loop2(boolean incoming) {
    // Add some code at entry to avoid having the entry block be a pre header.
    // This avoids having to create a synthesized block.
    System.out.println("Enter");
    while (true) {
      System.out.println("foo");
      while (incoming) {}
    }
  }

  /// CHECK-START: void Main.loop3(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse>>]
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       InvokeVirtual   [{{l\d+}},<<Arg>>] method_name:java.io.PrintStream.println liveness:<<InvokeLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK-EVAL:    <<InvokeLiv>> == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse>>

  public static void loop3(boolean incoming) {
    // 'incoming' only needs a use at the outer loop's back edge.
    while (System.currentTimeMillis() != 42) {
      while (Runtime.getRuntime() != null) {}
      System.out.println(incoming);
    }
  }

  /// CHECK-START: void Main.loop4(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>> ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgUse:\d+>>)} uses:[<<ArgUse>>]
  /// CHECK:                      InvokeVirtual   [{{l\d+}},<<Arg>>] method_name:java.io.PrintStream.println liveness:<<InvokeLiv:\d+>>
  /// CHECK-EVAL:    <<InvokeLiv>> == <<ArgUse>>

  public static void loop4(boolean incoming) {
    // 'incoming' has no loop use, so should not have back edge uses.
    System.out.println(incoming);
    while (System.currentTimeMillis() != 42) {
      while (Runtime.getRuntime() != null) {}
    }
  }

  /// CHECK-START: void Main.loop5(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse2:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse1:\d+>>,<<ArgLoopUse2>>]
  /// CHECK:                       InvokeVirtual   [{{l\d+}},<<Arg>>] method_name:java.io.PrintStream.println liveness:<<InvokeLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK:                       Exit
  /// CHECK-EVAL:    <<InvokeLiv>> == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv1>> + 2 == <<ArgLoopUse1>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse2>>

  public static void loop5(boolean incoming) {
    // 'incoming' must have a use at both back edges.
    for (long i = System.nanoTime(); i < 42; ++i) {
      for (long j = System.currentTimeMillis(); j != 42; ++j) {
        System.out.println(incoming);
      }
    }
  }

  /// CHECK-START: void Main.loop6(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse>>]
  /// CHECK:                       InvokeVirtual   [{{l\d+}},<<Arg>>] method_name:java.io.PrintStream.println liveness:<<InvokeLiv:\d+>>
  /// CHECK:                       Add
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       Add
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK:                       Exit
  /// CHECK-EVAL:    <<InvokeLiv>> == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse>>

  public static void loop6(boolean incoming) {
    // 'incoming' must have a use only at the first loop's back edge.
    for (long i = System.nanoTime(); i < 42; ++i) {
      System.out.println(incoming);
      for (long j = System.currentTimeMillis(); j != 42; ++j) {
        System.out.print(j);  // non-empty body
      }
    }
  }

  /// CHECK-START: void Main.loop7(boolean) liveness (after)
  /// CHECK:         <<Arg:z\d+>>  ParameterValue  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse2:\d+>>)} uses:[<<ArgUse1:\d+>>,<<ArgUse2:\d+>>,<<ArgLoopUse1:\d+>>,<<ArgLoopUse2>>]
  /// CHECK:                       InvokeVirtual   [{{l\d+}},<<Arg>>] method_name:java.io.PrintStream.println liveness:<<InvokeLiv:\d+>>
  /// CHECK:                       If              [<<Arg>>] liveness:<<IfLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK:                       Exit
  /// CHECK-EVAL:    <<InvokeLiv>> == <<ArgUse1>>
  /// CHECK-EVAL:    <<IfLiv>> + 1 == <<ArgUse2>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv1>> + 2 == <<ArgLoopUse1>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse2>>

  public static void loop7(boolean incoming) {
    // 'incoming' must have a use at both back edges.
    while (Runtime.getRuntime() != null) {
      System.out.println(incoming);
      while (incoming) {}
      System.nanoTime();  // beat back edge splitting
    }
  }

  /// CHECK-START: void Main.loop8() liveness (after)
  /// CHECK:         <<Arg:z\d+>>  StaticFieldGet  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse2:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse1:\d+>>,<<ArgLoopUse2>>]
  /// CHECK:                       If [<<Arg>>]    liveness:<<IfLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK:                       Exit
  /// CHECK-EVAL:    <<IfLiv>> + 1 == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv1>> + 2 == <<ArgLoopUse1>>
  /// CHECK-EVAL:    <<GotoLiv2>> + 2 == <<ArgLoopUse2>>

  public static void loop8() {
    // 'incoming' must have a use at both back edges.
    boolean incoming = field;
    while (Runtime.getRuntime() != null) {
      System.nanoTime();  // beat pre-header creation
      while (incoming) {}
      System.nanoTime();  // beat back edge splitting
    }
  }


  static boolean $opt$noinline$ensureSideEffects() {
    if (doThrow) throw new Error("");
    return true;
  }

  /// CHECK-START: void Main.loop9() liveness (after)
  /// CHECK:         <<Arg:z\d+>>  StaticFieldGet  liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopUse:\d+>>)} uses:[<<ArgUse:\d+>>,<<ArgLoopUse>>]
  /// CHECK:                       If [<<Arg>>]    liveness:<<IfLiv:\d+>>
  /// CHECK:                       Goto            liveness:<<GotoLiv1:\d+>>
  /// CHECK-DAG:                   Goto            liveness:<<GotoLiv2:\d+>>
  /// CHECK-DAG:                   Exit
  /// CHECK-EVAL:    <<IfLiv>> + 1 == <<ArgUse>>
  /// CHECK-EVAL:    <<GotoLiv1>> < <<GotoLiv2>>
  /// CHECK-EVAL:    <<GotoLiv1>> + 2 == <<ArgLoopUse>>

  public static void loop9() {
    // Add some code at entry to avoid having the entry block be a pre header.
    // This avoids having to create a synthesized block.
    System.out.println("Enter");
    while ($opt$noinline$ensureSideEffects()) {
      // 'incoming' must only have a use in the inner loop.
      boolean incoming = field;
      while (incoming) {}
    }
  }

  public static void main(String[] args) {
  }

  static boolean field;
  static boolean doThrow = false;
}
