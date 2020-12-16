/*
 * Copyright (C) 2017 The Android Open Source Project
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

// AUTO-GENENERATED by gen_srcs.py: DO NOT EDIT HERE DIRECTLY.
//
// $> python3.4 gen_srcs.py > src/art/Test988Intrinsics.java
//
// RUN ABOVE COMMAND TO REGENERATE THIS FILE.

package art;

class Test988Intrinsics {
  // Pre-initialize *all* instance variables used so that their constructors are not in the trace.
  static java.lang.String instance_java_lang_String = "some large string";
  static java.lang.StringBuffer instance_java_lang_StringBuffer = new java.lang.StringBuffer("some large string buffer");
  static java.lang.StringBuilder instance_java_lang_StringBuilder = new java.lang.StringBuilder("some large string builder");

  static void initialize() {
    // Ensure all static variables are initialized.
    // In addition, pre-load classes here so that we don't see diverging class loading traces.
    java.lang.Double.class.toString();
    java.lang.Float.class.toString();
    java.lang.Integer.class.toString();
    java.lang.Long.class.toString();
    java.lang.Short.class.toString();
    java.lang.Math.class.toString();
    java.lang.Thread.class.toString();
    java.lang.String.class.toString();
    java.lang.StringBuffer.class.toString();
    java.lang.StringBuilder.class.toString();
  }

  static void test() {
    // Call each intrinsic from art/runtime/intrinsics_list.h to make sure they are traced.
    java.lang.Double.doubleToRawLongBits(0.0);
    java.lang.Double.doubleToLongBits(0.0);
    java.lang.Double.isInfinite(0.0);
    java.lang.Double.isNaN(0.0);
    java.lang.Double.longBitsToDouble(0L);
    java.lang.Float.floatToRawIntBits(0.0f);
    java.lang.Float.floatToIntBits(0.0f);
    java.lang.Float.isInfinite(0.0f);
    java.lang.Float.isNaN(0.0f);
    java.lang.Float.intBitsToFloat(0);
    java.lang.Integer.reverse(0);
    java.lang.Integer.reverseBytes(0);
    java.lang.Integer.bitCount(0);
    java.lang.Integer.compare(0, 0);
    java.lang.Integer.highestOneBit(0);
    java.lang.Integer.lowestOneBit(0);
    java.lang.Integer.numberOfLeadingZeros(0);
    java.lang.Integer.numberOfTrailingZeros(0);
    java.lang.Integer.rotateRight(0, 0);
    java.lang.Integer.rotateLeft(0, 0);
    java.lang.Integer.signum(0);
    java.lang.Long.reverse(0L);
    java.lang.Long.reverseBytes(0L);
    java.lang.Long.bitCount(0L);
    java.lang.Long.compare(0L, 0L);
    java.lang.Long.highestOneBit(0L);
    java.lang.Long.lowestOneBit(0L);
    java.lang.Long.numberOfLeadingZeros(0L);
    java.lang.Long.numberOfTrailingZeros(0L);
    java.lang.Long.rotateRight(0L, 0);
    java.lang.Long.rotateLeft(0L, 0);
    java.lang.Long.signum(0L);
    java.lang.Short.reverseBytes((short)0);
    java.lang.Math.abs(0.0);
    java.lang.Math.abs(0.0f);
    java.lang.Math.abs(0L);
    java.lang.Math.abs(0);
    java.lang.Math.min(0.0, 0.0);
    java.lang.Math.min(0.0f, 0.0f);
    java.lang.Math.min(0L, 0L);
    java.lang.Math.min(0, 0);
    java.lang.Math.max(0.0, 0.0);
    java.lang.Math.max(0.0f, 0.0f);
    java.lang.Math.max(0L, 0L);
    java.lang.Math.max(0, 0);
    java.lang.Math.cos(0.0);
    java.lang.Math.sin(0.0);
    java.lang.Math.acos(0.0);
    java.lang.Math.asin(0.0);
    java.lang.Math.atan(0.0);
    java.lang.Math.atan2(0.0, 0.0);
    java.lang.Math.cbrt(0.0);
    java.lang.Math.cosh(0.0);
    java.lang.Math.exp(0.0);
    java.lang.Math.expm1(0.0);
    java.lang.Math.hypot(0.0, 0.0);
    java.lang.Math.log(0.0);
    java.lang.Math.log10(0.0);
    java.lang.Math.nextAfter(0.0, 0.0);
    java.lang.Math.sinh(0.0);
    java.lang.Math.tan(0.0);
    java.lang.Math.tanh(0.0);
    java.lang.Math.sqrt(0.0);
    java.lang.Math.ceil(0.0);
    java.lang.Math.floor(0.0);
    java.lang.Math.rint(0.0);
    java.lang.Math.round(0.0);
    java.lang.Math.round(0.0f);
    java.lang.Thread.currentThread();
    instance_java_lang_String.charAt(0);
    instance_java_lang_String.compareTo("hello");
    instance_java_lang_String.equals((java.lang.Object)null);
    instance_java_lang_String.indexOf(0);
    instance_java_lang_String.indexOf(0, 0);
    instance_java_lang_String.indexOf("hello");
    instance_java_lang_String.indexOf("hello", 0);
    instance_java_lang_String.isEmpty();
    instance_java_lang_String.length();
    instance_java_lang_StringBuffer.append("hello");
    instance_java_lang_StringBuffer.length();
    instance_java_lang_StringBuffer.toString();
    instance_java_lang_StringBuilder.append("hello");
    instance_java_lang_StringBuilder.length();
    instance_java_lang_StringBuilder.toString();
    java.lang.Integer.valueOf(0);
    java.lang.Thread.interrupted();
  }
}
