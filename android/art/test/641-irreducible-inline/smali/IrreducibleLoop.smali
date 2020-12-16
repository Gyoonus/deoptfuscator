# Copyright (C) 2017 The Android Open Source Project
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
   .registers 3
   const/16 v0, 42
   if-eqz p0, :loop_entry
   goto :other_loop_pre_entry

   # The then part: beginning of the irreducible loop.
   :loop_entry
   if-nez p0, :exit
   invoke-static {v0},LIrreducibleLoop;->foo(I)V
   :other_loop_entry
   goto :loop_entry

   # The else part.
   :other_loop_pre_entry
   if-eqz p0, :other_loop_entry
   invoke-static {v0},LIrreducibleLoop;->foo(I)V
   goto :other_loop_entry

   :exit
   return v0
.end method

.method public static foo(I)V
   .registers 3
   const/16 v0, 0
   sget-boolean v1,LIrreducibleLoop;->doThrow:Z
   if-eqz v1, :exit
   # Inlining a method that throws requires re-computing loop information
   # which is unsupported when the caller has an irreducible loop.
   throw v0
   :exit
   return-void
.end method

.field public static doThrow:Z
