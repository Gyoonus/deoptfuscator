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

public interface ParentInterface {
  // STATIC FIELD
  static int fieldPublicStaticWhitelist = 11;
  static int fieldPublicStaticLightGreylist = 12;
  static int fieldPublicStaticDarkGreylist = 13;
  static int fieldPublicStaticBlacklist = 14;

  // INSTANCE METHOD
  int methodPublicWhitelist();
  int methodPublicLightGreylist();
  int methodPublicDarkGreylist();
  int methodPublicBlacklist();

  // STATIC METHOD
  static int methodPublicStaticWhitelist() { return 21; }
  static int methodPublicStaticLightGreylist() { return 22; }
  static int methodPublicStaticDarkGreylist() { return 23; }
  static int methodPublicStaticBlacklist() { return 24; }

  // DEFAULT METHOD
  default int methodPublicDefaultWhitelist() { return 31; }
  default int methodPublicDefaultLightGreylist() { return 32; }
  default int methodPublicDefaultDarkGreylist() { return 33; }
  default int methodPublicDefaultBlacklist() { return 34; }
}
