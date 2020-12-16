#
# Copyright (C) 2016 The Android Open Source Project
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

.class public LMerge;
.super Ljava/lang/Object;

# Method that selects between x = new Integer[] or new AnError[],
# Reference type propagation should correctly see error in component type.
.method public static select(Z)Ljava/lang/Object;
    .registers 2
    const/16 v0, 10
    if-eqz v1, :Skip
    new-array v0, v0, [LAnError;
    goto :Done
:Skip
    new-array v0, v0, [Ljava/lang/Integer;
:Done
    return-object v0
.end method
