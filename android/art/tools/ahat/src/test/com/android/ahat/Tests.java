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

import org.junit.runner.JUnitCore;

public class Tests {
  public static void main(String[] args) {
    if (args.length == 0) {
      args = new String[]{
        "com.android.ahat.DiffFieldsTest",
        "com.android.ahat.DiffTest",
        "com.android.ahat.DominatorsTest",
        "com.android.ahat.HtmlEscaperTest",
        "com.android.ahat.InstanceTest",
        "com.android.ahat.NativeAllocationTest",
        "com.android.ahat.ObjectHandlerTest",
        "com.android.ahat.OverviewHandlerTest",
        "com.android.ahat.PerformanceTest",
        "com.android.ahat.ProguardMapTest",
        "com.android.ahat.RootedHandlerTest",
        "com.android.ahat.QueryTest",
        "com.android.ahat.SiteHandlerTest",
        "com.android.ahat.SiteTest",
      };
    }
    JUnitCore.main(args);
  }
}

