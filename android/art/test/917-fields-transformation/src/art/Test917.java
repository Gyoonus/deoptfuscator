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

package art;

import java.util.Base64;
public class Test917 {

  static class Transform {
    public String take1;
    public String take2;

    public Transform(String take1, String take2) {
      this.take1 = take1;
      this.take2 = take2;
    }

    public String getResult() {
      return take1;
    }
  }


  // base64 encoded class/dex file for
  // static class Transform {
  //   public String take1;
  //   public String take2;
  //
  //   public Transform(String a, String b) {
  //     take1 = a;
  //     take2 = b;
  //   }
  //
  //   public String getResult() {
  //     return take2;
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAGwoABQARCQAEABIJAAQAEwcAFQcAGAEABXRha2UxAQASTGphdmEvbGFuZy9TdHJp" +
    "bmc7AQAFdGFrZTIBAAY8aW5pdD4BACcoTGphdmEvbGFuZy9TdHJpbmc7TGphdmEvbGFuZy9TdHJp" +
    "bmc7KVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAJZ2V0UmVzdWx0AQAUKClMamF2YS9sYW5n" +
    "L1N0cmluZzsBAApTb3VyY2VGaWxlAQAMVGVzdDkxNy5qYXZhDAAJABkMAAYABwwACAAHBwAaAQAV" +
    "YXJ0L1Rlc3Q5MTckVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9s" +
    "YW5nL09iamVjdAEAAygpVgEAC2FydC9UZXN0OTE3ACAABAAFAAAAAgABAAYABwAAAAEACAAHAAAA" +
    "AgABAAkACgABAAsAAAAzAAIAAwAAAA8qtwABKiu1AAIqLLUAA7EAAAABAAwAAAASAAQAAAAJAAQA" +
    "CgAJAAsADgAMAAEADQAOAAEACwAAAB0AAQABAAAABSq0AAOwAAAAAQAMAAAABgABAAAADwACAA8A" +
    "AAACABAAFwAAAAoAAQAEABQAFgAI");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBdcPySAAAAAAAAAAAAAAAAAAAAAAAAAACQAwAAcAAAAHhWNBIAAAAAAAAAAMwCAAAS" +
    "AAAAcAAAAAcAAAC4AAAAAwAAANQAAAACAAAA+AAAAAMAAAAIAQAAAQAAACABAABQAgAAQAEAAEAB" +
    "AABIAQAASwEAAGQBAABzAQAAlwEAALcBAADLAQAA3wEAAO0BAAD4AQAA+wEAAAACAAANAgAAGAIA" +
    "AB4CAAAlAgAALAIAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAoAAAABAAAABQAAAAAAAAAKAAAA" +
    "BgAAAAAAAAALAAAABgAAADQCAAAAAAUADwAAAAAABQAQAAAAAAACAAAAAAAAAAAADQAAAAQAAQAA" +
    "AAAAAAAAAAAAAAAEAAAAAAAAAAgAAAC8AgAAjAIAAAAAAAAGPGluaXQ+AAFMABdMYXJ0L1Rlc3Q5" +
    "MTckVHJhbnNmb3JtOwANTGFydC9UZXN0OTE3OwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2lu" +
    "Z0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABJMamF2YS9sYW5nL09iamVj" +
    "dDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAMVGVzdDkxNy5qYXZhAAlUcmFuc2Zvcm0AAVYAA1ZMTAAL" +
    "YWNjZXNzRmxhZ3MACWdldFJlc3VsdAAEbmFtZQAFdGFrZTEABXRha2UyAAV2YWx1ZQAAAgAAAAUA" +
    "BQAJAgAABw4BAw8BAg8BAg8ADwAHDgAAAAADAAMAAQAAADwCAAAIAAAAcBACAAAAWwEAAFsCAQAO" +
    "AAIAAQAAAAAATAIAAAMAAABUEAEAEQAAAAACAQEAAQEBAIGABNQEAQH0BAAAAgIBERgBAgMCDAQI" +
    "DhcJAAIAAACgAgAApgIAALACAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAEAAAAAAAAAAQAAABIAAABw" +
    "AAAAAgAAAAcAAAC4AAAAAwAAAAMAAADUAAAABAAAAAIAAAD4AAAABQAAAAMAAAAIAQAABgAAAAEA" +
    "AAAgAQAAAiAAABIAAABAAQAAARAAAAEAAAA0AgAAAyAAAAIAAAA8AgAAASAAAAIAAABUAgAAACAA" +
    "AAEAAACMAgAABCAAAAIAAACgAgAAAxAAAAEAAACwAgAABiAAAAEAAAC8AgAAABAAAAEAAADMAgAA");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform("Hello", "Goodbye"),
           new Transform("start", "end"));
  }

  private static void printTransform(Transform t) {
    System.out.println("Result is " + t.getResult());
    System.out.println("take1 is " + t.take1);
    System.out.println("take2 is " + t.take2);
  }
  public static void doTest(Transform t1, Transform t2) {
    printTransform(t1);
    printTransform(t2);
    Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    printTransform(t1);
    printTransform(t2);
  }
}
