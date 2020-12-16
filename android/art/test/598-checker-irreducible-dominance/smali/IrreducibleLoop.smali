# Copyright (C) 2016 The Android Open Source Project
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

.class public LIrreducibleLoop;
.super Ljava/lang/Object;

# Test case in which `inner_back_edge` is not dominated by `inner_header` and
# causes `outer_back_edge` to not be dominated by `outer_header`. HGraphBuilder
# not do a fix-point iteration and would miss the path to `outer_back_edge`
# through `inner_back_edge` and incorrectly label the outer loop non-irreducible.

## CHECK-START: int IrreducibleLoop.dominance(int) builder (after)
## CHECK:         Add irreducible:true

.method public static dominance(I)I
    .registers 2

    if-eqz p0, :outer_header
    goto :inner_back_edge

    :outer_header
    if-eqz p0, :inner_header

    :outer_branch_exit
    if-eqz p0, :outer_merge
    return p0

    :inner_header
    goto :outer_merge

    :inner_back_edge
    goto :inner_header

    :outer_merge
    if-eqz p0, :inner_back_edge

    :outer_back_edge
    add-int/2addr p0, p0
    goto :outer_header

.end method
