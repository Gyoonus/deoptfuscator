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
public class Test1937 {

  private static final boolean PRINT_MESSAGE = false;

  static class Transform {
    public void sayHi() {
      // Use lower 'h' to make sure the string will have a different string id
      // than the transformation (the transformation code is the same except
      // the actual printed String, which was making the test inacurately passing
      // in JIT mode when loading the string from the dex cache, as the string ids
      // of the two different strings were the same).
      // We know the string ids will be different because lexicographically:
      // "Goodbye" < "LTransform;" < "hello".
      System.out.println("hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform {
   *   public void sayHi() {
   *    System.out.println("throwing");
   *    Redefinition.notPresent();
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAJQoABwAPCQAQABEIABIKABMAFAoAFQAWBwAYBwAbAQAGPGluaXQ+AQADKClWAQAE" +
    "Q29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAKU291cmNlRmlsZQEADVRlc3QxOTM3Lmph" +
    "dmEMAAgACQcAHAwAHQAeAQAIdGhyb3dpbmcHAB8MACAAIQcAIgwAIwAJBwAkAQAWYXJ0L1Rlc3Qx" +
    "OTM3JFRyYW5zZm9ybQEACVRyYW5zZm9ybQEADElubmVyQ2xhc3NlcwEAEGphdmEvbGFuZy9PYmpl" +
    "Y3QBABBqYXZhL2xhbmcvU3lzdGVtAQADb3V0AQAVTGphdmEvaW8vUHJpbnRTdHJlYW07AQATamF2" +
    "YS9pby9QcmludFN0cmVhbQEAB3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBABBhcnQv" +
    "UmVkZWZpbml0aW9uAQAKbm90UHJlc2VudAEADGFydC9UZXN0MTkzNwAgAAYABwAAAAAAAgAAAAgA" +
    "CQABAAoAAAAdAAEAAQAAAAUqtwABsQAAAAEACwAAAAYAAQAAACMAAQAMAAkAAQAKAAAALAACAAEA" +
    "AAAMsgACEgO2AAS4AAWxAAAAAQALAAAADgADAAAAJQAIACYACwAnAAIADQAAAAIADgAaAAAACgAB" +
    "AAYAFwAZAAg=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDfmxvwUHv7EEBCvzjdM/uAviWG8eIsKIbsAwAAcAAAAHhWNBIAAAAAAAAAACgDAAAW" +
    "AAAAcAAAAAoAAADIAAAAAgAAAPAAAAABAAAACAEAAAUAAAAQAQAAAQAAADgBAACUAgAAWAEAALoB" +
    "AADCAQAA1gEAAPABAAAAAgAAJAIAAEQCAABbAgAAbwIAAIMCAACXAgAApgIAALECAAC0AgAAuAIA" +
    "AMUCAADLAgAA1wIAANwCAADlAgAA7AIAAPYCAAABAAAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAA" +
    "CAAAAAkAAAAMAAAADAAAAAkAAAAAAAAADQAAAAkAAAC0AQAACAAFABEAAAAAAAAAEAAAAAEAAAAA" +
    "AAAAAQAAABMAAAAFAAEAEgAAAAYAAAAAAAAAAQAAAAAAAAAGAAAAAAAAAAoAAACkAQAAGAMAAAAA" +
    "AAACAAAACQMAAA8DAAABAAEAAQAAAP0CAAAEAAAAcBAEAAAADgADAAEAAgAAAAIDAAALAAAAYgAA" +
    "ABoBFABuIAMAEABxAAAAAAAOAAAAWAEAAAAAAAAAAAAAAAAAAAEAAAAHAAY8aW5pdD4AEkxhcnQv" +
    "UmVkZWZpbml0aW9uOwAYTGFydC9UZXN0MTkzNyRUcmFuc2Zvcm07AA5MYXJ0L1Rlc3QxOTM3OwAi" +
    "TGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lu" +
    "bmVyQ2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGph" +
    "dmEvbGFuZy9TdHJpbmc7ABJMamF2YS9sYW5nL1N5c3RlbTsADVRlc3QxOTM3LmphdmEACVRyYW5z" +
    "Zm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAApub3RQcmVzZW50AANvdXQAB3ByaW50bG4A" +
    "BXNheUhpAAh0aHJvd2luZwAFdmFsdWUAIwAHDgAlAAcOeDwAAgMBFRgCAgQCDgQIDxcLAAABAQGA" +
    "gATkAgIB/AIAABAAAAAAAAAAAQAAAAAAAAABAAAAFgAAAHAAAAACAAAACgAAAMgAAAADAAAAAgAA" +
    "APAAAAAEAAAAAQAAAAgBAAAFAAAABQAAABABAAAGAAAAAQAAADgBAAADEAAAAQAAAFgBAAABIAAA" +
    "AgAAAGQBAAAGIAAAAQAAAKQBAAABEAAAAQAAALQBAAACIAAAFgAAALoBAAADIAAAAgAAAP0CAAAE" +
    "IAAAAgAAAAkDAAAAIAAAAQAAABgDAAAAEAAAAQAAACgDAAA=");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    t.sayHi();
    Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    try {
      t.sayHi();
    } catch (Throwable e) {
      System.out.println("Caught exception " + e.getClass().getName());
      if (PRINT_MESSAGE) {
        System.out.println("Message: " + e.getMessage());
      }
    }
  }
}
