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

.method public static simpleLoop(I)I
   .registers 5
   const/16 v0, 42
   const/16 v1, 42
   const-wide/high16 v2, 0x4000000000000000L
   if-eq p0, v0, :other_loop_entry
   :loop_entry
   invoke-static {v1, v1}, LMain;->$inline$foo(FF)V
   invoke-static {v2, v3, v2, v3}, LMain;->$inline$foo(DD)V
   if-ne p0, v0, :exit
   add-int v0, v0, v0
   :other_loop_entry
   add-int v0, v0, v0
   goto :loop_entry
   :exit
   return v0
.end method
