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

## CHECK-START-X86: int IrreducibleLoop.simpleLoop(int) dead_code_elimination$initial (before)
## CHECK-DAG: <<Constant:i\d+>>   IntConstant 42
## CHECK-DAG:                     InvokeStaticOrDirect [<<Constant>>{{(,[ij]\d+)?}}] loop:{{B\d+}} irreducible:true
## CHECK-DAG:                     InvokeStaticOrDirect [<<Constant>>{{(,[ij]\d+)?}}] loop:none
.method public static simpleLoop(I)I
   .registers 3
   const/16 v0, 42
   if-eqz p0, :loop_entry
   goto :other_loop_pre_entry

   # The then part: beginning of the irreducible loop.
   :loop_entry
   if-nez p0, :exit
   invoke-static {v0},LIrreducibleLoop;->$noinline$m(I)V
   :other_loop_entry
   goto :loop_entry

   # The else part: a block uses the ArtMethod and branches to
   # a block that doesn't. The register allocator used to trip there, as the
   # ArtMethod was a live_in of the last block before the loop, but did not have
   # a location due to our liveness analysis.
   :other_loop_pre_entry
   if-eqz p0, :other_loop_entry
   invoke-static {v0},LIrreducibleLoop;->$noinline$m(I)V
   goto :other_loop_entry

   :exit
   return v0
.end method

.method public static $noinline$m(I)V
   .registers 3
   const/16 v0, 0
   sget-boolean v1,LIrreducibleLoop;->doThrow:Z
   if-eqz v1, :exit
   # Prevent inlining.
   throw v0
   :exit
   return-void
.end method

.field public static doThrow:Z
