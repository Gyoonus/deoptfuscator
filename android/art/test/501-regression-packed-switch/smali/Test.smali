#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTest;

.super Ljava/lang/Object;

.method public static EmptyPackedSwitch(I)I
  .registers 1
  packed-switch v0, :pswitch_data_6a
  const/4 v0, 0x5
  return v0

  :pswitch_data_6a
  .packed-switch 0x0
  .end packed-switch
.end method

.method public static PackedSwitchAfterData(I)I
  .registers 1
  goto :pswitch_instr

  :case0
  const/4 v0, 0x1
  return v0

  :pswitch_data
  .packed-switch 0x0
    :case0
    :case1
  .end packed-switch

  :pswitch_instr
  packed-switch v0, :pswitch_data
  const/4 v0, 0x7
  return v0

  :case1
  const/4 v0, 0x4
  return v0

.end method
