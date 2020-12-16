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

.class public LZ;
.super LA;

.method public constructor <init>()V
.registers 1
    invoke-direct {v0}, LA;-><init>()V
    return-void
.end method

.method public foo()V
.registers 3
    new-instance v0, LY;
    invoke-direct {v0}, LY;-><init>()V
    invoke-super {v0}, LY;->foo()V
    return-void
.end method
