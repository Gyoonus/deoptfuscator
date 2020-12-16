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

.source "Subclass.smali"

.class public LSubclass;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 3
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  goto :happy
  # Get a MethodHandleImpl instance (subclass of MethodHandle).
  invoke-static {}, LSubclass;->getMethodHandleSubclassInstance()Ljava/lang/invoke/MethodHandleImpl;
  move-result-object v0
  const-string v1, "1"
  const-string v2, "2"
  # Calling MethodHandle.invoke() on MethodHandleImpl instance (subclass of MethodHandle) => Okay
  invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
  # Calling MethodHandleImpl.invoke() rather than MethodHandle.invoke() [ declaring class is okay ] => Okay
  invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandleImpl;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
:happy
  sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
  const-string v2, "Passed Subclass test"
  invoke-virtual {v1, v2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
  return-void
.end method

.method public static getMethodHandleSubclassInstance()Ljava/lang/invoke/MethodHandleImpl;
.registers 1
  const/4 v0, 0
  return-object v0
.end method
