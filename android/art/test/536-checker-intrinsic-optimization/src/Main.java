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

import java.lang.reflect.Method;

public class Main {
  public static boolean doThrow = false;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertBooleanEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertStringContains(String searchTerm, String result) {
    if (result == null || !result.contains(searchTerm)) {
      throw new Error("Search term: " + searchTerm + ", not found in: " + result);
    }
  }

  public static void main(String[] args) {
    stringEqualsSame();
    stringArgumentNotNull("Foo");

    assertIntEquals(0, $opt$noinline$getStringLength(""));
    assertIntEquals(3, $opt$noinline$getStringLength("abc"));
    assertIntEquals(10, $opt$noinline$getStringLength("0123456789"));

    assertBooleanEquals(true, $opt$noinline$isStringEmpty(""));
    assertBooleanEquals(false, $opt$noinline$isStringEmpty("abc"));
    assertBooleanEquals(false, $opt$noinline$isStringEmpty("0123456789"));

    assertCharEquals('a', $opt$noinline$stringCharAt("a", 0));
    assertCharEquals('a', $opt$noinline$stringCharAt("abc", 0));
    assertCharEquals('b', $opt$noinline$stringCharAt("abc", 1));
    assertCharEquals('c', $opt$noinline$stringCharAt("abc", 2));
    assertCharEquals('7', $opt$noinline$stringCharAt("0123456789", 7));

    try {
      $opt$noinline$stringCharAt("abc", -1);
      throw new Error("Should throw SIOOB.");
    } catch (StringIndexOutOfBoundsException sioob) {
      assertStringContains("java.lang.String.charAt", sioob.getStackTrace()[0].toString());
      assertStringContains("Main.$opt$noinline$stringCharAt", sioob.getStackTrace()[1].toString());
    }
    try {
      $opt$noinline$stringCharAt("abc", 3);
      throw new Error("Should throw SIOOB.");
    } catch (StringIndexOutOfBoundsException sioob) {
      assertStringContains("java.lang.String.charAt", sioob.getStackTrace()[0].toString());
      assertStringContains("Main.$opt$noinline$stringCharAt", sioob.getStackTrace()[1].toString());
    }
    try {
      $opt$noinline$stringCharAt("abc", Integer.MAX_VALUE);
      throw new Error("Should throw SIOOB.");
    } catch (StringIndexOutOfBoundsException sioob) {
      assertStringContains("java.lang.String.charAt", sioob.getStackTrace()[0].toString());
      assertStringContains("Main.$opt$noinline$stringCharAt", sioob.getStackTrace()[1].toString());
    }

    assertCharEquals('7', $opt$noinline$stringCharAtCatch("0123456789", 7));
    assertCharEquals('7', $noinline$runSmaliTest("stringCharAtCatch", "0123456789", 7));
    assertCharEquals('\0', $opt$noinline$stringCharAtCatch("0123456789", 10));
    assertCharEquals('\0', $noinline$runSmaliTest("stringCharAtCatch","0123456789", 10));

    assertIntEquals('a' + 'b' + 'c', $opt$noinline$stringSumChars("abc"));
    assertIntEquals('a' + 'b' + 'c', $opt$noinline$stringSumLeadingChars("abcdef", 3));
    try {
      $opt$noinline$stringSumLeadingChars("abcdef", 7);
      throw new Error("Should throw SIOOB.");
    } catch (StringIndexOutOfBoundsException sioob) {
      assertStringContains("java.lang.String.charAt", sioob.getStackTrace()[0].toString());
      assertStringContains("Main.$opt$noinline$stringSumLeadingChars",
                           sioob.getStackTrace()[1].toString());
    }
    assertIntEquals('a' + 'b' + 'c' + 'd', $opt$noinline$stringSum4LeadingChars("abcdef"));
    try {
      $opt$noinline$stringSum4LeadingChars("abc");
      throw new Error("Should throw SIOOB.");
    } catch (StringIndexOutOfBoundsException sioob) {
      assertStringContains("java.lang.String.charAt", sioob.getStackTrace()[0].toString());
      assertStringContains("Main.$opt$noinline$stringSum4LeadingChars",
                           sioob.getStackTrace()[1].toString());
    }
  }

  /// CHECK-START: int Main.$opt$noinline$getStringLength(java.lang.String) instruction_simplifier (before)
  /// CHECK-DAG:  <<Length:i\d+>>   InvokeVirtual intrinsic:StringLength
  /// CHECK-DAG:                    Return [<<Length>>]

