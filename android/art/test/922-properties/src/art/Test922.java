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

import java.util.Set;
import java.util.TreeSet;

public class Test922 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    Set<String> recommendedProperties = getRecommendedProperties();

    System.out.println("Recommended properties:");
    for (String key : recommendedProperties) {
      checkProperty(key);
    }

    Set<String> allProperties = getAllProperties();

    Set<String> retained = new TreeSet<String>(recommendedProperties);
    retained.retainAll(allProperties);
    if (!retained.equals(recommendedProperties)) {
      Set<String> missing = new TreeSet<String>(recommendedProperties);
      missing.removeAll(retained);
      System.out.println("Missing recommended properties: " + missing);
    }

    Set<String> nonRecommended = new TreeSet<String>(allProperties);
    nonRecommended.removeAll(recommendedProperties);

    System.out.println("Other properties:");
    for (String key : nonRecommended) {
      checkProperty(key);
    }

    System.out.println("Non-specified property:");
    String key = generate(allProperties);
    checkProperty(key);

    System.out.println("Non-specified property (2):");
    String key2 = generateUnique(allProperties);
    checkProperty(key2);
  }

  private static Set<String> getRecommendedProperties() {
    Set<String> keys = new TreeSet<String>();
    keys.add("java.vm.vendor");
    keys.add("java.vm.version");
    keys.add("java.vm.name");
    keys.add("java.vm.info");
    keys.add("java.library.path");
    keys.add("java.class.path");
    return keys;
  }

  private static Set<String> getAllProperties() {
    Set<String> keys = new TreeSet<String>();
    String[] props = getSystemProperties();
    for (String p : props) {
      keys.add(p);
    }
    return keys;
  }

  private static boolean equals(String s1, String s2) {
    if (s1 == null && s2 == null) {
      return true;
    } else if (s1 != null) {
      return s1.equals(s2);
    } else {
      return false;
    }
  }

  private static void checkProperty(String key) {
    System.out.print(" \"" + key + "\": ");
    String err = null;
    String value = null;
    try {
      value = getSystemProperty(key);
    } catch (RuntimeException e) {
      err = e.getMessage();
    }
    String sysValue = System.getProperty(key);
    if (equals(value, sysValue)) {
      System.out.print("OK");
      if (err != null) {
        System.out.println(" !!!" + err);
      } else {
        System.out.println();
      }
    } else {
      System.out.println("ERROR !!!" + err);
    }

    System.out.print("  Setting value to \"abc\": ");
    try {
      setSystemProperty(key, "abc");
      System.out.println("SUCCEEDED");
    } catch (RuntimeException e) {
      System.out.println("!!!" + e.getMessage());
    }
  }

  private static String generateUnique(Set<String> others) {
    // Construct something. To be deterministic, just use "a+".
    StringBuilder sb = new StringBuilder("a");
    for (;;) {
      String key = sb.toString();
      if (!others.contains(key)) {
        return key;
      }
      sb.append('a');
    }
  }

  private static String generate(Set<String> others) {
    // First check for something in the overall System properties.
    TreeSet<String> sysProps = new TreeSet<String>(System.getProperties().stringPropertyNames());
    sysProps.removeAll(others);
    if (!sysProps.isEmpty()) {
      // Find something that starts with "java" or "os," trying to be platform-independent.
      for (String s: sysProps) {
        if (s.startsWith("java.") || s.startsWith("os.")) {
          return s;
        }
      }
      // Just return the first thing.
      return sysProps.iterator().next();
    }

    return generateUnique(others);
  }

  private static native String[] getSystemProperties();
  private static native String getSystemProperty(String key);
  private static native void setSystemProperty(String key, String value);
}
