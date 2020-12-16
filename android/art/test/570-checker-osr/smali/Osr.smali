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

.class public LOsr;

.super Ljava/lang/Object;

# Check that blocks only havig nops are not merged when they are loop headers.
# This ensures we can do on-stack replacement for branches to those nop blocks.

## CHECK-START: int Osr.simpleLoop(int, int) dead_code_elimination$final (after)
## CHECK-DAG:                     SuspendCheck loop:<<OuterLoop:B\d+>> outer_loop:none
## CHECK-DAG:                     SuspendCheck loop:{{B\d+}} outer_loop:<<OuterLoop>>
.method public static simpleLoop(II)I
   .registers 3
   const/16 v0, 0
   :nop_entry
   nop
   :loop_entry
   add-int v0, v0, v0
   if-eq v0, v1, :loop_entry
   if-eq v0, v2, :nop_entry
   return v0
.end method
