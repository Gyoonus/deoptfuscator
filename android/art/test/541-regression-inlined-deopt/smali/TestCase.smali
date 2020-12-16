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

.method private static $inline$depth1([I)V
    .registers 3

    # Expects array in v2.

    const v0, 0x0

    const v1, 0x3
    aput v0, p0, v1

    const v1, 0x4
    aput v0, p0, v1

    return-void
.end method

.method private static $inline$depth0([I)V
    .registers 1

    # Expects array in v0.

    invoke-static {p0}, LTestCase;->$inline$depth1([I)V
    return-void
.end method

.method public static foo()V
    .registers 10

    # Create a new array short enough to throw AIOOB in $inline$depth1.
    # Make sure the reference is not stored in the same vreg as used by
    # the inlined methods.

    const v5, 0x3
    new-array v6, v5, [I

    invoke-static {v6}, LTestCase;->$inline$depth0([I)V
    return-void
.end method
