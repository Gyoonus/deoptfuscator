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


.class public LInvokeSuper;
.super LSuperClass;

.method public constructor <init>()V
.registers 1
    invoke-direct {v0}, LSuperClass;-><init>()V
    return-void
.end method


.method public run()I
.registers 2
    # Do an invoke super on a non-super class to force complex resolution.
    invoke-super {v1}, LInvokeSuper;->returnInt()I
    move-result v0
    return v0
.end method


.method public returnInt()I
.registers 2
    const v0, 777
    return v0
.end method
