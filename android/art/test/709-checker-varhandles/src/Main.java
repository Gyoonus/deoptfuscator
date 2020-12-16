/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.lang.invoke.VarHandle;

/**
 * Checker test on the 1.8 unsafe operations. Note, this is by no means an
 * exhaustive unit test for these CAS (compare-and-swap) and fence operations.
 * Instead, this test ensures the methods are recognized as intrinsic and behave
 * as expected.
 */
public class Main {

  //
  // Fences (native).
  //

  /// CHECK-START: void Main.fullFence() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:VarHandleFullFence
  //
  /// CHECK-START: void Main.fullFence() instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:VarHandleFullFence
  //
  /// CHECK-START: void Main.fullFence() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:AnyAny
  private static void fullFence() {
      VarHandle.fullFence();
  }

  /// CHECK-START: void Main.acquireFence() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:VarHandleAcquireFence
  //
  /// CHECK-START: void Main.acquireFence() instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:VarHandleAcquireFence
  //
  /// CHECK-START: void Main.acquireFence() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:LoadAny
  private static void acquireFence() {
      VarHandle.acquireFence();
  }

  /// CHECK-START: void Main.releaseFence() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:VarHandleReleaseFence
  //
  /// CHECK-START: void Main.releaseFence() instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:VarHandleReleaseFence
  //
  /// CHECK-START: void Main.releaseFence() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:AnyStore
  private static void releaseFence() {
      VarHandle.releaseFence();
  }

  /// CHECK-START: void Main.loadLoadFence() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:VarHandleLoadLoadFence
  //
  /// CHECK-START: void Main.loadLoadFence() instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:VarHandleLoadLoadFence
  //
  /// CHECK-START: void Main.loadLoadFence() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:LoadAny
  private static void loadLoadFence() {
      VarHandle.loadLoadFence();
  }

  /// CHECK-START: void Main.storeStoreFence() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:VarHandleStoreStoreFence
  //
  /// CHECK-START: void Main.storeStoreFence() instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:VarHandleStoreStoreFence
  //
  /// CHECK-START: void Main.storeStoreFence() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:StoreStore
  private static void storeStoreFence() {
      VarHandle.storeStoreFence();
  }

  //
  // Driver.
  //

  public static void main(String[] args) {
    System.out.println("starting");
    acquireFence();
    releaseFence();
    loadLoadFence();
    storeStoreFence();
    fullFence();
    System.out.println("passed");
  }
}
