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

## CHECK-START: int IrreducibleLoop.test(int) GVN (before)
## CHECK-DAG:                     LoadClass loop:none
## CHECK-DAG:                     LoadClass loop:{{B\d+}} outer_loop:none

## CHECK-START: int IrreducibleLoop.test(int) GVN (after)
## CHECK-DAG:                     LoadClass loop:none
## CHECK-DAG:                     LoadClass loop:{{B\d+}} outer_loop:none
.method public static test(I)I
   .registers 2

   sget v0, LIrreducibleLoop;->field1:I
   sput v0, LIrreducibleLoop;->field2:I

   if-eqz p0, :loop_entry
   goto :exit

   :loop_entry
   if-eqz p0, :irreducible_loop_entry
   sget v0, LIrreducibleLoop;->field2:I
   sput v0, LIrreducibleLoop;->field1:I
   if-eqz v0, :exit
   goto :irreducible_other_loop_entry

   :irreducible_loop_entry
   if-eqz p0, :loop_back_edge
   :irreducible_other_loop_entry
   if-eqz v0, :loop_back_edge
   goto :irreducible_loop_entry

   :loop_back_edge
   goto :loop_entry

   :exit
   return v0
.end method

.field public static field1:I
.field public static field2:I
