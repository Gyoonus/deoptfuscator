/*
 * Copyright (C) 2014 The Android Open Source Project
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
  static class Other {
  }

  static class OtherWithClinit {
    static int a;
    static {
      System.out.println("Hello from OtherWithClinit");
      a = 42;
    }
  }

  static class OtherWithClinit2 {
    static int a;
    static {
      System.out.println("Hello from OtherWithClinit2");
      a = 43;
    }
  }

  public static void main(String[] args) {
    // Call methods twice in case they have a slow path.

    System.out.println($opt$LoadThisClass());
    System.out.println($opt$LoadThisClass());

    System.out.println($opt$LoadOtherClass());
    System.out.println($opt$LoadOtherClass());

    System.out.println($opt$LoadSystemClass());
    System.out.println($opt$LoadSystemClass());

    $opt$ClinitCheckAndLoad();
    $opt$ClinitCheckAndLoad();

    $opt$LoadAndClinitCheck();
    $opt$LoadAndClinitCheck();
  }

  public static Class<?> $opt$LoadThisClass() {
    return Main.class;
  }

  public static Class<?> $opt$LoadOtherClass() {
    return Other.class;
  }

  public static Class<?> $opt$LoadSystemClass() {
    return System.class;
  }

  public static void $opt$ClinitCheckAndLoad() {
    System.out.println(OtherWithClinit.a);
    System.out.println(OtherWithClinit.class);
  }

  public static void $opt$LoadAndClinitCheck() {
    System.out.println(OtherWithClinit2.class);
    System.out.println(OtherWithClinit2.a);
  }
}
