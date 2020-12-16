/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.ahat;

/**
 * A menu showed in the UI that can be used to jump to common pages.
 */
class Menu {
  private static DocString mMenu =
    DocString.link(DocString.uri("/"), DocString.text("overview"))
      .append(" - ")
      .appendLink(DocString.uri("rooted"), DocString.text("rooted"))
      .append(" - ")
      .appendLink(DocString.uri("sites"), DocString.text("allocations"));

  /**
   * Returns the menu as a DocString.
   */
  public static DocString getMenu() {
    return mMenu;
  }
}
