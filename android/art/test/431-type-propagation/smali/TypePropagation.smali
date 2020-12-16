# Copyright (C) 2014 The Android Open Source Project
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

.class public LTypePropagation;

.super Ljava/lang/Object;

.method public static method([I)V
   .registers 3
   const/4 v0, 0
   aget v1, v2, v0
   add-int v2, v1, v0
   if-eq v1, v0, :end
   # Putting a float in v1 will lead to the creation of a phi with one
   # float input and one integer input. Since the SSA builder trusts
   # the verifier, it assumes that the integer input must be converted
   # to float. However, since v0 is not used afterwards, the verifier
   # hasn't ensured that. Therefore, the compiler must remove
   # the phi prior to doing type propagation.
   int-to-float v1, v0
   :end
   # Do a call to create an environment that will capture all Dex registers.
   # This environment is the reason why a phi is created at the join block
   # of the if.
   invoke-static {}, LTypePropagation;->emptyMethod()V
   return-void
.end method

.method public static emptyMethod()V
   .registers 0
   return-void
.end method
