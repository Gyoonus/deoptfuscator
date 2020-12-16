#
# Copyright (C) 2014 The Android Open Source Project
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

.class public LNewInstance;
.super Ljava/lang/Object;

.method public constructor <init>()V
      .registers 1
       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public newInstanceInterface()Ljava/lang/Object;
      .registers 5
      new-instance v1, LTestInterface;
      # invoke-direct {v3}, LTestInterface;-><init>()V
      # intentionally return v4 ("this")
      return-object v4
.end method

.method public newInstanceClass()Ljava/lang/Object;
      .registers 5
      new-instance v1, LTestClass;
      # invoke-direct {v3}, LTestClass;-><init>()V
      # intentionally return v4 ("this")
      return-object v4
.end method

.method public newInstancePrivateClass()Ljava/lang/Object;
      .registers 5
      new-instance v1, Lpkg/ProtectedClass;
      # invoke-direct {v3}, Lpck/ProtectedClass;-><init>()V
      # intentionally return v4 ("this")
      return-object v4
.end method

.method public newInstanceUnknownClass()Ljava/lang/Object;
      .registers 5
      new-instance v1, LUnknownClass;
      # invoke-direct {v3}, LUnknownClass;-><init>()V
      # intentionally return v4 ("this")
      return-object v4
.end method
