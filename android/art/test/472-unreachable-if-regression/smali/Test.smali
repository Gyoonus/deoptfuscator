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

.method public static UnreachableIf()V
    .registers 1
    return-void
    : unreachable
    not-int v0, v0
    if-lt v0, v0, :unreachable
    # fall-through out of code item
.end method

.method public static UnreachablePackedSwitch()V
    .registers 1
    return-void
    : unreachable
    goto :pswitch_2
    :pswitch_data
    .packed-switch 1
        :pswitch_1
        :pswitch_2
        :pswitch_1
        :pswitch_2
    .end packed-switch
    :pswitch_1
    not-int v0, v0
    :pswitch_2
    packed-switch v0, :pswitch_data
    # fall-through out of code item
.end method
