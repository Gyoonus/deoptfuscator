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

# Check that the result of aget on null can be used as a float.

.class public LNullArraySuccessInt2Float;

.super Ljava/lang/Object;

.method public static floatMethod(F)V
   .registers 1
   return-void
.end method

.method public static method()V
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget v0, v0, v1
   invoke-static { v0 }, LNullArraySuccessInt2Float;->floatMethod(F)V
   return-void
.end method
