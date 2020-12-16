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

public class MapItem implements RawDexObject {
  public static final int TYPE_HEADER_ITEM = 0x0;
  public static final int TYPE_STRING_ID_ITEM = 0x1;
  public static final int TYPE_TYPE_ID_ITEM = 0x2;
  public static final int TYPE_PROTO_ID_ITEM = 0x3;
  public static final int TYPE_FIELD_ID_ITEM = 0x4;
  public static final int TYPE_METHOD_ID_ITEM = 0x5;
  public static final int TYPE_CLASS_DEF_ITEM = 0x6;
  public static final int TYPE_MAP_LIST = 0x1000;
  public static final int TYPE_TYPE_LIST = 0x1001;
  public static final int TYPE_ANNOTATION_SET_REF_LIST = 0x1002;
  public static final int TYPE_ANNOTATION_SET_ITEM = 0x1003;
  public static final int TYPE_CLASS_DATA_ITEM = 0x2000;
  public static final int TYPE_CODE_ITEM = 0x2001;
  public static final int TYPE_STRING_DATA_ITEM = 0x2002;
  public static final int TYPE_DEBUG_INFO_ITEM = 0x2003;
  public static final int TYPE_ANNOTATION_ITEM = 0x2004;
  public static final int TYPE_ENCODED_ARRAY_ITEM = 0x2005;
  public static final int TYPE_ANNOTATIONS_DIRECTORY_ITEM = 0x2006;

  public short type;
  public int size;
  public Offset offset;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    type = file.readUShort();
    file.readUShort(); // Unused padding.
    size = file.readUInt();
    if (type == TYPE_HEADER_ITEM) {
      offset = file.getOffsetTracker().getNewHeaderOffset(file.readUInt());
    } else {
      offset = file.getOffsetTracker().getNewOffset(file.readUInt());
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeUShort(type);
    file.writeUShort((short) 0); // Unused padding.
    file.writeUInt(size);
    file.getOffsetTracker().tryToWriteOffset(offset, file, false /* ULEB128 */);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }
}
