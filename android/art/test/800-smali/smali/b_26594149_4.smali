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

.class public LB26594149_4;
.super Ljava/lang/Object;

.field public field:Ljava/lang/String;

.method public constructor <init>()V
    .registers 4
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static run()V
    .registers 4

    new-instance v1, LB26594149_4;
    invoke-direct {v1}, LB26594149_4;-><init>()V

    new-instance v0, Ljava/lang/String;

    # Illegal operation.
    iput-object v0, v1, LB26594149_4;->field:Ljava/lang/String;

    return-void
  .end method
