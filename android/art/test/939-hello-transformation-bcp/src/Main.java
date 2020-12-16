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

import static art.Redefinition.doCommonClassRedefinition;
import java.util.Base64;
import java.util.OptionalLong;
public class Main {

  /**
   * This is the base64 encoded class/dex.
   *
   * package java.util;
   * import java.util.function.LongConsumer;
   * import java.util.function.LongSupplier;
   * import java.util.function.Supplier;
   * public final class OptionalLong {
   *   // Make sure we have a <clinit> function since the real implementation of OptionalLong does.
   *   static { EMPTY = null; }
   *   private static final OptionalLong EMPTY;
   *   private final boolean isPresent;
   *   private final long value;
   *   private OptionalLong() { isPresent = false; value = 0; }
   *   private OptionalLong(long l) { this(); }
   *   public static OptionalLong empty() { return null; }
   *   public static OptionalLong of(long value) { return null; }
   *   public long getAsLong() { return 0; }
   *   public boolean isPresent() { return false; }
   *   public void ifPresent(LongConsumer c) { }
   *   public long orElse(long l) { return 0; }
   *   public long orElseGet(LongSupplier s) { return 0; }
   *   public<X extends Throwable> long orElseThrow(Supplier<X> s) throws X { return 0; }
   *   public boolean equals(Object o) { return false; }
   *   public int hashCode() { return 0; }
   *   public String toString() { return "Redefined OptionalLong!"; }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAOAoACAAwCQAHADEJAAcAMgoABwAwCAAzCQAHADQHADUHADYBAAVFTVBUWQEAGExq" +
    "YXZhL3V0aWwvT3B0aW9uYWxMb25nOwEACWlzUHJlc2VudAEAAVoBAAV2YWx1ZQEAAUoBAAY8aW5p" +
    "dD4BAAMoKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAEKEopVgEABWVtcHR5AQAaKClMamF2" +
    "YS91dGlsL09wdGlvbmFsTG9uZzsBAAJvZgEAGyhKKUxqYXZhL3V0aWwvT3B0aW9uYWxMb25nOwEA" +
    "CWdldEFzTG9uZwEAAygpSgEAAygpWgEACWlmUHJlc2VudAEAJChMamF2YS91dGlsL2Z1bmN0aW9u" +
    "L0xvbmdDb25zdW1lcjspVgEABm9yRWxzZQEABChKKUoBAAlvckVsc2VHZXQBACQoTGphdmEvdXRp" +
    "bC9mdW5jdGlvbi9Mb25nU3VwcGxpZXI7KUoBAAtvckVsc2VUaHJvdwEAIChMamF2YS91dGlsL2Z1" +
    "bmN0aW9uL1N1cHBsaWVyOylKAQAKRXhjZXB0aW9ucwcANwEACVNpZ25hdHVyZQEAQjxYOkxqYXZh" +
    "L2xhbmcvVGhyb3dhYmxlOz4oTGphdmEvdXRpbC9mdW5jdGlvbi9TdXBwbGllcjxUWDs+OylKXlRY" +
    "OwEABmVxdWFscwEAFShMamF2YS9sYW5nL09iamVjdDspWgEACGhhc2hDb2RlAQADKClJAQAIdG9T" +
    "dHJpbmcBABQoKUxqYXZhL2xhbmcvU3RyaW5nOwEACDxjbGluaXQ+AQAKU291cmNlRmlsZQEAEU9w" +
    "dGlvbmFsTG9uZy5qYXZhDAAPABAMAAsADAwADQAOAQAXUmVkZWZpbmVkIE9wdGlvbmFsTG9uZyEM" +
    "AAkACgEAFmphdmEvdXRpbC9PcHRpb25hbExvbmcBABBqYXZhL2xhbmcvT2JqZWN0AQATamF2YS9s" +
    "YW5nL1Rocm93YWJsZQAxAAcACAAAAAMAGgAJAAoAAAASAAsADAAAABIADQAOAAAADgACAA8AEAAB" +
    "ABEAAAAnAAMAAQAAAA8qtwABKgO1AAIqCbUAA7EAAAABABIAAAAGAAEAAAALAAIADwATAAEAEQAA" +
    "AB0AAQADAAAABSq3AASxAAAAAQASAAAABgABAAAADAAJABQAFQABABEAAAAaAAEAAAAAAAIBsAAA" +
    "AAEAEgAAAAYAAQAAAA0ACQAWABcAAQARAAAAGgABAAIAAAACAbAAAAABABIAAAAGAAEAAAAOAAEA" +
    "GAAZAAEAEQAAABoAAgABAAAAAgmtAAAAAQASAAAABgABAAAADwABAAsAGgABABEAAAAaAAEAAQAA" +
    "AAIDrAAAAAEAEgAAAAYAAQAAABAAAQAbABwAAQARAAAAGQAAAAIAAAABsQAAAAEAEgAAAAYAAQAA" +
    "ABEAAQAdAB4AAQARAAAAGgACAAMAAAACCa0AAAABABIAAAAGAAEAAAASAAEAHwAgAAEAEQAAABoA" +
    "AgACAAAAAgmtAAAAAQASAAAABgABAAAAEwABACEAIgADABEAAAAaAAIAAgAAAAIJrQAAAAEAEgAA" +
    "AAYAAQAAABQAIwAAAAQAAQAkACUAAAACACYAAQAnACgAAQARAAAAGgABAAIAAAACA6wAAAABABIA" +
    "AAAGAAEAAAAVAAEAKQAqAAEAEQAAABoAAQABAAAAAgOsAAAAAQASAAAABgABAAAAFgABACsALAAB" +
    "ABEAAAAbAAEAAQAAAAMSBbAAAAABABIAAAAGAAEAAAAXAAgALQAQAAEAEQAAAB0AAQAAAAAABQGz" +
    "AAaxAAAAAQASAAAABgABAAAABwABAC4AAAACAC8=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCvAoivSJqk6GdYOgJmvrM/b2/flxhw99q8BwAAcAAAAHhWNBIAAAAAAAAAAPgGAAAq" +
    "AAAAcAAAAA0AAAAYAQAADQAAAEwBAAADAAAA6AEAAA8AAAAAAgAAAQAAAHgCAAAkBQAAmAIAACoE" +
    "AAA4BAAAPQQAAEcEAABPBAAAUwQAAFoEAABdBAAAYAQAAGQEAABoBAAAawQAAG8EAACOBAAAqgQA" +
    "AL4EAADSBAAA6QQAAAMFAAAmBQAASQUAAGcFAACGBQAAmQUAALIFAAC1BQAAuQUAAL0FAADABQAA" +
    "xAUAANgFAADfBQAA5wUAAPIFAAD8BQAABwYAABIGAAAWBgAAHgYAACkGAAA2BgAAQAYAAAYAAAAH" +
    "AAAADAAAAA0AAAAOAAAADwAAABAAAAARAAAAEgAAABMAAAAVAAAAGAAAABsAAAAGAAAAAAAAAAAA" +
    "AAAHAAAAAQAAAAAAAAAIAAAAAQAAAAQEAAAJAAAAAQAAAAwEAAAJAAAAAQAAABQEAAAKAAAABQAA" +
    "AAAAAAAKAAAABwAAAAAAAAALAAAABwAAAAQEAAAYAAAACwAAAAAAAAAZAAAACwAAAAQEAAAaAAAA" +
    "CwAAABwEAAAbAAAADAAAAAAAAAAcAAAADAAAACQEAAAHAAcABQAAAAcADAAjAAAABwABACkAAAAE" +
    "AAgAAwAAAAcACAACAAAABwAIAAMAAAAHAAkAAwAAAAcABgAeAAAABwAMAB8AAAAHAAEAIAAAAAcA" +
    "AAAhAAAABwAKACIAAAAHAAsAIwAAAAcABwAkAAAABwACACUAAAAHAAMAJgAAAAcABAAnAAAABwAF" +
    "ACgAAAAHAAAAEQAAAAQAAAAAAAAAFgAAAOwDAACtBgAAAAAAAAIAAACVBgAApQYAAAEAAAAAAAAA" +
    "RwYAAAQAAAASAGkAAAAOAAMAAQABAAAATQYAAAsAAABwEAAAAgASAFwgAQAWAAAAWiACAA4AAAAD" +
    "AAMAAQAAAFIGAAAEAAAAcBACAAAADgABAAAAAAAAAFgGAAACAAAAEgARAAMAAgAAAAAAXQYAAAIA" +
    "AAASABEAAwACAAAAAABjBgAAAgAAABIADwADAAEAAAAAAGkGAAADAAAAFgAAABAAAAACAAEAAAAA" +
    "AG4GAAACAAAAEgAPAAIAAgAAAAAAcwYAAAEAAAAOAAAAAgABAAAAAAB5BgAAAgAAABIADwAFAAMA" +
    "AAAAAH4GAAADAAAAFgAAABAAAAAEAAIAAAAAAIQGAAADAAAAFgAAABAAAAAEAAIAAAAAAIoGAAAD" +
    "AAAAFgAAABAAAAACAAEAAAAAAJAGAAAEAAAAGwAXAAAAEQAAAAAAAAAAAAEAAAAAAAAADQAAAJgC" +
    "AAABAAAAAQAAAAEAAAAJAAAAAQAAAAoAAAABAAAACAAAAAEAAAAEAAw8VFg7PjspSl5UWDsAAzxY" +
    "OgAIPGNsaW5pdD4ABjxpbml0PgACPigABUVNUFRZAAFJAAFKAAJKSgACSkwAAUwAAkxKAB1MZGFs" +
    "dmlrL2Fubm90YXRpb24vU2lnbmF0dXJlOwAaTGRhbHZpay9hbm5vdGF0aW9uL1Rocm93czsAEkxq" +
    "YXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABVMamF2YS9sYW5nL1Rocm93YWJs" +
    "ZTsAGExqYXZhL3V0aWwvT3B0aW9uYWxMb25nOwAhTGphdmEvdXRpbC9mdW5jdGlvbi9Mb25nQ29u" +
    "c3VtZXI7ACFMamF2YS91dGlsL2Z1bmN0aW9uL0xvbmdTdXBwbGllcjsAHExqYXZhL3V0aWwvZnVu" +
    "Y3Rpb24vU3VwcGxpZXIAHUxqYXZhL3V0aWwvZnVuY3Rpb24vU3VwcGxpZXI7ABFPcHRpb25hbExv" +
    "bmcuamF2YQAXUmVkZWZpbmVkIE9wdGlvbmFsTG9uZyEAAVYAAlZKAAJWTAABWgACWkwAEmVtaXR0" +
    "ZXI6IGphY2stNC4yMgAFZW1wdHkABmVxdWFscwAJZ2V0QXNMb25nAAhoYXNoQ29kZQAJaWZQcmVz" +
    "ZW50AAlpc1ByZXNlbnQAAm9mAAZvckVsc2UACW9yRWxzZUdldAALb3JFbHNlVGhyb3cACHRvU3Ry" +
    "aW5nAAV2YWx1ZQAHAAcOOQALAAcOAAwBAAcOAA0ABw4ADgEABw4AFQEABw4ADwAHDgAWAAcOABEB" +
    "AAcOABAABw4AEgEABw4AEwEABw4AFAEABw4AFwAHDgACAgEpHAUXARcQFwQXFBcAAgMBKRwBGAYB" +
    "AgUJABoBEgESAYiABKQFAYKABLwFAYKABOQFAQn8BQYJkAYFAaQGAQG4BgEB0AYBAeQGAQH4BgIB" +
    "jAcBAaQHAQG8BwEB1AcAAAAQAAAAAAAAAAEAAAAAAAAAAQAAACoAAABwAAAAAgAAAA0AAAAYAQAA" +
    "AwAAAA0AAABMAQAABAAAAAMAAADoAQAABQAAAA8AAAAAAgAABgAAAAEAAAB4AgAAAxAAAAEAAACY" +
    "AgAAASAAAA4AAACkAgAABiAAAAEAAADsAwAAARAAAAUAAAAEBAAAAiAAACoAAAAqBAAAAyAAAA4A" +
    "AABHBgAABCAAAAIAAACVBgAAACAAAAEAAACtBgAAABAAAAEAAAD4BgAA");

  public static void main(String[] args) {
    // OptionalLong is a class that is unlikely to be used by the time this test starts and is not
    // likely to be changed in any meaningful way in the future.
    OptionalLong ol = OptionalLong.of(0xDEADBEEF);
    System.out.println("ol.toString() -> '" + ol.toString() + "'");
    System.out.println("Redefining OptionalLong!");
    doCommonClassRedefinition(OptionalLong.class, CLASS_BYTES, DEX_BYTES);
    System.out.println("ol.toString() -> '" + ol.toString() + "'");
  }
}
