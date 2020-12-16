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

.method public static testCase([BLMain;)V
    .registers 12
    const/4 v2, 0
    array-length v0, v10
    div-int/lit8 v0, v0, 7
    invoke-static {v2, v0}, Ljava/lang/Math;->max(II)I
    move-result v7
    move v6, v2
    move v3, v2
    :label5
    if-ge v6, v7, :label1
    const-wide/16 v0, 0
    move-wide v4, v0
    move v1, v2
    move v0, v3
    :label4
    const/4 v3, 6
    if-ge v1, v3, :label2
    const/16 v3, 8
    shl-long/2addr v4, v3
    add-int/lit8 v3, v0, 1
    aget-byte v0, v10, v0
    if-gez v0, :label3
    add-int/lit16 v0, v0, 256
    :label3
    int-to-long v8, v0
    or-long/2addr v4, v8
    add-int/lit8 v0, v1, 1
    move v1, v0
    move v0, v3
    goto :label4
    :label2
    add-int/lit8 v3, v0, 1
    aget-byte v0, v10, v0
    invoke-interface {v11, v4, v5, v0}, LItf;->invokeInterface(JI)V
    add-int/lit8 v0, v6, 1
    move v6, v0
    goto :label5
    :label1
    return-void
.end method
