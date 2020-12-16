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

.class public LTest;

.super Ljava/lang/Object;

.method public static test([I)I
    .locals 2
    const/4 v0, 0
    :try1_begin
    array-length v1, p0
    :try1_end
    add-int/lit8 v0, v1, -1
    :try2_begin
    aget v0, p0, v0
    :try2_end
    :end
    return v0

    :catch_all
    # Regression test for bug 32545860:
    #     SimplifyIfs() would have redirected exception handler edges leading here.
    # Note: There is no move-exception here to prevent matching the SimplifyIfs() pattern.
    if-eqz v0, :is_zero
    const/4 v0, -1
    goto :end
    :is_zero
    const/4 v0, -2
    goto :end

    .catchall {:try1_begin .. :try1_end } :catch_all
    .catchall {:try2_begin .. :try2_end } :catch_all
.end method

.method public static test2([II)I
    .locals 3
    move v0, p1
    :try_begin
    array-length v1, p0
    add-int/lit8 v1, v1, -1
    add-int/lit8 v0, v0, 1
    aget v1, p0, v1
    const/4 v0, 2
    aget v2, p0, p1
    const/4 v0, 3
    :try_end
    :end
    return v0

    :catch_all
    # Regression test for bug 32546110:
    #     SimplifyIfs() would have looked at predecessors of this block based on the indexes
    #     of the catch Phi's inputs. For catch blocks these two arrays are unrelated, so
    #     this caused out-of-range access triggering a DCHECK() in dchecked_vector<>.
    # Note: There is no move-exception here to prevent matching the SimplifyIfs() pattern.
    if-eqz v0, :is_zero
    const/4 v0, -1
    goto :end
    :is_zero
    const/4 v0, -2
    goto :end

    .catchall {:try_begin .. :try_end } :catch_all
.end method
