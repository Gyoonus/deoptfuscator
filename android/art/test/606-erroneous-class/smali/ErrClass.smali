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

.class public final LErrClass;
.super Ljava/lang/Object;

.field public g:Ljava/lang/Object;

.method public foo()V
    .registers 6
    # Use a new instance before initializing it => hard verifier error.
    new-instance v0, LSomeClass;
    iput-object v0, p0, LErrClass;->g:Ljava/lang/Object;
    return-void
.end method
