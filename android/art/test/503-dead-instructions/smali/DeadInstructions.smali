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

.class public LDeadInstructions;

.super Ljava/lang/Object;

.method public static method1()V
   .registers 2
   return-void
   # Create a label and a branch to that label to trick the
   # optimizing compiler into thinking the invoke is live.
   :start
   const/4 v0, 0
   const/4 v1, 0
   # Provide more arguments than we should. Because this is dead
   # code, the verifier will not check the argument count. So
   # the compilers must do the same.
   invoke-static {v0, v1}, LDeadInstructions;->method1()V
   goto :start
.end method

.method public static method2(J)V
   .registers 3
   return-void
   :start
   const/4 v0, 0
   const/4 v1, 0
   const/4 v2, 0
   # Give a non-sequential pair for the long argument.
   invoke-static {v0, v2}, LDeadInstructions;->method2(J)V
   goto :start
.end method

.method public static method3()V
   .registers 1
   return-void
   :start
   const/4 v0, 0
   # Give one half of a pair.
   invoke-static {v0}, LDeadInstructions;->method2(J)V
   goto :start
.end method

.method public static method4()V
   .registers 2
   return-void
   :start
   # Provide less arguments than we should.
   invoke-static {}, LDeadInstructions;->method3(J)V
   goto :start
.end method
