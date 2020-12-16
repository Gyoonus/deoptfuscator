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

.class public LRegisterAllocator;

.super Ljava/lang/Object;

# Test that catch phis are allocated to a stack slot, and that equivalent catch
# phis are allocated to the same stack slot.

## CHECK-START: int RegisterAllocator.testEquivalentCatchPhiSlot_Single(int, int, int) register (after)
## CHECK-DAG:     Phi reg:0 is_catch_phi:true locations:{{\[.*\]}}-><<SlotA1:\d+>>(sp)
## CHECK-DAG:     Phi reg:0 is_catch_phi:true locations:{{\[.*\]}}-><<SlotA2:\d+>>(sp)
## CHECK-DAG:     Phi reg:1 is_catch_phi:true locations:{{\[.*\]}}-><<SlotB:\d+>>(sp)
## CHECK-EVAL:    <<SlotA1>> == <<SlotA2>>
## CHECK-EVAL:    <<SlotB>> != <<SlotA1>>

.method public static testEquivalentCatchPhiSlot_Single(III)I
  .registers 8

  :try_start
  const/high16 v0, 0x40000000 # float 2
  move v1, p0
  div-int/2addr p0, p1

  const/high16 v0, 0x41000000 # float 8
  move v1, p1
  div-int/2addr p0, p2
  goto :return
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  :catch_all
  # 2x CatchPhi for v0, 1x for v1
  if-eqz v1, :use_as_float

  :use_as_int
  goto :return

  :use_as_float
  float-to-int v0, v0

  :return
  return v0
.end method

# Test that wide catch phis are allocated to two stack slots.

## CHECK-START: long RegisterAllocator.testEquivalentCatchPhiSlot_Wide(int, int, int) register (after)
## CHECK-DAG:     Phi reg:0 is_catch_phi:true locations:{{\[.*\]}}->2x<<SlotB1:\d+>>(sp)
## CHECK-DAG:     Phi reg:0 is_catch_phi:true locations:{{\[.*\]}}->2x<<SlotB2:\d+>>(sp)
## CHECK-DAG:     Phi reg:2 is_catch_phi:true locations:{{\[.*\]}}-><<SlotA:\d+>>(sp)
## CHECK-EVAL:    <<SlotB1>> == <<SlotB2>>
## CHECK-EVAL:    abs(<<SlotA>> - <<SlotB1>>) >= 8

.method public static testEquivalentCatchPhiSlot_Wide(III)J
  .registers 8

  :try_start
  const-wide/high16 v0, 0x4000000000000000L # double 2
  move v2, p0
  div-int/2addr p0, p1

  const-wide/high16 v0, 0x4100000000000000L # double 8
  move v2, p1
  div-int/2addr p0, p2
  goto :return
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  :catch_all
  # 2x CatchPhi for v0, 1x for v2
  if-eqz v2, :use_as_double

  :use_as_long
  goto :return

  :use_as_double
  double-to-long v0, v0

  :return
  return-wide v0
.end method