  /// CHECK-START: int Main.$opt$noinline$getStringLength(java.lang.String) instruction_simplifier (after)
  /// CHECK-DAG:  <<String:l\d+>>   ParameterValue
  /// CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
  /// CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
  /// CHECK-DAG:                    Return [<<Length>>]

  /// CHECK-START: int Main.$opt$noinline$getStringLength(java.lang.String) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringLength

  static public int $opt$noinline$getStringLength(String s) {
    if (doThrow) { throw new Error(); }
    return s.length();
  }

  /// CHECK-START: boolean Main.$opt$noinline$isStringEmpty(java.lang.String) instruction_simplifier (before)
  /// CHECK-DAG:  <<IsEmpty:z\d+>>  InvokeVirtual intrinsic:StringIsEmpty
  /// CHECK-DAG:                    Return [<<IsEmpty>>]

  /// CHECK-START: boolean Main.$opt$noinline$isStringEmpty(java.lang.String) instruction_simplifier (after)
  /// CHECK-DAG:  <<String:l\d+>>   ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
  /// CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
  /// CHECK-DAG:  <<IsEmpty:z\d+>>  Equal [<<Length>>,<<Const0>>]
  /// CHECK-DAG:                    Return [<<IsEmpty>>]

  /// CHECK-START: boolean Main.$opt$noinline$isStringEmpty(java.lang.String) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringIsEmpty

  static public boolean $opt$noinline$isStringEmpty(String s) {
    if (doThrow) { throw new Error(); }
    return s.isEmpty();
  }

  /// CHECK-START: char Main.$opt$noinline$stringCharAt(java.lang.String, int) instruction_simplifier (before)
  /// CHECK-DAG:  <<Char:c\d+>>     InvokeVirtual intrinsic:StringCharAt
  /// CHECK-DAG:                    Return [<<Char>>]

  /// CHECK-START: char Main.$opt$noinline$stringCharAt(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-DAG:  <<String:l\d+>>   ParameterValue
  /// CHECK-DAG:  <<Pos:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
  /// CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
  /// CHECK-DAG:  <<Bounds:i\d+>>   BoundsCheck [<<Pos>>,<<Length>>] is_string_char_at:true
  /// CHECK-DAG:  <<Char:c\d+>>     ArrayGet [<<NullCk>>,<<Bounds>>] is_string_char_at:true
  /// CHECK-DAG:                    Return [<<Char>>]

  /// CHECK-START: char Main.$opt$noinline$stringCharAt(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt

  static public char $opt$noinline$stringCharAt(String s, int pos) {
    if (doThrow) { throw new Error(); }
    return s.charAt(pos);
  }

  /// CHECK-START: char Main.$opt$noinline$stringCharAtCatch(java.lang.String, int) instruction_simplifier (before)
  /// CHECK-DAG:  <<Int:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<Char:c\d+>>     InvokeVirtual intrinsic:StringCharAt

  //                                The return value can come from a Phi should the two returns be merged.
  //                                Please refer to the Smali code for a more detailed verification.

  /// CHECK-DAG:                    Return [{{(c|i)\d+}}]

  /// CHECK-START: char Main.$opt$noinline$stringCharAtCatch(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-DAG:  <<String:l\d+>>   ParameterValue
  /// CHECK-DAG:  <<Pos:i\d+>>      ParameterValue
  /// CHECK-DAG:  <<Int:i\d+>>      IntConstant 0
  /// CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
  /// CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
  /// CHECK-DAG:  <<Bounds:i\d+>>   BoundsCheck [<<Pos>>,<<Length>>] is_string_char_at:true
  /// CHECK-DAG:  <<Char:c\d+>>     ArrayGet [<<NullCk>>,<<Bounds>>] is_string_char_at:true
  /// CHECK-DAG:                    Return [{{(c|i)\d+}}]

  /// CHECK-START: char Main.$opt$noinline$stringCharAtCatch(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt

  static public char $opt$noinline$stringCharAtCatch(String s, int pos) {
    if (doThrow) { throw new Error(); }
    try {
      return s.charAt(pos);
    } catch (StringIndexOutOfBoundsException ignored) {
      return '\0';
    }
  }

  /// CHECK-START: int Main.$opt$noinline$stringSumChars(java.lang.String) instruction_simplifier (before)
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringLength
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSumChars(java.lang.String) instruction_simplifier (after)
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true

