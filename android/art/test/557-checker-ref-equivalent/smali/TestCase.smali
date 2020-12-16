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

## CHECK-START: void TestCase.testIntRefEquivalent() builder (after)
## CHECK-NOT: Phi
.method public static testIntRefEquivalent()V
    .registers 4

    const v0, 0

    :try_start
    invoke-static {v0,v0}, LTestCase;->foo(ILjava/lang/Object;)V
    if-eqz v0, :end_if
    const v0, 0
    :end_if
    invoke-static {v0,v0}, LTestCase;->foo(ILjava/lang/Object;)V
    goto :no_catch
    :try_end

    .catch Ljava/lang/Exception; {:try_start .. :try_end} :exception
    :exception
    # We used to have a reference and an integer phi equivalents here, which
    # broke the invariant of not sharing the same spill slot between those two
    # types.
    invoke-static {v0,v0}, LTestCase;->foo(ILjava/lang/Object;)V

    :no_catch
    goto :try_start
    return-void

.end method

.method public static foo(ILjava/lang/Object;)V
    .registers 4
    return-void
.end method
