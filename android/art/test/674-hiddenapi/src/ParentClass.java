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

public class ParentClass {
  public ParentClass() {}

  // INSTANCE FIELD

  public int fieldPublicWhitelist = 211;
  int fieldPackageWhitelist = 212;
  protected int fieldProtectedWhitelist = 213;
  private int fieldPrivateWhitelist = 214;
  public int fieldPublicWhitelistB = 215;

  public int fieldPublicLightGreylist = 221;
  int fieldPackageLightGreylist = 222;
  protected int fieldProtectedLightGreylist = 223;
  private int fieldPrivateLightGreylist = 224;
  public int fieldPublicLightGreylistB = 225;

  public int fieldPublicDarkGreylist = 231;
  int fieldPackageDarkGreylist = 232;
  protected int fieldProtectedDarkGreylist = 233;
  private int fieldPrivateDarkGreylist = 234;
  public int fieldPublicDarkGreylistB = 235;

  public int fieldPublicBlacklist = 241;
  int fieldPackageBlacklist = 242;
  protected int fieldProtectedBlacklist = 243;
  private int fieldPrivateBlacklist = 244;
  public int fieldPublicBlacklistB = 245;

  // STATIC FIELD

  public static int fieldPublicStaticWhitelist = 111;
  static int fieldPackageStaticWhitelist = 112;
  protected static int fieldProtectedStaticWhitelist = 113;
  private static int fieldPrivateStaticWhitelist = 114;
  public static int fieldPublicStaticWhitelistB = 115;

  public static int fieldPublicStaticLightGreylist = 121;
  static int fieldPackageStaticLightGreylist = 122;
  protected static int fieldProtectedStaticLightGreylist = 123;
  private static int fieldPrivateStaticLightGreylist = 124;
  public static int fieldPublicStaticLightGreylistB = 125;

  public static int fieldPublicStaticDarkGreylist = 131;
  static int fieldPackageStaticDarkGreylist = 132;
  protected static int fieldProtectedStaticDarkGreylist = 133;
  private static int fieldPrivateStaticDarkGreylist = 134;
  public static int fieldPublicStaticDarkGreylistB = 135;

  public static int fieldPublicStaticBlacklist = 141;
  static int fieldPackageStaticBlacklist = 142;
  protected static int fieldProtectedStaticBlacklist = 143;
  private static int fieldPrivateStaticBlacklist = 144;
  public static int fieldPublicStaticBlacklistB = 145;

  // INSTANCE METHOD

  public int methodPublicWhitelist() { return 411; }
  int methodPackageWhitelist() { return 412; }
  protected int methodProtectedWhitelist() { return 413; }
  private int methodPrivateWhitelist() { return 414; }

  public int methodPublicLightGreylist() { return 421; }
  int methodPackageLightGreylist() { return 422; }
  protected int methodProtectedLightGreylist() { return 423; }
  private int methodPrivateLightGreylist() { return 424; }

  public int methodPublicDarkGreylist() { return 431; }
  int methodPackageDarkGreylist() { return 432; }
  protected int methodProtectedDarkGreylist() { return 433; }
  private int methodPrivateDarkGreylist() { return 434; }

  public int methodPublicBlacklist() { return 441; }
  int methodPackageBlacklist() { return 442; }
  protected int methodProtectedBlacklist() { return 443; }
  private int methodPrivateBlacklist() { return 444; }

  // STATIC METHOD

  public static int methodPublicStaticWhitelist() { return 311; }
  static int methodPackageStaticWhitelist() { return 312; }
  protected static int methodProtectedStaticWhitelist() { return 313; }
  private static int methodPrivateStaticWhitelist() { return 314; }

  public static int methodPublicStaticLightGreylist() { return 321; }
  static int methodPackageStaticLightGreylist() { return 322; }
  protected static int methodProtectedStaticLightGreylist() { return 323; }
  private static int methodPrivateStaticLightGreylist() { return 324; }

  public static int methodPublicStaticDarkGreylist() { return 331; }
  static int methodPackageStaticDarkGreylist() { return 332; }
  protected static int methodProtectedStaticDarkGreylist() { return 333; }
  private static int methodPrivateStaticDarkGreylist() { return 334; }

  public static int methodPublicStaticBlacklist() { return 341; }
  static int methodPackageStaticBlacklist() { return 342; }
  protected static int methodProtectedStaticBlacklist() { return 343; }
  private static int methodPrivateStaticBlacklist() { return 344; }

  // CONSTRUCTOR

  // Whitelist
  public ParentClass(int x, short y) {}
  ParentClass(float x, short y) {}
  protected ParentClass(long x, short y) {}
  private ParentClass(double x, short y) {}

  // Light greylist
  public ParentClass(int x, boolean y) {}
  ParentClass(float x, boolean y) {}
  protected ParentClass(long x, boolean y) {}
  private ParentClass(double x, boolean y) {}

  // Dark greylist
  public ParentClass(int x, byte y) {}
  ParentClass(float x, byte y) {}
  protected ParentClass(long x, byte y) {}
  private ParentClass(double x, byte y) {}

  // Blacklist
  public ParentClass(int x, char y) {}
  ParentClass(float x, char y) {}
  protected ParentClass(long x, char y) {}
  private ParentClass(double x, char y) {}

  // HELPERS

  public int callMethodPublicWhitelist() { return methodPublicWhitelist(); }
  public int callMethodPackageWhitelist() { return methodPackageWhitelist(); }
  public int callMethodProtectedWhitelist() { return methodProtectedWhitelist(); }

  public int callMethodPublicLightGreylist() { return methodPublicLightGreylist(); }
  public int callMethodPackageLightGreylist() { return methodPackageLightGreylist(); }
  public int callMethodProtectedLightGreylist() { return methodProtectedLightGreylist(); }

  public int callMethodPublicDarkGreylist() { return methodPublicDarkGreylist(); }
  public int callMethodPackageDarkGreylist() { return methodPackageDarkGreylist(); }
  public int callMethodProtectedDarkGreylist() { return methodProtectedDarkGreylist(); }

  public int callMethodPublicBlacklist() { return methodPublicBlacklist(); }
  public int callMethodPackageBlacklist() { return methodPackageBlacklist(); }
  public int callMethodProtectedBlacklist() { return methodProtectedBlacklist(); }

}
