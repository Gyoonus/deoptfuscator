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

import static art.Redefinition.doCommonClassRedefinition;
import java.util.Base64;
import java.util.Random;
import java.util.function.*;
import java.util.stream.*;

public class Main {

  // The bytes below define the following java program.
  // package java.lang;
  // import java.math.*;
  // public final class Long extends Number implements Comparable<Long> {
  //     public static final long MIN_VALUE = 0;
  //     public static final long MAX_VALUE = 0;
  //     public static final Class<Long> TYPE = null;
  //     static { }
  //     // Used for Stream.count for some reason.
  //     public static long sum(long a, long b) {
  //       return a + b;
  //     }
  //     // Used in stream/lambda functions.
  //     public Long(long value) {
  //       this.value = value;
  //     }
  //     // Used in stream/lambda functions.
  //     public static Long valueOf(long l) {
  //       return new Long(l);
  //     }
  //     // Intrinsic! Do something cool. Return i + 1
  //     public static long highestOneBit(long i) {
  //       return i + 1;
  //     }
  //     // Intrinsic! Do something cool. Return i - 1
  //     public static long lowestOneBit(long i) {
  //       return i - 1;
  //     }
  //     // Intrinsic! Do something cool. Return i + i
  //     public static int numberOfLeadingZeros(long i) {
  //       return (int)(i + i);
  //     }
  //     // Intrinsic! Do something cool. Return i & (i >>> 1);
  //     public static int numberOfTrailingZeros(long i) {
  //       return (int)(i & (i >>> 1));
  //     }
  //     // Intrinsic! Do something cool. Return 5
  //      public static int bitCount(long i) {
  //        return 5;
  //      }
  //     // Intrinsic! Do something cool. Return i
  //     public static long rotateLeft(long i, int distance) {
  //       return i;
  //     }
  //     // Intrinsic! Do something cool. Return 10 * i
  //     public static long rotateRight(long i, int distance) {
  //       return 10 * i;
  //     }
  //     // Intrinsic! Do something cool. Return -i
  //     public static long reverse(long i) {
  //       return -i;
  //     }
  //     // Intrinsic! Do something cool. Return 0
  //     public static int signum(long i) {
  //       return 0;
  //     }
  //     // Intrinsic! Do something cool. Return 0
  //     public static long reverseBytes(long i) {
  //       return 0;
  //     }
  //     public String toString() {
  //       return "Redefined Long! value (as double)=" + ((double)value);
  //     }
  //     public static String toString(long i, int radix) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toUnsignedString(long i, int radix) {
  //       throw new Error("Method redefined away!");
  //     }
  //     private static BigInteger toUnsignedBigInteger(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toHexString(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toOctalString(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toBinaryString(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     static String toUnsignedString0(long val, int shift) {
  //       throw new Error("Method redefined away!");
  //     }
  //     static int formatUnsignedLong(long val, int shift, char[] buf, int offset, int len) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toString(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static String toUnsignedString(long i) {
  //       throw new Error("Method redefined away!");
  //     }
  //     static void getChars(long i, int index, char[] buf) {
  //       throw new Error("Method redefined away!");
  //     }
  //     static int stringSize(long x) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long parseLong(String s, int radix) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long parseLong(String s) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long parseUnsignedLong(String s, int radix) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long parseUnsignedLong(String s) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long valueOf(String s, int radix) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long valueOf(String s) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long decode(String nm) throws NumberFormatException {
  //       throw new Error("Method redefined away!");
  //     }
  //     private final long value;
  //     public Long(String s) throws NumberFormatException {
  //       this(0);
  //       throw new Error("Method redefined away!");
  //     }
  //     public byte byteValue() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public short shortValue() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public int intValue() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public long longValue() {
  //       return value;
  //     }
  //     public float floatValue() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public double doubleValue() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public int hashCode() {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static int hashCode(long value) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public boolean equals(Object obj) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long getLong(String nm) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long getLong(String nm, long val) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static Long getLong(String nm, Long val) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public int compareTo(Long anotherLong) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static int compare(long x, long y) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static int compareUnsigned(long x, long y) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long divideUnsigned(long dividend, long divisor) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long remainderUnsigned(long dividend, long divisor) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static final int SIZE = 64;
  //     public static final int BYTES = SIZE / Byte.SIZE;
  //     public static long max(long a, long b) {
  //       throw new Error("Method redefined away!");
  //     }
  //     public static long min(long a, long b) {
  //       throw new Error("Method redefined away!");
  //     }
  //     private static final long serialVersionUID = 0;
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAiQUAAAAAAAAACgcAdQoAAwB2CAB3CgADAHgJAA0AeQoAAwB6CgADAHsHAHwIAH0K" +
    "AAoAfgcAfwoADQCACgASAHYKAA0AgQkADQCCBwCDBwCEAQAJTUlOX1ZBTFVFAQABSgEADUNvbnN0" +
    "YW50VmFsdWUFAAAAAAAAAAABAAlNQVhfVkFMVUUBAARUWVBFAQARTGphdmEvbGFuZy9DbGFzczsB" +
    "AAlTaWduYXR1cmUBACNMamF2YS9sYW5nL0NsYXNzPExqYXZhL2xhbmcvTG9uZzs+OwEABXZhbHVl" +
    "AQAEU0laRQEAAUkDAAAAQAEABUJZVEVTAwAAAAgBABBzZXJpYWxWZXJzaW9uVUlEAQANaGlnaGVz" +
    "dE9uZUJpdAEABChKKUoBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAMbG93ZXN0T25lQml0AQAU" +
    "bnVtYmVyT2ZMZWFkaW5nWmVyb3MBAAQoSilJAQAVbnVtYmVyT2ZUcmFpbGluZ1plcm9zAQAIYml0" +
    "Q291bnQBAApyb3RhdGVMZWZ0AQAFKEpJKUoBAAtyb3RhdGVSaWdodAEAB3JldmVyc2UBAAZzaWdu" +
    "dW0BAAxyZXZlcnNlQnl0ZXMBAAh0b1N0cmluZwEAFCgpTGphdmEvbGFuZy9TdHJpbmc7AQAWKEpJ" +
    "KUxqYXZhL2xhbmcvU3RyaW5nOwEAEHRvVW5zaWduZWRTdHJpbmcBABR0b1Vuc2lnbmVkQmlnSW50" +
    "ZWdlcgEAGShKKUxqYXZhL21hdGgvQmlnSW50ZWdlcjsBAAt0b0hleFN0cmluZwEAFShKKUxqYXZh" +
    "L2xhbmcvU3RyaW5nOwEADXRvT2N0YWxTdHJpbmcBAA50b0JpbmFyeVN0cmluZwEAEXRvVW5zaWdu" +
    "ZWRTdHJpbmcwAQASZm9ybWF0VW5zaWduZWRMb25nAQAJKEpJW0NJSSlJAQAIZ2V0Q2hhcnMBAAco" +
    "SklbQylWAQAKc3RyaW5nU2l6ZQEACXBhcnNlTG9uZwEAFihMamF2YS9sYW5nL1N0cmluZztJKUoB" +
    "AApFeGNlcHRpb25zBwCFAQAVKExqYXZhL2xhbmcvU3RyaW5nOylKAQARcGFyc2VVbnNpZ25lZExv" +
    "bmcBAAd2YWx1ZU9mAQAlKExqYXZhL2xhbmcvU3RyaW5nO0kpTGphdmEvbGFuZy9Mb25nOwEAJChM" +
    "amF2YS9sYW5nL1N0cmluZzspTGphdmEvbGFuZy9Mb25nOwEAEyhKKUxqYXZhL2xhbmcvTG9uZzsB" +
    "AAZkZWNvZGUBAAY8aW5pdD4BAAQoSilWAQAVKExqYXZhL2xhbmcvU3RyaW5nOylWAQAJYnl0ZVZh" +
    "bHVlAQADKClCAQAKc2hvcnRWYWx1ZQEAAygpUwEACGludFZhbHVlAQADKClJAQAJbG9uZ1ZhbHVl" +
    "AQADKClKAQAKZmxvYXRWYWx1ZQEAAygpRgEAC2RvdWJsZVZhbHVlAQADKClEAQAIaGFzaENvZGUB" +
    "AAZlcXVhbHMBABUoTGphdmEvbGFuZy9PYmplY3Q7KVoBAAdnZXRMb25nAQAlKExqYXZhL2xhbmcv" +
    "U3RyaW5nO0opTGphdmEvbGFuZy9Mb25nOwEANChMamF2YS9sYW5nL1N0cmluZztMamF2YS9sYW5n" +
    "L0xvbmc7KUxqYXZhL2xhbmcvTG9uZzsBAAljb21wYXJlVG8BABMoTGphdmEvbGFuZy9Mb25nOylJ" +
    "AQAHY29tcGFyZQEABShKSilJAQAPY29tcGFyZVVuc2lnbmVkAQAOZGl2aWRlVW5zaWduZWQBAAUo" +
    "SkopSgEAEXJlbWFpbmRlclVuc2lnbmVkAQADc3VtAQADbWF4AQADbWluAQAVKExqYXZhL2xhbmcv" +
    "T2JqZWN0OylJAQAIPGNsaW5pdD4BAAMoKVYBADpMamF2YS9sYW5nL051bWJlcjtMamF2YS9sYW5n" +
    "L0NvbXBhcmFibGU8TGphdmEvbGFuZy9Mb25nOz47AQAKU291cmNlRmlsZQEACUxvbmcuamF2YQEA" +
    "F2phdmEvbGFuZy9TdHJpbmdCdWlsZGVyDABPAHEBACJSZWRlZmluZWQgTG9uZyEgdmFsdWUgKGFz" +
    "IGRvdWJsZSk9DACGAIcMAB4AFQwAhgCIDAA0ADUBAA9qYXZhL2xhbmcvRXJyb3IBABZNZXRob2Qg" +
    "cmVkZWZpbmVkIGF3YXkhDABPAFEBAA5qYXZhL2xhbmcvTG9uZwwATwBQDABkAGUMABoAGwEAEGph" +
    "dmEvbGFuZy9OdW1iZXIBABRqYXZhL2xhbmcvQ29tcGFyYWJsZQEAH2phdmEvbGFuZy9OdW1iZXJG" +
    "b3JtYXRFeGNlcHRpb24BAAZhcHBlbmQBAC0oTGphdmEvbGFuZy9TdHJpbmc7KUxqYXZhL2xhbmcv" +
    "U3RyaW5nQnVpbGRlcjsBABwoRClMamF2YS9sYW5nL1N0cmluZ0J1aWxkZXI7ADEADQASAAEAEwAH" +
    "ABkAFAAVAAEAFgAAAAIAFwAZABkAFQABABYAAAACABcAGQAaABsAAQAcAAAAAgAdABIAHgAVAAAA" +
    "GQAfACAAAQAWAAAAAgAhABkAIgAgAAEAFgAAAAIAIwAaACQAFQABABYAAAACABcANwAJACUAJgAB" +
    "ACcAAAAcAAQAAgAAAAQeCmGtAAAAAQAoAAAABgABAAAADgAJACkAJgABACcAAAAcAAQAAgAAAAQe" +
    "CmWtAAAAAQAoAAAABgABAAAAEwAJACoAKwABACcAAAAdAAQAAgAAAAUeHmGIrAAAAAEAKAAAAAYA" +
    "AQAAABgACQAsACsAAQAnAAAAHwAFAAIAAAAHHh4EfX+IrAAAAAEAKAAAAAYAAQAAAB0ACQAtACsA" +
    "AQAnAAAAGgABAAIAAAACCKwAAAABACgAAAAGAAEAAAAiAAkALgAvAAEAJwAAABoAAgADAAAAAh6t" +
    "AAAAAQAoAAAABgABAAAAJwAJADAALwABACcAAAAeAAQAAwAAAAYUAAEeaa0AAAABACgAAAAGAAEA" +
    "AAAsAAkAMQAmAAEAJwAAABsAAgACAAAAAx51rQAAAAEAKAAAAAYAAQAAADEACQAyACsAAQAnAAAA" +
    "GgABAAIAAAACA6wAAAABACgAAAAGAAEAAAA2AAkAMwAmAAEAJwAAABoAAgACAAAAAgmtAAAAAQAo" +
    "AAAABgABAAAAOwABADQANQABACcAAAAwAAMAAQAAABi7AANZtwAEEgW2AAYqtAAHirYACLYACbAA" +
    "AAABACgAAAAGAAEAAAA/AAkANAA2AAEAJwAAACIAAwADAAAACrsAClkSC7cADL8AAAABACgAAAAG" +
    "AAEAAABDAAkANwA2AAEAJwAAACIAAwADAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABGAAoA" +
    "OAA5AAEAJwAAACIAAwACAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABKAAkAOgA7AAEAJwAA" +
    "ACIAAwACAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABNAAkAPAA7AAEAJwAAACIAAwACAAAA" +
    "CrsAClkSC7cADL8AAAABACgAAAAGAAEAAABRAAkAPQA7AAEAJwAAACIAAwACAAAACrsAClkSC7cA" +
    "DL8AAAABACgAAAAGAAEAAABVAAgAPgA2AAEAJwAAACIAAwADAAAACrsAClkSC7cADL8AAAABACgA" +
    "AAAGAAEAAABZAAgAPwBAAAEAJwAAACIAAwAGAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABd" +
    "AAkANAA7AAEAJwAAACIAAwACAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABhAAkANwA7AAEA" +
    "JwAAACIAAwACAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABlAAgAQQBCAAEAJwAAACIAAwAE" +
    "AAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAABpAAgAQwArAAEAJwAAACIAAwACAAAACrsAClkS" +
    "C7cADL8AAAABACgAAAAGAAEAAABtAAkARABFAAIAJwAAACIAAwACAAAACrsAClkSC7cADL8AAAAB" +
    "ACgAAAAGAAEAAABxAEYAAAAEAAEARwAJAEQASAACACcAAAAiAAMAAQAAAAq7AApZEgu3AAy/AAAA" +
    "AQAoAAAABgABAAAAdQBGAAAABAABAEcACQBJAEUAAgAnAAAAIgADAAIAAAAKuwAKWRILtwAMvwAA" +
    "AAEAKAAAAAYAAQAAAHkARgAAAAQAAQBHAAkASQBIAAIAJwAAACIAAwABAAAACrsAClkSC7cADL8A" +
    "AAABACgAAAAGAAEAAAB9AEYAAAAEAAEARwAJAEoASwACACcAAAAiAAMAAgAAAAq7AApZEgu3AAy/" +
    "AAAAAQAoAAAABgABAAAAgQBGAAAABAABAEcACQBKAEwAAgAnAAAAIgADAAEAAAAKuwAKWRILtwAM" +
    "vwAAAAEAKAAAAAYAAQAAAIQARgAAAAQAAQBHAAkASgBNAAEAJwAAACEABAACAAAACbsADVketwAO" +
    "sAAAAAEAKAAAAAYAAQAAAIcACQBOAEwAAgAnAAAAIgADAAEAAAAKuwAKWRILtwAMvwAAAAEAKAAA" +
    "AAYAAQAAAIsARgAAAAQAAQBHAAEATwBQAAEAJwAAACoAAwADAAAACiq3AA8qH7UAB7EAAAABACgA" +
    "AAAOAAMAAACQAAQAkQAJAJIAAQBPAFEAAgAnAAAAKwADAAIAAAAPKgm3AA67AApZEgu3AAy/AAAA" +
    "AQAoAAAACgACAAAAlQAFAJYARgAAAAQAAQBHAAEAUgBTAAEAJwAAACIAAwABAAAACrsAClkSC7cA" +
    "DL8AAAABACgAAAAGAAEAAACaAAEAVABVAAEAJwAAACIAAwABAAAACrsAClkSC7cADL8AAAABACgA" +
    "AAAGAAEAAACeAAEAVgBXAAEAJwAAACIAAwABAAAACrsAClkSC7cADL8AAAABACgAAAAGAAEAAACi" +
    "AAEAWABZAAEAJwAAAB0AAgABAAAABSq0AAetAAAAAQAoAAAABgABAAAApgABAFoAWwABACcAAAAi" +
    "AAMAAQAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAAqgABAFwAXQABACcAAAAiAAMAAQAAAAq7" +
    "AApZEgu3AAy/AAAAAQAoAAAABgABAAAArgABAF4AVwABACcAAAAiAAMAAQAAAAq7AApZEgu3AAy/" +
    "AAAAAQAoAAAABgABAAAAsgAJAF4AKwABACcAAAAiAAMAAgAAAAq7AApZEgu3AAy/AAAAAQAoAAAA" +
    "BgABAAAAtgABAF8AYAABACcAAAAiAAMAAgAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAAugAJ" +
    "AGEATAABACcAAAAiAAMAAQAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAAvgAJAGEAYgABACcA" +
    "AAAiAAMAAwAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAAwgAJAGEAYwABACcAAAAiAAMAAgAA" +
    "AAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAAxgABAGQAZQABACcAAAAiAAMAAgAAAAq7AApZEgu3" +
    "AAy/AAAAAQAoAAAABgABAAAAyQAJAGYAZwABACcAAAAiAAMABAAAAAq7AApZEgu3AAy/AAAAAQAo" +
    "AAAABgABAAAAzQAJAGgAZwABACcAAAAiAAMABAAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAA" +
    "0QAJAGkAagABACcAAAAiAAMABAAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAA1QAJAGsAagAB" +
    "ACcAAAAiAAMABAAAAAq7AApZEgu3AAy/AAAAAQAoAAAABgABAAAA2QAJAGwAagABACcAAAAcAAQA" +
    "BAAAAAQeIGGtAAAAAQAoAAAABgABAAAA4AAJAG0AagABACcAAAAiAAMABAAAAAq7AApZEgu3AAy/" +
    "AAAAAQAoAAAABgABAAAA5AAJAG4AagABACcAAAAiAAMABAAAAAq7AApZEgu3AAy/AAAAAQAoAAAA" +
    "BgABAAAA5xBBAGQAbwABACcAAAAhAAIAAgAAAAkqK8AADbYAEKwAAAABACgAAAAGAAEAAAAFAAgA" +
    "cABxAAEAJwAAACEAAQAAAAAABQGzABGxAAAAAQAoAAAACgACAAAACAAEAAoAAgAcAAAAAgByAHMA" +
    "AAACAHQ=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAFtMupmeDN6Ck5nxdemGsp43KmLNpYLrMYFgAAcAAAAHhWNBIAAAAAAAAAAEgVAABl" +
    "AAAAcAAAABUAAAAEAgAAIAAAAFgCAAAHAAAA2AMAAD0AAAAQBAAAAQAAAPgFAAAAEAAAGAYAAB4O" +
    "AAAhDgAAKw4AADMOAAA3DgAAOg4AAEEOAABEDgAARw4AAEoOAABODgAAVg4AAFsOAABfDgAAYg4A" +
    "AGYOAABrDgAAcA4AAHQOAAB5DgAAfA4AAIAOAACEDgAAiQ4AAI0OAACSDgAAlw4AAJwOAAC7DgAA" +
    "1w4AAOkOAAD8DgAAEw8AACsPAAA+DwAAUA8AAGQPAACHDwAAmw8AAK8PAADKDwAA4g8AAO0PAAD4" +
    "DwAAAxAAABsQAAA/EAAAQhAAAEgQAABOEAAAURAAAFUQAABbEAAAXxAAAGIQAABmEAAAahAAAHIQ" +
    "AAB8EAAAhxAAAJAQAACbEAAArBAAALQQAADEEAAA0RAAAOUQAADtEAAA+RAAAA0RAAAXEQAAIBEA" +
    "ACoRAAA5EQAAQxEAAE4RAABcEQAAYREAAGYRAAB8EQAAkxEAAJ4RAACxEQAAxBEAAM0RAADbEQAA" +
    "5xEAAPQRAAAGEgAAEhIAABoSAAAmEgAAKxIAADsSAABIEgAAVxIAAGESAAB3EgAAiRIAAJwSAACj" +
    "EgAABAAAAAYAAAAHAAAACAAAAA0AAAAbAAAAHAAAAB4AAAAgAAAAIQAAACIAAAAjAAAAJAAAACUA" +
    "AAAmAAAAJwAAACgAAAAuAAAAMQAAADUAAAA3AAAABAAAAAAAAAAAAAAABgAAAAEAAAAAAAAABwAA" +
    "AAIAAAAAAAAACAAAAAMAAAAAAAAACQAAAAMAAAC0DQAACgAAAAMAAAC8DQAACwAAAAMAAADMDQAA" +
    "DAAAAAMAAADUDQAADAAAAAMAAADcDQAADQAAAAQAAAAAAAAADgAAAAQAAAC0DQAADwAAAAQAAADk" +
    "DQAAEAAAAAQAAADMDQAAEQAAAAQAAADsDQAAEgAAAAQAAAD0DQAAFQAAAAoAAAC0DQAAFwAAAAoA" +
    "AADsDQAAGAAAAAoAAAD0DQAAGQAAAAoAAAD8DQAAGgAAAAoAAAAEDgAAEwAAAA4AAAAAAAAAFQAA" +
    "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" +
    "EwAAAwAAAJsAAgQQAAAABAACAAIAAACPEwAACQAAACIACQAbASwAAABwIAAAEAAnAAAABAACAAIA" +
    "AACVEwAACQAAACIACQAbASwAAABwIAAAEAAnAAAABAACAAIAAACbEwAACQAAACIACQAbASwAAABw" +
    "IAAAEAAnAAAABAACAAIAAAChEwAACQAAACIACQAbASwAAABwIAAAEAAnAAAABQADAAIAAACnEwAA" +
    "CQAAACIACQAbASwAAABwIAAAEAAnAAAABAACAAIAAACuEwAACQAAACIACQAbASwAAABwIAAAEAAn" +
    "AAAABAACAAIAAAC0EwAACQAAACIACQAbASwAAABwIAAAEAAnAAAABQADAAIAAAC6EwAACQAAACIA" +
    "CQAbASwAAABwIAAAEAAnAAAABQADAAIAAADBEwAACQAAACIACQAbASwAAABwIAAAEAAnAAAABAAC" +
    "AAMAAADIEwAABgAAACIACgBwMAIAIAMRAAMAAQACAAAAzxMAAAkAAAAiAAkAGwEsAAAAcCAAABAA" +
    "JwAAAAQAAgACAAAA1hMAAAkAAAAiAAkAGwEsAAAAcCAAABAAJwAAAAMAAQACAAAA3hMAAAkAAAAi" +
    "AAkAGwEsAAAAcCAAABAAJwAAAAQAAgACAAAA5BMAAAkAAAAiAAkAGwEsAAAAcCAAABAAJwAAAAMA" +
    "AgACAAAA6xMAAAcAAAAfAgoAbiAHACEACgAPAAAAAwABAAIAAADyEwAACQAAACIACQAbASwAAABw" +
    "IAAAEAAnAAAABAACAAIAAAD4EwAACQAAACIACQAbASwAAABwIAAAEAAnAAAAAwABAAIAAAD/EwAA" +
    "CQAAACIACQAbASwAAABwIAAAEAAnAAAAAwABAAIAAAAFFAAACQAAACIACQAbASwAAABwIAAAEAAn" +
    "AAAAAwABAAIAAAALFAAACQAAACIACQAbASwAAABwIAAAEAAnAAAAAwABAAAAAAARFAAAAwAAAFMg" +
    "BgAQAAAAAwABAAIAAAAXFAAACQAAACIACQAbASwAAABwIAAAEAAnAAAABQABAAMAAAAdFAAAGAAA" +
    "ACIADwBwEDkAAAAbAS0AAABuIDsAEAAMAFNCBgCGIm4wOgAgAwwAbhA8AAAADAARABgGAAABAAAA" +
    "CAAAAAAAAAAEAAAAIAYAAAMAAAAoBgAACgAAACgGAAAeAAAAKAYAAB8AAAAoBgAAIAAAACgGAAAh" +
    "AAAAKAYAADYAAAAoBgAANwAAACgGAAABAAAACAAAAAEAAAAEAAAABQAAAAQAAwAUAAMAAwAAAAIA" +
    "AAAEAAQAAQAAAAoAAAABAAAADQAAAAIAAAAEAAMAAQAAAA4AAAACAAAADgADAAIAAAAOAAQAAgAA" +
    "AA4ACgABAAAAAQAAAAMAAAAEAAMAFAABPAAIPGNsaW5pdD4ABjxpbml0PgACPjsAAUIABUJZVEVT" +
    "AAFEAAFGAAFJAAJJSgAGSUpJTElJAANJSkoAAklMAAFKAAJKSgADSkpJAANKSkoAAkpMAANKTEkA" +
    "AUwAAkxEAAJMSgADTEpJAAJMTAADTExJAANMTEoAA0xMTAAdTGRhbHZpay9hbm5vdGF0aW9uL1Np" +
    "Z25hdHVyZTsAGkxkYWx2aWsvYW5ub3RhdGlvbi9UaHJvd3M7ABBMamF2YS9sYW5nL0NsYXNzABFM" +
    "amF2YS9sYW5nL0NsYXNzOwAVTGphdmEvbGFuZy9Db21wYXJhYmxlABZMamF2YS9sYW5nL0NvbXBh" +
    "cmFibGU7ABFMamF2YS9sYW5nL0Vycm9yOwAQTGphdmEvbGFuZy9Mb25nOwASTGphdmEvbGFuZy9O" +
    "dW1iZXI7ACFMamF2YS9sYW5nL051bWJlckZvcm1hdEV4Y2VwdGlvbjsAEkxqYXZhL2xhbmcvT2Jq" +
    "ZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABlMamF2YS9sYW5nL1N0cmluZ0J1aWxkZXI7ABZMamF2" +
    "YS9tYXRoL0JpZ0ludGVnZXI7AAlMb25nLmphdmEACU1BWF9WQUxVRQAJTUlOX1ZBTFVFABZNZXRo" +
    "b2QgcmVkZWZpbmVkIGF3YXkhACJSZWRlZmluZWQgTG9uZyEgdmFsdWUgKGFzIGRvdWJsZSk9AAFT" +
    "AARTSVpFAARUWVBFAAFWAAJWSgAEVkpJTAACVkwAAVoAAlpMAAJbQwAGYXBwZW5kAAhiaXRDb3Vu" +
    "dAAJYnl0ZVZhbHVlAAdjb21wYXJlAAljb21wYXJlVG8AD2NvbXBhcmVVbnNpZ25lZAAGZGVjb2Rl" +
    "AA5kaXZpZGVVbnNpZ25lZAALZG91YmxlVmFsdWUAEmVtaXR0ZXI6IGphY2stNC4yNQAGZXF1YWxz" +
    "AApmbG9hdFZhbHVlABJmb3JtYXRVbnNpZ25lZExvbmcACGdldENoYXJzAAdnZXRMb25nAAhoYXNo" +
    "Q29kZQANaGlnaGVzdE9uZUJpdAAIaW50VmFsdWUACWxvbmdWYWx1ZQAMbG93ZXN0T25lQml0AANt" +
    "YXgAA21pbgAUbnVtYmVyT2ZMZWFkaW5nWmVyb3MAFW51bWJlck9mVHJhaWxpbmdaZXJvcwAJcGFy" +
    "c2VMb25nABFwYXJzZVVuc2lnbmVkTG9uZwARcmVtYWluZGVyVW5zaWduZWQAB3JldmVyc2UADHJl" +
    "dmVyc2VCeXRlcwAKcm90YXRlTGVmdAALcm90YXRlUmlnaHQAEHNlcmlhbFZlcnNpb25VSUQACnNo" +
    "b3J0VmFsdWUABnNpZ251bQAKc3RyaW5nU2l6ZQADc3VtAA50b0JpbmFyeVN0cmluZwALdG9IZXhT" +
    "dHJpbmcADXRvT2N0YWxTdHJpbmcACHRvU3RyaW5nABR0b1Vuc2lnbmVkQmlnSW50ZWdlcgAQdG9V" +
    "bnNpZ25lZFN0cmluZwARdG9VbnNpZ25lZFN0cmluZzAABXZhbHVlAAd2YWx1ZU9mAAUABw4AkAEB" +
    "AAcOPC0AlQEBAAcOWgAiAQAHDgDNAQIAAAcOANEBAgAABw4AiwEBAAcOANUBAgAABw4AXQUAAAAA" +
    "AAcOAGkDAAAABw4AvgEBAAcOAMIBAgAABw4AxgECAAAHDgC2AQEABw4ADgEABw4AEwEABw4A5AEC" +
    "AAAHDgDnAQIAAAcOABgBAAcOAB0BAAcOAHUBAAcOAHECAAAHDgB9AQAHDgB5AgAABw4A2QECAAAH" +
    "DgAxAQAHDgA7AQAHDgAnAgAABw4ALAIAAAcOADYBAAcOAG0BAAcOAOABAgAABw4AVQEABw4ATQEA" +
    "Bw4AUQEABw4AYQEABw4AQwIAAAcOAEoBAAcOAGUBAAcOAEYCAAAHDgBZAgAABw4AhwEBAAcOAIQB" +
    "AQAHDgCBAQIAAAcOAJoBAAcOAMkBAQAHDgDIAQEABw4ArgEABw4AugEBAAcOAKoBAAcOALIBAAcO" +
    "AKIBAAcOAKYBAAcOAJ4BAAcOAD8ABw4AAgUBYxwFFyMXHxcAFyIXAwIFAWMcBBcdFwAXIhcDAgYB" +
    "YxwBGAwEBAgGAAYABEAGASwLABkBGQEZARkBGQEaBhIBiIAEsAwBgYAExAwBgYAE4AwBCYwNAgmg" +
    "DQMJxA0BCegNAQmMDgQIsA4BCNQOAQn4DgEJnA8BCcAPAgnkDwEJiBADCaAQAQm8EAEJ4BABCYQR" +
    "AQmcEQEJuBEBCdwRAQmAEgEJpBIBCcgSAQnsEgEJgBMBCZgTAQmsEwIJxBMBCNgTAQn8EwEJlBQB" +
    "CbgUAQncFAIJgBUBCaQVAQrIFQEJ7BUBCZAWAQi0FgEJ2BYBCfQWAQmYFwUBvBcCAeAXAcEghBgE" +
    "AaQYAQHIGAEB7BgGAZAZAwG0GQEB2BkPAfAZBwGUGgAAEQAAAAAAAAABAAAAAAAAAAEAAABlAAAA" +
    "cAAAAAIAAAAVAAAABAIAAAMAAAAgAAAAWAIAAAQAAAAHAAAA2AMAAAUAAAA9AAAAEAQAAAYAAAAB" +
    "AAAA+AUAAAMQAAADAAAAGAYAAAEgAAA3AAAAMAYAAAYgAAABAAAAVA0AAAEQAAANAAAArA0AAAIg" +
    "AABlAAAAHg4AAAMgAAA3AAAArBIAAAQgAAADAAAAIhQAAAUgAAABAAAASBQAAAAgAAABAAAAURQA" +
    "AAAQAAABAAAASBUAAA==");

  static class FuncCmp implements LongPredicate {
    final String name;
    final LongPredicate p;
    public FuncCmp(String name, LongPredicate p) {
      this.name = name;
      this.p = p;
    }

    public boolean test(long l) {
      return p.test(l);
    }
  }
  static FuncCmp l2l(String name, final LongUnaryOperator a, final LongUnaryOperator b) {
    return new FuncCmp(name, (v) -> a.applyAsLong(v) == b.applyAsLong(v));
  }
  static FuncCmp l2i(String name, final LongToIntFunction a, final LongToIntFunction b) {
    return new FuncCmp(name, (v) -> a.applyAsInt(v) == b.applyAsInt(v));
  }

  /** Interface for a long, int -> long function. */
  static interface LI2IFunction { public long applyToLongInt(long a, int b); }

  static FuncCmp li2l(String name, final Random r, final LI2IFunction a, final LI2IFunction b) {
    return new FuncCmp(name, new LongPredicate() {
      public boolean test(long v) {
        int i = r.nextInt();
        return a.applyToLongInt(v, i) == b.applyToLongInt(v, i);
      }
    });
  }

  public static void main(String[] args) {
    doTest(10000);
  }

  public static void doTest(int iters) {
    // Just transform immediately.
    doCommonClassRedefinition(Long.class, CLASS_BYTES, DEX_BYTES);
    final Random r = new Random();
    FuncCmp[] comps = new FuncCmp[] {
      l2l("highestOneBit", Long::highestOneBit, RedefinedLongIntrinsics::highestOneBit),
      l2l("lowestOneBit", Long::lowestOneBit, RedefinedLongIntrinsics::lowestOneBit),
      l2i("numberOfLeadingZeros",
          Long::numberOfLeadingZeros,
          RedefinedLongIntrinsics::numberOfLeadingZeros),
      l2i("numberOfTrailingZeros",
          Long::numberOfTrailingZeros,
          RedefinedLongIntrinsics::numberOfTrailingZeros),
      l2i("bitCount", Long::bitCount, RedefinedLongIntrinsics::bitCount),
      li2l("rotateLeft", r, Long::rotateLeft, RedefinedLongIntrinsics::rotateLeft),
      li2l("rotateRight", r, Long::rotateRight, RedefinedLongIntrinsics::rotateRight),
      l2l("reverse", Long::reverse, RedefinedLongIntrinsics::reverse),
      l2i("signum", Long::signum, RedefinedLongIntrinsics::signum),
      l2l("reverseBytes", Long::reverseBytes, RedefinedLongIntrinsics::reverseBytes),
    };
    for (FuncCmp f : comps) {
      // Just actually use ints so we can cast them back int the tests to print them (since we
      // deleted a bunch of the Long methods needed for printing longs)!
      int failures = (int)r.ints(iters)
                           .mapToLong((v) -> (long)v)
                           .filter(f.negate()) // Get all the test cases that failed.
                           .count();
      if (failures != 0) {
        double percent = 100.0d*((double)failures/(double)iters);
        System.out.println("for intrinsic " + f.name + " " + failures + "/" + iters
            + " (" + Double.toString(percent) + "%) tests failed!");
      }
    }
    System.out.println("Finished!");
  }
}
