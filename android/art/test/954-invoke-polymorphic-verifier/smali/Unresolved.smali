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

.source "Unresolved.smali"

.class public LUnresolved;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 3
.line 23
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  # Get an unresolvable instance (abstract class)
  invoke-static {}, LUnresolved;->getUnresolvedInstance()Lother/thing/Foo;
  move-result-object v0
  const-string v1, "1"
  const-string v2, "2"
  # Calling MethodHandle.invoke() on unresolved receiver.
  invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
  return-void
.end method

.method public static getUnresolvedInstance()Lother/thing/Foo;
.registers 1
.line 37
  const/4 v0, 0
  return-object v0
.end method
