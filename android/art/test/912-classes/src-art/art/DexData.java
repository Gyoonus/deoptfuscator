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

package art;

import java.nio.ByteBuffer;
import java.util.Base64;

import dalvik.system.InMemoryDexClassLoader;

public class DexData {
  public static ClassLoader getBootClassLoader() {
    ClassLoader cl = DexData.class.getClassLoader();
    while (cl.getParent() != null) {
      cl = cl.getParent();
    }
    return cl;
  }

  public static ClassLoader create1() {
    return create1(getBootClassLoader());
  }
  public static ClassLoader create1(ClassLoader parent) {
    return create(parent, DEX_DATA_B);
  }

  public static ClassLoader create2() {
    return create2(getBootClassLoader());
  }
  public static ClassLoader create2(ClassLoader parent) {
    return create(parent, DEX_DATA_AC);
  }

  public static ClassLoader create12() {
    return create12(getBootClassLoader());
  }
  public static ClassLoader create12(ClassLoader parent) {
    return create(parent, DEX_DATA_AC, DEX_DATA_B);
  }

  private static ClassLoader create(ClassLoader parent, String... stringData) {
    ByteBuffer byteBuffers[] = new ByteBuffer[stringData.length];
    for (int i = 0; i < stringData.length; i++) {
      byteBuffers[i] = ByteBuffer.wrap(Base64.getDecoder().decode(stringData[i]));
    }
    return new InMemoryDexClassLoader(byteBuffers, parent);
  }

  /*
   * Derived from:
   *
   *   public class A {
   *   }
   *
   *   public class C extends A {
   *   }
   *
   */
  private final static String DEX_DATA_AC =
      "ZGV4CjAzNQD5KyH7WmGuqVEyL+2aKG1nyb27UJaCjFwQAgAAcAAAAHhWNBIAAAAAAAAAAIgBAAAH" +
      "AAAAcAAAAAQAAACMAAAAAQAAAJwAAAAAAAAAAAAAAAMAAACoAAAAAgAAAMAAAAAQAQAAAAEAADAB" +
      "AAA4AQAAQAEAAEgBAABNAQAAUgEAAGYBAAADAAAABAAAAAUAAAAGAAAABgAAAAMAAAAAAAAAAAAA" +
      "AAAAAAABAAAAAAAAAAIAAAAAAAAAAAAAAAEAAAACAAAAAAAAAAEAAAAAAAAAcwEAAAAAAAABAAAA" +
      "AQAAAAAAAAAAAAAAAgAAAAAAAAB9AQAAAAAAAAEAAQABAAAAaQEAAAQAAABwEAIAAAAOAAEAAQAB" +
      "AAAAbgEAAAQAAABwEAAAAAAOAAY8aW5pdD4ABkEuamF2YQAGQy5qYXZhAANMQTsAA0xDOwASTGph" +
      "dmEvbGFuZy9PYmplY3Q7AAFWABEABw4AEQAHDgAAAAEAAIGABIACAAABAAGBgASYAgALAAAAAAAA" +
      "AAEAAAAAAAAAAQAAAAcAAABwAAAAAgAAAAQAAACMAAAAAwAAAAEAAACcAAAABQAAAAMAAACoAAAA" +
      "BgAAAAIAAADAAAAAASAAAAIAAAAAAQAAAiAAAAcAAAAwAQAAAyAAAAIAAABpAQAAACAAAAIAAABz" +
      "AQAAABAAAAEAAACIAQAA";

  /*
   * Derived from:
   *
   *   public class B {
   *   }
   *
   */
  private final static String DEX_DATA_B =
      "ZGV4CjAzNQBgKV6iWFG4aOm5WEy8oGtDZjqsftBgwJ2oAQAAcAAAAHhWNBIAAAAAAAAAACABAAAF" +
      "AAAAcAAAAAMAAACEAAAAAQAAAJAAAAAAAAAAAAAAAAIAAACcAAAAAQAAAKwAAADcAAAAzAAAAOQA" +
      "AADsAAAA9AAAAPkAAAANAQAAAgAAAAMAAAAEAAAABAAAAAIAAAAAAAAAAAAAAAAAAAABAAAAAAAA" +
      "AAAAAAABAAAAAQAAAAAAAAABAAAAAAAAABUBAAAAAAAAAQABAAEAAAAQAQAABAAAAHAQAQAAAA4A" +
      "Bjxpbml0PgAGQi5qYXZhAANMQjsAEkxqYXZhL2xhbmcvT2JqZWN0OwABVgARAAcOAAAAAQAAgYAE" +
      "zAEACwAAAAAAAAABAAAAAAAAAAEAAAAFAAAAcAAAAAIAAAADAAAAhAAAAAMAAAABAAAAkAAAAAUA" +
      "AAACAAAAnAAAAAYAAAABAAAArAAAAAEgAAABAAAAzAAAAAIgAAAFAAAA5AAAAAMgAAABAAAAEAEA" +
      "AAAgAAABAAAAFQEAAAAQAAABAAAAIAEAAA==";
}
