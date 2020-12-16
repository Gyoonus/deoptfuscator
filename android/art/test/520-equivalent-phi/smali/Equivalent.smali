# Copyright (C) 2015 The Android Open Source Project
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

.class public LEquivalent;

.super Ljava/lang/Object;

.method public static method([I)V
   .registers 3
   const/4 v0, 0
   if-eq p0, v0, :first_if
   move-object v0, p0
   :first_if
   if-eqz v0, :second_if
   # Having this move-object used to confuse the type propagation
   # phase of the optimizing compiler: the phase propagates types
   # based on uses, but a move-object disappears after SSA, leaving
   # the compiler with a reference equivalent that has no use. So
   # we would consider the phi equivalent reference of v0, as dead,
   # even though it is the one that has the correct type.
   move-object v1, v0
   :second_if
   return-void
.end method
