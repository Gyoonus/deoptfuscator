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

## CHECK-START-X86: int IrreducibleLoop.simpleLoop1(int) dead_code_elimination$initial (before)
## CHECK-DAG: <<Constant:i\d+>>   IntConstant 42
## CHECK-DAG:                     Goto irreducible:true
## CHECK-DAG:                     InvokeStaticOrDirect [<<Constant>>{{(,[ij]\d+)?}}] loop:none
## CHECK-DAG:                     InvokeStaticOrDirect [{{i\d+}}{{(,[ij]\d+)?}}] loop:none
.method public static simpleLoop1(I)I
   .registers 3
   const/16 v0, 42
   invoke-static {v0}, LIrreducibleLoop;->$noinline$m(I)V
   if-eqz p0, :b22
   goto :b34

   :b34
   goto :b20

   :b20
   if-nez p0, :b45
   goto :b46

   :b46
   goto :b21

   :b21
   goto :b34

   :b22
   :try_start
   div-int v0, v0, v0
   :try_end
   .catchall {:try_start .. :try_end} :b34
   goto :b20

   :b45
   invoke-static {v0}, LIrreducibleLoop;->$noinline$m(I)V
   goto :b26

   :b26
   return v0
.end method

## CHECK-START-X86: int IrreducibleLoop.simpleLoop2(int) dead_code_elimination$initial (before)
## CHECK-DAG: <<Constant:i\d+>>   IntConstant 42
## CHECK-DAG:                     Goto irreducible:true
## CHECK-DAG:                     InvokeStaticOrDirect [<<Constant>>{{(,[ij]\d+)?}}] loop:none
## CHECK-DAG:                     InvokeStaticOrDirect [{{i\d+}}{{(,[ij]\d+)?}}] loop:none
.method public static simpleLoop2(I)I
   .registers 3
   const/16 v0, 42

   :try_start1
   invoke-static {v0}, LIrreducibleLoop;->$noinline$m(I)V
   div-int v0, v0, v0
   :try_end1
   .catchall {:try_start1 .. :try_end1} :b14

   :try_start2
   invoke-static {v0}, LIrreducibleLoop;->$noinline$m(I)V
   div-int v0, v0, v0
   :try_end2
   .catchall {:try_start2 .. :try_end2} :b45
   goto :b49

   :b14
   goto :b15

   :b45
   goto :b15

   :b15
   goto :b16

   :b16
   goto :b49

   :b49
   invoke-static {v0}, LIrreducibleLoop;->$noinline$m(I)V
   div-int v0, v0, v0
   :try_end3
   .catchall {:b49 .. :try_end3} :b49
   if-eqz p0, :b16
   goto :b26

   :b26
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
