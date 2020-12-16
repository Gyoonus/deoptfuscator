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

.class public LPhiLiveness;

.super Ljava/lang/Object;

.method public static mergeOk(ZB)V
   .registers 5
   const/4 v0, 0x0
   const/4 v1, 0x1
   move v2, v3
   if-eq v1, v0, :else
   move v2, v4
   :else
   invoke-static {}, LPhiLiveness;->regsNativeCall()V
   return-void
.end method

.method public static mergeNotOk(ZF)V
   .registers 5
   const/4 v0, 0x0
   const/4 v1, 0x1
   move v2, v3
   if-eq v1, v0, :else
   move v2, v4
   :else
   invoke-static {}, LPhiLiveness;->regsNativeCall()V
   return-void
.end method

.method public static mergeReferences(LMain;)V
   .registers 4
   const/4 v0, 0x0
   const/4 v1, 0x1
   move-object v2, p0
   if-eq v1, v0, :else
   move v2, v0
   :else
   invoke-static {}, LPhiLiveness;->regsNativeCall()V
   return-void
.end method

.method public static phiEquivalent()F
   .registers 5
   const/4 v0, 0x0
   const/4 v1, 0x1
   move v2, v0
   if-eq v1, v0, :else
   move v2, v1
   :else
   invoke-static {}, LPhiLiveness;->regsNativeCall()V
   return v2
.end method

.method public static phiAllEquivalents(LMain;)V
   .registers 4
   const/4 v0, 0x0
   const/4 v1, 0x1
   move v2, v0
   if-eq v1, v0, :else
   move v2, v0
   :else
   invoke-static {v2, v2, v2}, LPhiLiveness;->regsNativeCallWithParameters(LMain;IF)V
   return-void
.end method

.method public static native regsNativeCall()V
.end method
.method public static native regsNativeCallWithParameters(LMain;IF)V
.end method
