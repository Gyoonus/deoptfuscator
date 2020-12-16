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

# Check that on x86 we don't crash because irreducible loops
# disabled the constant pool optimization.
.method public static test1(IF)F
   .registers 5
   const/16 v0, 1
   const/16 v1, 42

   if-nez p0, :loop_entry
   goto :other_loop_pre_entry

   # The then part: beginning of the irreducible loop.
   :loop_entry
   if-eqz p0, :exit
   add-float v2, p1, v1
   sub-float v2, v2, v1
   div-float v2, v2, v1
   mul-float v2, v2, v1
   :other_loop_entry
   sub-int p0, p0, v0
   goto :loop_entry

   # The other block branching to the irreducible loop.
   # In that block, v4 has no live range.
   :other_loop_pre_entry
   goto :other_loop_entry

   :exit
   return v1
.end method
