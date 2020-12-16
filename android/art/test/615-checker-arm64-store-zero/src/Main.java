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

  public static boolean doThrow = false;

  public void $noinline$foo(int in_w1,
                            int in_w2,
                            int in_w3,
                            int in_w4,
                            int in_w5,
                            int in_w6,
                            int in_w7,
                            int on_stack_int,
                            long on_stack_long,
                            float in_s0,
                            float in_s1,
                            float in_s2,
                            float in_s3,
                            float in_s4,
                            float in_s5,
                            float in_s6,
                            float in_s7,
                            float on_stack_float,
                            double on_stack_double) {
    if (doThrow) throw new Error();
  }

  // We expect a parallel move that moves four times the zero constant to stack locations.
  /// CHECK-START-ARM64: void Main.bar() register (after)
  /// CHECK:             ParallelMove {{.*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*}}

  // Those four moves should generate four 'store' instructions using directly the zero register.
  /// CHECK-START-ARM64: void Main.bar() disassembly (after)
  /// CHECK-DAG:         {{(str|stur)}} wzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} xzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} wzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} xzr, [sp, #{{[0-9]+}}]

  public void bar() {
    $noinline$foo(1, 2, 3, 4, 5, 6, 7,     // Integral values in registers.
                  0, 0L,                   // Integral values on the stack.
                  1, 2, 3, 4, 5, 6, 7, 8,  // Floating-point values in registers.
                  0.0f, 0.0);              // Floating-point values on the stack.
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_byte_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static byte static_byte_field;

  public void store_zero_to_static_byte_field() {
    static_byte_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_char_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static char static_char_field;

  public void store_zero_to_static_char_field() {
    static_char_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_short_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static short static_short_field;

  public void store_zero_to_static_short_field() {
    static_short_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_int_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static int static_int_field;

  public void store_zero_to_static_int_field() {
    static_int_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_long_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static long static_long_field;

  public void store_zero_to_static_long_field() {
    static_long_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_float_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static float static_float_field;

  public void store_zero_to_static_float_field() {
    static_float_field = 0.0f;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_static_double_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public static double static_double_field;

  public void store_zero_to_static_double_field() {
    static_double_field = 0.0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_byte_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrb wzr, [<<temp>>]

  public static volatile byte volatile_static_byte_field;

  public void store_zero_to_volatile_static_byte_field() {
    volatile_static_byte_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_char_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrh wzr, [<<temp>>]

  public static volatile char volatile_static_char_field;

  public void store_zero_to_volatile_static_char_field() {
    volatile_static_char_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_short_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrh wzr, [<<temp>>]

  public static volatile short volatile_static_short_field;

  public void store_zero_to_volatile_static_short_field() {
    volatile_static_short_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_int_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr wzr, [<<temp>>]

  public static volatile int volatile_static_int_field;

  public void store_zero_to_volatile_static_int_field() {
    volatile_static_int_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_long_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr xzr, [<<temp>>]

  public static volatile long volatile_static_long_field;

  public void store_zero_to_volatile_static_long_field() {
    volatile_static_long_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_float_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr wzr, [<<temp>>]

  public static volatile float volatile_static_float_field;

  public void store_zero_to_volatile_static_float_field() {
    volatile_static_float_field = 0.0f;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_static_double_field() disassembly (after)
  /// CHECK:             StaticFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr xzr, [<<temp>>]

  public static volatile double volatile_static_double_field;

  public void store_zero_to_volatile_static_double_field() {
    volatile_static_double_field = 0.0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_byte_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public byte instance_byte_field;

  public void store_zero_to_instance_byte_field() {
    instance_byte_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_char_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public char instance_char_field;

  public void store_zero_to_instance_char_field() {
    instance_char_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_short_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public short instance_short_field;

  public void store_zero_to_instance_short_field() {
    instance_short_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_int_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public int instance_int_field;

  public void store_zero_to_instance_int_field() {
    instance_int_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_long_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public long instance_long_field;

  public void store_zero_to_instance_long_field() {
    instance_long_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_float_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public float instance_float_field;

  public void store_zero_to_instance_float_field() {
    instance_float_field = 0.0f;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_double_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  public double instance_double_field;

  public void store_zero_to_instance_double_field() {
    instance_double_field = 0.0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_byte_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrb wzr, [<<temp>>]

  public volatile byte volatile_instance_byte_field;

  public void store_zero_to_volatile_instance_byte_field() {
    volatile_instance_byte_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_char_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrh wzr, [<<temp>>]

  public volatile char volatile_instance_char_field;

  public void store_zero_to_volatile_instance_char_field() {
    volatile_instance_char_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_short_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlrh wzr, [<<temp>>]

  public volatile short volatile_instance_short_field;

  public void store_zero_to_volatile_instance_short_field() {
    volatile_instance_short_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_int_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr wzr, [<<temp>>]

  public volatile int volatile_instance_int_field;

  public void store_zero_to_volatile_instance_int_field() {
    volatile_instance_int_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_long_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr xzr, [<<temp>>]

  public volatile long volatile_instance_long_field;

  public void store_zero_to_volatile_instance_long_field() {
    volatile_instance_long_field = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_float_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr wzr, [<<temp>>]

  public volatile float volatile_instance_float_field;

  public void store_zero_to_volatile_instance_float_field() {
    volatile_instance_float_field = 0.0f;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_volatile_instance_double_field() disassembly (after)
  /// CHECK:             InstanceFieldSet
  /// CHECK-NEXT:        add <<temp:x[0-9]+>>, x{{[0-9]+}}, #0x{{[0-9a-fA-F]+}}
  /// CHECK-NEXT:        stlr xzr, [<<temp>>]

  public volatile double volatile_instance_double_field;

  public void store_zero_to_volatile_instance_double_field() {
    volatile_instance_double_field = 0.0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_byte() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  byte array_byte[];

  public void store_zero_to_array_byte() {
    array_byte[0] = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_char() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  char array_char[];

  public void store_zero_to_array_char() {
    array_char[0] = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_short() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  short array_short[];

  public void store_zero_to_array_short() {
    array_short[0] = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_int() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  int array_int[];

  public void store_zero_to_array_int() {
    array_int[0] = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_long() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  long array_long[];

  public void store_zero_to_array_long() {
    array_long[0] = 0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_float() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  float array_float[];

  public void store_zero_to_array_float() {
    array_float[0] = 0.0f;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_array_double() disassembly (after)
  /// CHECK:             ArraySet
  /// CHECK-NEXT:        str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  double array_double[];

  public void store_zero_to_array_double() {
    array_double[0] = 0.0;
  }

  public static void main(String args[]) {
    Main obj = new Main();
    obj.array_byte = new byte[1];
    obj.array_char = new char[1];
    obj.array_short = new short[1];
    obj.array_int = new int[1];
    obj.array_long = new long[1];
    obj.array_float = new float[1];
    obj.array_double = new double[1];

    obj.bar();
    obj.store_zero_to_static_byte_field();
    obj.store_zero_to_static_char_field();
    obj.store_zero_to_static_short_field();
    obj.store_zero_to_static_int_field();
    obj.store_zero_to_static_long_field();
    obj.store_zero_to_static_float_field();
    obj.store_zero_to_static_double_field();
    obj.store_zero_to_volatile_static_byte_field();
    obj.store_zero_to_volatile_static_char_field();
    obj.store_zero_to_volatile_static_short_field();
    obj.store_zero_to_volatile_static_int_field();
    obj.store_zero_to_volatile_static_long_field();
    obj.store_zero_to_volatile_static_float_field();
    obj.store_zero_to_volatile_static_double_field();
    obj.store_zero_to_instance_byte_field();
    obj.store_zero_to_instance_char_field();
    obj.store_zero_to_instance_short_field();
    obj.store_zero_to_instance_int_field();
    obj.store_zero_to_instance_long_field();
    obj.store_zero_to_instance_float_field();
    obj.store_zero_to_instance_double_field();
    obj.store_zero_to_volatile_instance_byte_field();
    obj.store_zero_to_volatile_instance_char_field();
    obj.store_zero_to_volatile_instance_short_field();
    obj.store_zero_to_volatile_instance_int_field();
    obj.store_zero_to_volatile_instance_long_field();
    obj.store_zero_to_volatile_instance_float_field();
    obj.store_zero_to_volatile_instance_double_field();
    obj.store_zero_to_array_byte();
    obj.store_zero_to_array_char();
    obj.store_zero_to_array_short();
    obj.store_zero_to_array_int();
    obj.store_zero_to_array_long();
    obj.store_zero_to_array_float();
    obj.store_zero_to_array_double();
  }
}
