# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTestCase;

.super Ljava/lang/Object;

.method private static flagToString(I)Ljava/lang/String;
  .registers 2

    # The bug is triggered when inlining a method with few Load/StoreLocals but
    # many constants. The switch instruction helps with that.

    sparse-switch p0, :sswitch_data_1a
    const/4 v0, 0x0

    :goto_4
    return-object v0

  :sswitch_5
    const-string v0, "DEFAULT"
    goto :goto_4

  :sswitch_8
    const-string v0, "FLAG_INCLUDE_NOT_IMPORTANT_VIEWS"
    goto :goto_4

  :sswitch_b
    const-string v0, "FLAG_REQUEST_TOUCH_EXPLORATION_MODE"
    goto :goto_4

  :sswitch_e
    const-string v0, "FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY"
    goto :goto_4

  :sswitch_11
    const-string v0, "FLAG_REPORT_VIEW_IDS"
    goto :goto_4

  :sswitch_14
    const-string v0, "FLAG_REQUEST_FILTER_KEY_EVENTS"
    goto :goto_4

  :sswitch_17
    const-string v0, "FLAG_RETRIEVE_INTERACTIVE_WINDOWS"
    goto :goto_4

  :sswitch_data_1a
  .sparse-switch
      0x1 -> :sswitch_5
      0x2 -> :sswitch_8
      0x4 -> :sswitch_b
      0x8 -> :sswitch_e
      0x10 -> :sswitch_11
      0x20 -> :sswitch_14
      0x40 -> :sswitch_17
  .end sparse-switch
.end method

.method public static testCase(I)Ljava/lang/String;
  .registers 2
    invoke-static {v1}, LTestCase;->flagToString(I)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method