  /// CHECK-START: int Main.$opt$noinline$stringSumChars(java.lang.String) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringLength
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSumChars(java.lang.String) GVN (after)
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-NOT:                    ArrayLength is_string_length:true

  /// CHECK-START: int Main.$opt$noinline$stringSumChars(java.lang.String) BCE (after)
  /// CHECK-NOT:                    BoundsCheck

  static public int $opt$noinline$stringSumChars(String s) {
    if (doThrow) { throw new Error(); }
    int sum = 0;
    int len = s.length();
    for (int i = 0; i < len; ++i) {
      sum += s.charAt(i);
    }
    return sum;
  }

  /// CHECK-START: int Main.$opt$noinline$stringSumLeadingChars(java.lang.String, int) instruction_simplifier (before)
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSumLeadingChars(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true

  /// CHECK-START: int Main.$opt$noinline$stringSumLeadingChars(java.lang.String, int) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSumLeadingChars(java.lang.String, int) BCE (after)
  /// CHECK-DAG:                    Deoptimize env:[[{{[^\]]*}}]]

  /// CHECK-START: int Main.$opt$noinline$stringSumLeadingChars(java.lang.String, int) BCE (after)
  /// CHECK-NOT:                    BoundsCheck is_string_char_at:true

  static public int $opt$noinline$stringSumLeadingChars(String s, int n) {
    if (doThrow) { throw new Error(); }
    int sum = 0;
    for (int i = 0; i < n; ++i) {
      sum += s.charAt(i);
    }
    return sum;
  }

  /// CHECK-START: int Main.$opt$noinline$stringSum4LeadingChars(java.lang.String) instruction_simplifier (before)
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt
  /// CHECK-DAG:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSum4LeadingChars(java.lang.String) instruction_simplifier (after)
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true
  /// CHECK-DAG:                    ArrayLength is_string_length:true
  /// CHECK-DAG:                    BoundsCheck is_string_char_at:true
  /// CHECK-DAG:                    ArrayGet is_string_char_at:true

  /// CHECK-START: int Main.$opt$noinline$stringSum4LeadingChars(java.lang.String) instruction_simplifier (after)
  /// CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt

  /// CHECK-START: int Main.$opt$noinline$stringSum4LeadingChars(java.lang.String) BCE (after)
  /// CHECK-DAG:                    Deoptimize env:[[{{[^\]]*}}]]

  /// CHECK-START: int Main.$opt$noinline$stringSum4LeadingChars(java.lang.String) BCE (after)
  /// CHECK-NOT:                    BoundsCheck is_string_char_at:true

  static public int $opt$noinline$stringSum4LeadingChars(String s) {
    if (doThrow) { throw new Error(); }
    int sum = s.charAt(0) + s.charAt(1) + s.charAt(2) + s.charAt(3);
    return sum;
  }

  /// CHECK-START: boolean Main.stringEqualsSame() instruction_simplifier (before)
  /// CHECK:      InvokeStaticOrDirect

  /// CHECK-START: boolean Main.stringEqualsSame() register (before)
  /// CHECK:      <<Const1:i\d+>> IntConstant 1
  /// CHECK:      Return [<<Const1>>]

  /// CHECK-START: boolean Main.stringEqualsSame() register (before)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static boolean stringEqualsSame() {
    return $inline$callStringEquals("obj", "obj");
  }

  /// CHECK-START: boolean Main.stringEqualsNull() register (after)
  /// CHECK:      <<Invoke:z\d+>> InvokeVirtual
  /// CHECK:      Return [<<Invoke>>]
  public static boolean stringEqualsNull() {
    String o = (String)myObject;
    return $inline$callStringEquals(o, o);
  }

  public static boolean $inline$callStringEquals(String a, String b) {
    return a.equals(b);
  }

  /// CHECK-START-X86: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  /// CHECK-NOT:      test

  /// CHECK-START-X86_64: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  /// CHECK-NOT:      test

  /// CHECK-START-ARM: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  // CompareAndBranchIfZero() may emit either CBZ or CMP+BEQ.
  /// CHECK-NOT:      cbz
  /// CHECK-NOT:      cmp {{r\d+}}, #0
  // Terminate the scope for the CHECK-NOT search at the reference or length comparison,
  // whichever comes first.
  /// CHECK:          cmp {{r\d+}}, {{r\d+}}

