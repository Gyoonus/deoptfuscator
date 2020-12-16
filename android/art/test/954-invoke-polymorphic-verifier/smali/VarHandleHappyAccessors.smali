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

.source "VarHandleHappyAccessors.smali"

.class public LVarHandleHappyAccessors;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 4
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  invoke-static {}, LVarHandleHappyAccessors;->getVarHandle()Ljava/lang/invoke/VarHandle;
  move-result-object v0
  if-eqz v0, :done
  const/4 v1, 0
  move-object v1, v1
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->compareAndExchange([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->compareAndExchangeAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->compareAndExchangeRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->compareAndSet([Ljava/lang/Object;)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->get([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndAdd([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndAddAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndAddRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseAnd([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseAndAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseAndRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseOr([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseOrAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseOrRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseXor([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseXorAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndBitwiseXorRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndSet([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndSetAcquire([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getAndSetRelease([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getOpaque([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->getVolatile([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->set([Ljava/lang/Object;)V, ([Ljava/lang/Object;)V
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->setOpaque([Ljava/lang/Object;)V, ([Ljava/lang/Object;)V
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->setRelease([Ljava/lang/Object;)V, ([Ljava/lang/Object;)V
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->setVolatile([Ljava/lang/Object;)V, ([Ljava/lang/Object;)V
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->weakCompareAndSet([Ljava/lang/Object;)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->weakCompareAndSetAcquire([Ljava/lang/Object;)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->weakCompareAndSetPlain([Ljava/lang/Object;)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
  invoke-polymorphic {v0, v1}, Ljava/lang/invoke/VarHandle;->weakCompareAndSetRelease([Ljava/lang/Object;)Z, ([Ljava/lang/Object;)Ljava/lang/Object;
  return-void
  :done
  sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
  const-string v1, "Passed VarHandleHappyAccessors test"
  invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
  return-void
.end method

.method public static getVarHandle()Ljava/lang/invoke/VarHandle;
.registers 1
  const/4 v0, 0
  return-object v0
.end method
