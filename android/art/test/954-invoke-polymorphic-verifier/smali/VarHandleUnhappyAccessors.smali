#
# Copyright (C) 2018 The Android Open Source Project
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

.source "VarHandleUnhappyAccessors.smali"

.class public LVarHandleUnhappyAccessors;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 4
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  invoke-static {}, LVarHandleUnhappyAccessors;->getVarHandle()Ljava/lang/invoke/VarHandle;
  move-result-object v0
  invoke-static {}, LVarHandleUnhappyAccessors;->getObjectArray()[Ljava/lang/Object;
  move-result-object v1
  invoke-static {v0, v1}, LVarHandleUnhappyAccessors;->compareAndExchange(Ljava/lang/invoke/VarHandle;[Ljava/lang/Object;)V
  invoke-static {v0}, LVarHandleUnhappyAccessors;->compareAndExchangeAcquire(Ljava/lang/invoke/VarHandle;)V
  invoke-static {v0}, LVarHandleUnhappyAccessors;->compareAndExchangeRelease(Ljava/lang/invoke/VarHandle;)V
  invoke-static {v0}, LVarHandleUnhappyAccessors;->compareAndSet(Ljava/lang/invoke/VarHandle;)V
  return-void
.end method

# The following methods all invoke VarHandle accessors but the targetted
# accessor methods have the wrong signature.

.method public static compareAndExchange(Ljava/lang/invoke/VarHandle;[Ljava/lang/Object;)V
.registers 2
  invoke-polymorphic {p0, p1}, Ljava/lang/invoke/VarHandle;->compareAndExchange([Ljava/lang/Object;)Ljava/lang/Integer;, ([Ljava/lang/Object;)Ljava/lang/Object;
.end method

.method public static compareAndExchangeAcquire(Ljava/lang/invoke/VarHandle;)V
.registers 2
  const/4 v0, 1
  invoke-polymorphic {p0, v0}, Ljava/lang/invoke/VarHandle;->compareAndExchangeAcquire(I)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
.end method

.method public static compareAndExchangeRelease(Ljava/lang/invoke/VarHandle;)V
.registers 1
  invoke-polymorphic {p0}, Ljava/lang/invoke/VarHandle;->compareAndExchangeRelease()V, ([Ljava/lang/Object;)Ljava/lang/Object;
.end method

.method public static compareAndSet(Ljava/lang/invoke/VarHandle;)V
.registers 2
  const/4 v0, 1
  invoke-polymorphic {p0, v0}, Ljava/lang/invoke/VarHandle;->compareAndSet(I)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
.end method

.method public static getVarHandle()Ljava/lang/invoke/VarHandle;
.registers 1
  const/4 v0, 0
  return-object v0
.end method

.method public static getObjectArray()[Ljava/lang/Object;
.registers 1
  const/4 v0, 0
  return-object v0
.end method