  /// CHECK-START-ARM64: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  /// CHECK-NOT:      cbz
  // Terminate the scope for the CHECK-NOT search at the reference or length comparison,
  // whichever comes first.
  /// CHECK:          cmp {{w.*,}} {{w.*|#.*}}

  /// CHECK-START-MIPS: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  /// CHECK-NOT:      beq zero,
  /// CHECK-NOT:      beqz
  /// CHECK-NOT:      beqzc
  // Terminate the scope for the CHECK-NOT search at the class field or length comparison,
  // whichever comes first.
  /// CHECK:          lw

  /// CHECK-START-MIPS64: boolean Main.stringArgumentNotNull(java.lang.Object) disassembly (after)
  /// CHECK:          InvokeVirtual {{.*\.equals.*}} intrinsic:StringEquals
  /// CHECK-NOT:      beqzc
  // Terminate the scope for the CHECK-NOT search at the reference comparison.
  /// CHECK:          beqc
  public static boolean stringArgumentNotNull(Object obj) {
    obj.getClass();
    return "foo".equals(obj);
  }

  // Test is very brittle as it depends on the order we emit instructions.
  /// CHECK-START-X86: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          test
  /// CHECK:          jz/eq
  // Check that we don't try to compare the classes.
  /// CHECK-NOT:      mov
  /// CHECK:          cmp

  // Test is very brittle as it depends on the order we emit instructions.
  /// CHECK-START-X86_64: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          test
  /// CHECK:          jz/eq
  // Check that we don't try to compare the classes.
  /// CHECK-NOT:      mov
  /// CHECK:          cmp

  // Test is brittle as it depends on the class offset being 0.
  /// CHECK-START-ARM: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          {{cbz|cmp}}
  // Check that we don't try to compare the classes.
  // The dissassembler currently explicitly emits the offset 0 but don't rely on it.
  // We want to terminate the CHECK-NOT search after two CMPs, one for reference
  // equality and one for length comparison but these may be emitted in different order,
  // so repeat the check twice.
  /// CHECK-NOT:      ldr{{(|.w)}} {{r\d+}}, [{{r\d+}}]
  /// CHECK-NOT:      ldr{{(|.w)}} {{r\d+}}, [{{r\d+}}, #0]
  /// CHECK:          cmp {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:      ldr{{(|.w)}} {{r\d+}}, [{{r\d+}}]
  /// CHECK-NOT:      ldr{{(|.w)}} {{r\d+}}, [{{r\d+}}, #0]
  /// CHECK:          cmp {{r\d+}}, {{r\d+}}

  // Test is brittle as it depends on the class offset being 0.
  /// CHECK-START-ARM64: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          cbz
  // Check that we don't try to compare the classes.
  // The dissassembler currently does not explicitly emits the offset 0 but don't rely on it.
  // We want to terminate the CHECK-NOT search after two CMPs, one for reference
  // equality and one for length comparison but these may be emitted in different order,
  // so repeat the check twice.
  /// CHECK-NOT:      ldr {{w\d+}}, [{{x\d+}}]
  /// CHECK-NOT:      ldr {{w\d+}}, [{{x\d+}}, #0]
  /// CHECK:          cmp {{w\d+}}, {{w\d+|#.*}}
  /// CHECK-NOT:      ldr {{w\d+}}, [{{x\d+}}]
  /// CHECK-NOT:      ldr {{w\d+}}, [{{x\d+}}, #0]
  /// CHECK:          cmp {{w\d+}}, {{w\d+|#.*}}

  // Test is brittle as it depends on the class offset being 0.
  /// CHECK-START-MIPS: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          beq{{(zc)?}}
  // Check that we don't try to compare the classes.
  /// CHECK-NOT:      lw {{r\d+}}, +0({{r\d+}})
  /// CHECK:          bne{{c?}}

  // Test is brittle as it depends on the class offset being 0.
  /// CHECK-START-MIPS64: boolean Main.stringArgumentIsString() disassembly (after)
  /// CHECK:          InvokeVirtual intrinsic:StringEquals
  /// CHECK:          beqzc
  // Check that we don't try to compare the classes.
  /// CHECK-NOT:      lw {{r\d+}}, +0({{r\d+}})
  /// CHECK:          bnec
  public static boolean stringArgumentIsString() {
    return "foo".equals(myString);
  }

  static String myString;
  static Object myObject;

  public static char $noinline$runSmaliTest(String name, String str, int pos) {
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name, String.class, int.class);
      return (Character) m.invoke(null, str, pos);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }
}
