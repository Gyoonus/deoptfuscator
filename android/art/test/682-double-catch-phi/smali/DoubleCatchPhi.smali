# Copyright (C) 2018 The Android Open Source Project
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

.class public LDoubleCatchPhi;

.super Ljava/lang/Object;

.field public mValue:D

.method public constructor <init>()V
.registers 1
    invoke-direct {v0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public strangeMethod(F)V
.registers 6
   move-object v2, v4
   monitor-enter v4
:try_start1
   float-to-double v0, v5
   iput-wide v0, v4, LDoubleCatchPhi;->mValue:D
   monitor-exit v2
:try_end1
   goto :end_catch
:catch
:try_start2
   move-exception v3
   monitor-exit v2
:try_end2
   throw v3
:end_catch
   return-void
.catchall {:try_start1 .. :try_end1} :catch
.catchall {:try_start2 .. :try_end2} :catch
.end method
