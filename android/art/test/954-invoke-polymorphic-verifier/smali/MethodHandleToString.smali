#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.source "MethodHandleToString.smali"

.class public LMethodHandleToString;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  invoke-static {}, LMethodHandleToString;->getMethodHandle()Ljava/lang/invoke/MethodHandle;
  move-result-object v0
  # Attempt invoke-polymorphic on MethodHandle.toString().
  invoke-polymorphic {v0}, Ljava/lang/invoke/MethodHandle;->toString()Ljava/lang/String;, ()Ljava/lang/Object;
  return-void
.end method

.method public static getMethodHandle()Ljava/lang/invoke/MethodHandle;
.registers 1
  const/4 v0, 0
  return-object v0
.end method
