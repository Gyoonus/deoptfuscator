/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.io.File;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

public class Main {
  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Assertion failed: " + expected + " != " + actual);
    }
  }

  private static void testRelativePositions(ByteBuffer b) throws Exception {
    // This goes into Memory.pokeByte(), which is an intrinsic that has
    // kWriteSideEffects. Stores before this call need to be kept.
    b.put((byte) 0);
    assertEquals(1, b.position());
  }

  private static ByteBuffer allocateMapped(int size) throws Exception {
    File f = File.createTempFile("mapped", "tmp");
    f.deleteOnExit();
    RandomAccessFile raf = new RandomAccessFile(f, "rw");
    raf.setLength(size);
    FileChannel ch = raf.getChannel();
    MappedByteBuffer result = ch.map(FileChannel.MapMode.READ_WRITE, 0, size);
    ch.close();
    return result;
  }

  public static void testRelativePositionsMapped() throws Exception {
    testRelativePositions(allocateMapped(10));
  }

  public static void main(String[] args) throws Exception {
    testRelativePositionsMapped();
  }
}
