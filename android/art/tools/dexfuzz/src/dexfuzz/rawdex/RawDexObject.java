/*
 * Copyright (C) 2014 The Android Open Source Project
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

package dexfuzz.rawdex;

import java.io.IOException;

/**
 * Base class for any data structure that we may read or write from a DEX file.
 */
public interface RawDexObject {
  /**
   * Populate information for this DEX data from the file.
   * @param file Input file, should already be "seeked" to the correct position.
   * @throws IOException If there's a problem writing to the file.
   */
  public void read(DexRandomAccessFile file) throws IOException;

  /**
   * Write information for this DEX data to the file.
   * @param file Output file, should already be "seeked" to the correct position.
   * @throws IOException If there's a problem writing to the file.
   */
  public void write(DexRandomAccessFile file) throws IOException;

  public static enum IndexUpdateKind {
    STRING_ID,
    TYPE_ID,
    PROTO_ID,
    FIELD_ID,
    METHOD_ID
  }

  /**
   * When we insert a new string, type, proto, field or method into the DEX file,
   * this must be called. We may have inserted something into the middle of a table,
   * so any indices pointing afterwards must be updated.
   */
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx);
}
