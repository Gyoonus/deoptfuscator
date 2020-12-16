/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.ahat.heapdump;

import com.android.ahat.proguard.ProguardMap;
import java.io.File;
import java.io.IOException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Provides methods for parsing heap dumps.
 */
public class Parser {
  private static final int ID_SIZE = 4;

  private Parser() {
  }

  /**
   * Parses a heap dump from a File.
   * <p>
   * The heap dump should be a heap dump in the J2SE HPROF format optionally
   * with Android extensions and satisfying the following additional
   * constraints:
   * <ul>
   * <li>
   * Class serial numbers, stack frames, and stack traces individually satisfy
   * the following:
   * <ul>
   *   <li> All elements are defined before they are referenced.
   *   <li> Ids are densely packed in some range [a, b] where a is not necessarily 0.
   *   <li> There are not more than 2^31 elements defined.
   * </ul>
   * <li> All classes are defined via a LOAD CLASS record before the first
   * heap dump segment.
   * <li> The ID size used in the heap dump is 4 bytes.
   * </ul>
   * <p>
   * The given proguard map will be used to deobfuscate class names, field
   * names, and stack traces in the heap dump.
   *
   * @param hprof the hprof file to parse
   * @param map the proguard map for deobfuscation
   * @return the parsed heap dump
   * @throws IOException if the heap dump could not be read
   * @throws HprofFormatException if the heap dump is not properly formatted
   */
  public static AhatSnapshot parseHeapDump(File hprof, ProguardMap map)
    throws IOException, HprofFormatException {
    try {
      return parseHeapDump(new HprofBuffer(hprof), map);
    } catch (BufferUnderflowException e) {
      throw new HprofFormatException("Unexpected end of file", e);
    }
  }

  /**
   * Parses a heap dump from a byte buffer.
   * <p>
   * The heap dump should be a heap dump in the J2SE HPROF format optionally
   * with Android extensions and satisfying the following additional
   * constraints:
   * <ul>
   * <li>
   * Class serial numbers, stack frames, and stack traces individually satisfy
   * the following:
   * <ul>
   *   <li> All elements are defined before they are referenced.
   *   <li> Ids are densely packed in some range [a, b] where a is not necessarily 0.
   *   <li> There are not more than 2^31 elements defined.
   * </ul>
   * <li> All classes are defined via a LOAD CLASS record before the first
   * heap dump segment.
   * <li> The ID size used in the heap dump is 4 bytes.
   * </ul>
   * <p>
   * The given proguard map will be used to deobfuscate class names, field
   * names, and stack traces in the heap dump.
   *
   * @param hprof the bytes of the hprof file to parse
   * @param map the proguard map for deobfuscation
   * @return the parsed heap dump
   * @throws IOException if the heap dump could not be read
   * @throws HprofFormatException if the heap dump is not properly formatted
   */
  public static AhatSnapshot parseHeapDump(ByteBuffer hprof, ProguardMap map)
    throws IOException, HprofFormatException {
    try {
      return parseHeapDump(new HprofBuffer(hprof), map);
    } catch (BufferUnderflowException e) {
      throw new HprofFormatException("Unexpected end of file", e);
    }
  }

  private static AhatSnapshot parseHeapDump(HprofBuffer hprof, ProguardMap map)
    throws IOException, HprofFormatException, BufferUnderflowException {
    // Read, and mostly ignore, the hprof header info.
    {
      StringBuilder format = new StringBuilder();
      int b;
      while ((b = hprof.getU1()) != 0) {
        format.append((char)b);
      }

      int idSize = hprof.getU4();
      if (idSize != ID_SIZE) {
        throw new HprofFormatException("Id size " + idSize + " not supported.");
      }
      int hightime = hprof.getU4();
      int lowtime = hprof.getU4();
    }

    // First pass: Read through all the heap dump records. Construct the
    // AhatInstances, initialize them as much as possible and save any
    // additional temporary data we need to complete their initialization in
    // the fixup pass.
    Site rootSite = new Site("ROOT");
    List<AhatInstance> instances = new ArrayList<AhatInstance>();
    List<RootData> roots = new ArrayList<RootData>();
    HeapList heaps = new HeapList();
    {
      // Note: Strings do not satisfy the DenseMap requirements on heap dumps
      // from Android K.
      UnDenseMap<String> strings = new UnDenseMap<String>("String");
      DenseMap<ProguardMap.Frame> frames = new DenseMap<ProguardMap.Frame>("Stack Frame");
      DenseMap<Site> sites = new DenseMap<Site>("Stack Trace");
      DenseMap<String> classNamesBySerial = new DenseMap<String>("Class Serial Number");
      AhatClassObj javaLangClass = null;
      AhatClassObj[] primArrayClasses = new AhatClassObj[Type.values().length];
      ArrayList<AhatClassObj> classes = new ArrayList<AhatClassObj>();
      Instances<AhatClassObj> classById = null;

      while (hprof.hasRemaining()) {
        int tag = hprof.getU1();
        int time = hprof.getU4();
        int recordLength = hprof.getU4();
        switch (tag) {
          case 0x01: { // STRING
            long id = hprof.getId();
            byte[] bytes = new byte[recordLength - ID_SIZE];
            hprof.getBytes(bytes);
            String str = new String(bytes, StandardCharsets.UTF_8);
            strings.put(id, str);
            break;
          }

          case 0x02: { // LOAD CLASS
            int classSerialNumber = hprof.getU4();
            long objectId = hprof.getId();
            int stackSerialNumber = hprof.getU4();
            long classNameStringId = hprof.getId();
            String obfClassName = strings.get(classNameStringId);
            String clrClassName = map.getClassName(obfClassName);
            AhatClassObj classObj = new AhatClassObj(objectId, clrClassName);
            classNamesBySerial.put(classSerialNumber, clrClassName);
            classes.add(classObj);

            // Check whether this class is one of the special classes we are
            // interested in, and if so, save it for later use.
            if ("java.lang.Class".equals(clrClassName)) {
              javaLangClass = classObj;
            }

            for (Type type : Type.values()) {
              if (clrClassName.equals(type.name + "[]")) {
                primArrayClasses[type.ordinal()] = classObj;
              }
            }
            break;
          }

          case 0x04: { // STACK FRAME
            long frameId = hprof.getId();
            long methodNameStringId = hprof.getId();
            long methodSignatureStringId = hprof.getId();
            long methodFileNameStringId = hprof.getId();
            int classSerialNumber = hprof.getU4();
            int lineNumber = hprof.getU4();

            ProguardMap.Frame frame = map.getFrame(
                classNamesBySerial.get(classSerialNumber),
                strings.get(methodNameStringId),
                strings.get(methodSignatureStringId),
                strings.get(methodFileNameStringId),
                lineNumber);
            frames.put(frameId, frame);
            break;
          }

          case 0x05: { // STACK TRACE
            int stackSerialNumber = hprof.getU4();
            int threadSerialNumber = hprof.getU4();
            int numFrames = hprof.getU4();
            ProguardMap.Frame[] trace = new ProguardMap.Frame[numFrames];
            for (int i = 0; i < numFrames; i++) {
              long frameId = hprof.getId();
              trace[i] = frames.get(frameId);
            }
            sites.put(stackSerialNumber, rootSite.getSite(trace));
            break;
          }

          case 0x1C: { // HEAP DUMP SEGMENT
            if (classById == null) {
              classById = new Instances<AhatClassObj>(classes);
            }
            int subtag;
            while (!isEndOfHeapDumpSegment(subtag = hprof.getU1())) {
              switch (subtag) {
                case 0x01: { // ROOT JNI GLOBAL
                  long objectId = hprof.getId();
                  long refId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.JNI_GLOBAL));
                  break;
                }

                case 0x02: { // ROOT JNI LOCAL
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  int frameNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.JNI_LOCAL));
                  break;
                }

                case 0x03: { // ROOT JAVA FRAME
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  int frameNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.JAVA_FRAME));
                  break;
                }

                case 0x04: { // ROOT NATIVE STACK
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.NATIVE_STACK));
                  break;
                }

                case 0x05: { // ROOT STICKY CLASS
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.STICKY_CLASS));
                  break;
                }

                case 0x06: { // ROOT THREAD BLOCK
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.THREAD_BLOCK));
                  break;
                }

                case 0x07: { // ROOT MONITOR USED
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.MONITOR));
                  break;
                }

                case 0x08: { // ROOT THREAD OBJECT
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  int stackSerialNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.THREAD));
                  break;
                }

                case 0x20: { // CLASS DUMP
                  ClassObjData data = new ClassObjData();
                  long objectId = hprof.getId();
                  int stackSerialNumber = hprof.getU4();
                  long superClassId = hprof.getId();
                  data.classLoaderId = hprof.getId();
                  long signersId = hprof.getId();
                  long protectionId = hprof.getId();
                  long reserved1 = hprof.getId();
                  long reserved2 = hprof.getId();
                  int instanceSize = hprof.getU4();
                  int constantPoolSize = hprof.getU2();
                  for (int i = 0; i < constantPoolSize; ++i) {
                    int index = hprof.getU2();
                    Type type = hprof.getType();
                    hprof.skip(type.size);
                  }
                  int numStaticFields = hprof.getU2();
                  data.staticFields = new FieldValue[numStaticFields];
                  AhatClassObj obj = classById.get(objectId);
                  String clrClassName = obj.getName();
                  long staticFieldsSize = 0;
                  for (int i = 0; i < numStaticFields; ++i) {
                    String obfName = strings.get(hprof.getId());
                    String clrName = map.getFieldName(clrClassName, obfName);
                    Type type = hprof.getType();
                    Value value = hprof.getDeferredValue(type);
                    staticFieldsSize += type.size;
                    data.staticFields[i] = new FieldValue(clrName, type, value);
                  }
                  AhatClassObj superClass = classById.get(superClassId);
                  int numInstanceFields = hprof.getU2();
                  Field[] ifields = new Field[numInstanceFields];
                  for (int i = 0; i < numInstanceFields; ++i) {
                    String name = map.getFieldName(obj.getName(), strings.get(hprof.getId()));
                    ifields[i] = new Field(name, hprof.getType());
                  }
                  Site site = sites.get(stackSerialNumber);

                  if (javaLangClass == null) {
                    throw new HprofFormatException("No class definition found for java.lang.Class");
                  }
                  obj.initialize(heaps.getCurrentHeap(), site, javaLangClass);
                  obj.initialize(superClass, instanceSize, ifields, staticFieldsSize);
                  obj.setTemporaryUserData(data);
                  break;
                }

                case 0x21: { // INSTANCE DUMP
                  long objectId = hprof.getId();
                  int stackSerialNumber = hprof.getU4();
                  long classId = hprof.getId();
                  int numBytes = hprof.getU4();
                  ClassInstData data = new ClassInstData(hprof.tell());
                  hprof.skip(numBytes);

                  Site site = sites.get(stackSerialNumber);
                  AhatClassObj classObj = classById.get(classId);
                  AhatClassInstance obj = new AhatClassInstance(objectId);
                  obj.initialize(heaps.getCurrentHeap(), site, classObj);
                  obj.setTemporaryUserData(data);
                  instances.add(obj);
                  break;
                }

                case 0x22: { // OBJECT ARRAY DUMP
                  long objectId = hprof.getId();
                  int stackSerialNumber = hprof.getU4();
                  int length = hprof.getU4();
                  long classId = hprof.getId();
                  ObjArrayData data = new ObjArrayData(length, hprof.tell());
                  hprof.skip(length * ID_SIZE);

                  Site site = sites.get(stackSerialNumber);
                  AhatClassObj classObj = classById.get(classId);
                  AhatArrayInstance obj = new AhatArrayInstance(objectId);
                  obj.initialize(heaps.getCurrentHeap(), site, classObj);
                  obj.setTemporaryUserData(data);
                  instances.add(obj);
                  break;
                }

                case 0x23: { // PRIMITIVE ARRAY DUMP
                  long objectId = hprof.getId();
                  int stackSerialNumber = hprof.getU4();
                  int length = hprof.getU4();
                  Type type = hprof.getPrimitiveType();
                  Site site = sites.get(stackSerialNumber);

                  AhatClassObj classObj = primArrayClasses[type.ordinal()];
                  if (classObj == null) {
                    throw new HprofFormatException(
                        "No class definition found for " + type.name + "[]");
                  }

                  AhatArrayInstance obj = new AhatArrayInstance(objectId);
                  obj.initialize(heaps.getCurrentHeap(), site, classObj);
                  instances.add(obj);
                  switch (type) {
                    case BOOLEAN: {
                      boolean[] data = new boolean[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getBool();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case CHAR: {
                      char[] data = new char[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getChar();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case FLOAT: {
                      float[] data = new float[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getFloat();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case DOUBLE: {
                      double[] data = new double[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getDouble();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case BYTE: {
                      byte[] data = new byte[length];
                      hprof.getBytes(data);
                      obj.initialize(data);
                      break;
                    }

                    case SHORT: {
                      short[] data = new short[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getShort();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case INT: {
                      int[] data = new int[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getInt();
                      }
                      obj.initialize(data);
                      break;
                    }

                    case LONG: {
                      long[] data = new long[length];
                      for (int i = 0; i < length; ++i) {
                        data[i] = hprof.getLong();
                      }
                      obj.initialize(data);
                      break;
                    }
                  }
                  break;
                }

                case 0x89: { // ROOT INTERNED STRING (ANDROID)
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.INTERNED_STRING));
                  break;
                }

                case 0x8a: { // ROOT FINALIZING (ANDROID)
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.FINALIZING));
                  break;
                }

                case 0x8b: { // ROOT DEBUGGER (ANDROID)
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.DEBUGGER));
                  break;
                }

                case 0x8d: { // ROOT VM INTERNAL (ANDROID)
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.VM_INTERNAL));
                  break;
                }

                case 0x8e: { // ROOT JNI MONITOR (ANDROID)
                  long objectId = hprof.getId();
                  int threadSerialNumber = hprof.getU4();
                  int frameNumber = hprof.getU4();
                  roots.add(new RootData(objectId, RootType.JNI_MONITOR));
                  break;
                }

                case 0xfe: { // HEAP DUMP INFO (ANDROID)
                  int type = hprof.getU4();
                  long stringId = hprof.getId();
                  heaps.setCurrentHeap(strings.get(stringId));
                  break;
                }

                case 0xff: { // ROOT UNKNOWN
                  long objectId = hprof.getId();
                  roots.add(new RootData(objectId, RootType.UNKNOWN));
                  break;
                }

                default:
                  throw new HprofFormatException(
                      String.format("Unsupported heap dump sub tag 0x%02x", subtag));
              }
            }

            // Reset the file pointer back because we read the first byte into
            // the next record.
            hprof.skip(-1);
            break;
          }

          default:
            // Ignore any other tags that we either don't know about or don't
            // care about.
            hprof.skip(recordLength);
            break;
        }
      }

      instances.addAll(classes);
    }

    // Sort roots and instances by id in preparation for the fixup pass.
    Instances<AhatInstance> mInstances = new Instances<AhatInstance>(instances);
    roots.sort(new Comparator<RootData>() {
      @Override
      public int compare(RootData a, RootData b) {
        return Long.compare(a.id, b.id);
      }
    });
    roots.add(null);

    // Fixup pass: Label the root instances and fix up references to instances
    // that we couldn't previously resolve.
    SuperRoot superRoot = new SuperRoot();
    {
      Iterator<RootData> ri = roots.iterator();
      RootData root = ri.next();
      for (AhatInstance inst : mInstances) {
        long id = inst.getId();

        // Skip past any roots that don't have associated instances.
        // It's not clear why there would be a root without an associated
        // instance dump, but it does happen in practice, for example when
        // taking heap dumps using the RI.
        while (root != null && root.id < id) {
          root = ri.next();
        }

        // Check if this instance is a root, and if so, update its root types.
        if (root != null && root.id == id) {
          superRoot.addRoot(inst);
          while (root != null && root.id == id) {
            inst.addRootType(root.type);
            root = ri.next();
          }
        }

        // Fixup the instance based on its type using the temporary data we
        // saved during the first pass over the heap dump.
        if (inst instanceof AhatClassInstance) {
          ClassInstData data = (ClassInstData)inst.getTemporaryUserData();
          inst.setTemporaryUserData(null);

          // Compute the size of the fields array in advance to avoid
          // extra allocations and copies that would come from using an array
          // list to collect the field values.
          int numFields = 0;
          for (AhatClassObj cls = inst.getClassObj(); cls != null; cls = cls.getSuperClassObj()) {
            numFields += cls.getInstanceFields().length;
          }

          Value[] fields = new Value[numFields];
          int i = 0;
          hprof.seek(data.position);
          for (AhatClassObj cls = inst.getClassObj(); cls != null; cls = cls.getSuperClassObj()) {
            for (Field field : cls.getInstanceFields()) {
              fields[i++] = hprof.getValue(field.type, mInstances);
            }
          }
          ((AhatClassInstance)inst).initialize(fields);
        } else if (inst instanceof AhatClassObj) {
          ClassObjData data = (ClassObjData)inst.getTemporaryUserData();
          inst.setTemporaryUserData(null);
          AhatInstance loader = mInstances.get(data.classLoaderId);
          for (int i = 0; i < data.staticFields.length; ++i) {
            FieldValue field = data.staticFields[i];
            if (field.value instanceof DeferredInstanceValue) {
              DeferredInstanceValue deferred = (DeferredInstanceValue)field.value;
              data.staticFields[i] = new FieldValue(
                  field.name, field.type, Value.pack(mInstances.get(deferred.getId())));
            }
          }
          ((AhatClassObj)inst).initialize(loader, data.staticFields);
        } else if (inst instanceof AhatArrayInstance && inst.getTemporaryUserData() != null) {
          // TODO: Have specialized object array instance and check for that
          // rather than checking for the presence of user data?
          ObjArrayData data = (ObjArrayData)inst.getTemporaryUserData();
          inst.setTemporaryUserData(null);
          AhatInstance[] array = new AhatInstance[data.length];
          hprof.seek(data.position);
          for (int i = 0; i < data.length; i++) {
            array[i] = mInstances.get(hprof.getId());
          }
          ((AhatArrayInstance)inst).initialize(array);
        }
      }
    }

    hprof = null;
    roots = null;
    return new AhatSnapshot(superRoot, mInstances, heaps.heaps, rootSite);
  }

  private static boolean isEndOfHeapDumpSegment(int subtag) {
    return subtag == 0x1C || subtag == 0x2C;
  }

  private static class RootData {
    public long id;
    public RootType type;

    public RootData(long id, RootType type) {
      this.id = id;
      this.type = type;
    }
  }

  private static class ClassInstData {
    // The byte position in the hprof file where instance field data starts.
    public int position;

    public ClassInstData(int position) {
      this.position = position;
    }
  }

  private static class ObjArrayData {
    public int length;          // Number of array elements.
    public int position;        // Position in hprof file containing element data.

    public ObjArrayData(int length, int position) {
      this.length = length;
      this.position = position;
    }
  }

  private static class ClassObjData {
    public long classLoaderId;
    public FieldValue[] staticFields; // Contains DeferredInstanceValues.
  }

  /**
   * Dummy value representing a reference to an instance that has not yet been
   * resolved.
   * When first initializing class static fields, we don't yet know what kinds
   * of objects Object references refer to. We use DeferredInstanceValue as
   * a dummy kind of value to store the id of an object. In the fixup pass we
   * resolve all the DeferredInstanceValues into their proper InstanceValues.
   */
  private static class DeferredInstanceValue extends Value {
    private long mId;

    public DeferredInstanceValue(long id) {
      mId = id;
    }

    public long getId() {
      return mId;
    }

    @Override
    Type getType() {
      return Type.OBJECT;
    }

    @Override
    public String toString() {
      return String.format("0x%08x", mId);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof DeferredInstanceValue) {
        DeferredInstanceValue value = (DeferredInstanceValue)other;
        return mId == value.mId;
      }
      return false;
    }
  }

  /**
   * A convenient abstraction for lazily building up the list of heaps seen in
   * the heap dump.
   */
  private static class HeapList {
    public List<AhatHeap> heaps = new ArrayList<AhatHeap>();
    private AhatHeap current;

    public AhatHeap getCurrentHeap() {
      if (current == null) {
        setCurrentHeap("default");
      }
      return current;
    }

    public void setCurrentHeap(String name) {
      for (AhatHeap heap : heaps) {
        if (name.equals(heap.getName())) {
          current = heap;
          return;
        }
      }

      current = new AhatHeap(name, heaps.size());
      heaps.add(current);
    }
  }

  /**
   * A mapping from id to elements, where certain conditions are
   * satisfied. The conditions are:
   *  - all elements are defined before they are referenced.
   *  - ids are densely packed in some range [a, b] where a is not
   *    necessarily 0.
   *  - there are not more than 2^31 elements defined.
   */
  private static class DenseMap<T> {
    private String mElementType;

    // mValues behaves like a circular buffer.
    // mKeyAt0 is the key corresponding to index 0 of mValues. Values with
    // smaller keys will wrap around to the end of the mValues buffer. The
    // buffer is expanded when it is no longer big enough to hold all the keys
    // from mMinKey to mMaxKey.
    private Object[] mValues;
    private long mKeyAt0;
    private long mMaxKey;
    private long mMinKey;

    /**
     * Constructs a DenseMap.
     * @param elementType Human readable name describing the type of
     *                    elements for error message if the required
     *                    conditions are found not to hold.
     */
    public DenseMap(String elementType) {
      mElementType = elementType;
    }

    public void put(long key, T value) {
      if (mValues == null) {
        mValues = new Object[8];
        mValues[0] = value;
        mKeyAt0 = key;
        mMaxKey = key;
        mMinKey = key;
        return;
      }

      long max = Math.max(mMaxKey, key);
      long min = Math.min(mMinKey, key);
      int count = (int)(max + 1 - min);
      if (count > mValues.length) {
        Object[] values = new Object[2 * count];

        // Copy over the values into the newly allocated larger buffer. It is
        // convenient to move the value with mMinKey to index 0 when we make
        // the copy.
        for (int i = 0; i < mValues.length; ++i) {
          values[i] = mValues[indexOf(i + mMinKey)];
        }
        mValues = values;
        mKeyAt0 = mMinKey;
      }
      mMinKey = min;
      mMaxKey = max;
      mValues[indexOf(key)] = value;
    }

    /**
     * Returns the value for the given key.
     * @throws HprofFormatException if there is no value with the key in the
     *         given map.
     */
    public T get(long key) throws HprofFormatException {
      T value = null;
      if (mValues != null && key >= mMinKey && key <= mMaxKey) {
        value = (T)mValues[indexOf(key)];
      }

      if (value == null) {
        throw new HprofFormatException(String.format(
              "%s with id 0x%x referenced before definition", mElementType, key));
      }
      return value;
    }

    private int indexOf(long key) {
      return ((int)(key - mKeyAt0) + mValues.length) % mValues.length;
    }
  }

  /**
   * A mapping from id to elements, where we don't have nice conditions to
   * work with.
   */
  private static class UnDenseMap<T> {
    private String mElementType;
    private Map<Long, T> mValues = new HashMap<Long, T>();

    /**
     * Constructs an UnDenseMap.
     * @param elementType Human readable name describing the type of
     *                    elements for error message if the required
     *                    conditions are found not to hold.
     */
    public UnDenseMap(String elementType) {
      mElementType = elementType;
    }

    public void put(long key, T value) {
      mValues.put(key, value);
    }

    /**
     * Returns the value for the given key.
     * @throws HprofFormatException if there is no value with the key in the
     *         given map.
     */
    public T get(long key) throws HprofFormatException {
      T value = mValues.get(key);
      if (value == null) {
        throw new HprofFormatException(String.format(
              "%s with id 0x%x referenced before definition", mElementType, key));
      }
      return value;
    }
  }

  /**
   * Wrapper around a ByteBuffer that presents a uniform interface for
   * accessing data from an hprof file.
   */
  private static class HprofBuffer {
    private ByteBuffer mBuffer;

    public HprofBuffer(File path) throws IOException {
      FileChannel channel = FileChannel.open(path.toPath(), StandardOpenOption.READ);
      mBuffer = channel.map(FileChannel.MapMode.READ_ONLY, 0, channel.size());
      channel.close();
    }

    public HprofBuffer(ByteBuffer buffer) {
      mBuffer = buffer;
    }

    public boolean hasRemaining() {
      return mBuffer.hasRemaining();
    }

    /**
     * Return the current absolution position in the file.
     */
    public int tell() {
      return mBuffer.position();
    }

    /**
     * Seek to the given absolution position in the file.
     */
    public void seek(int position) {
      mBuffer.position(position);
    }

    /**
     * Skip ahead in the file by the given delta bytes. Delta may be negative
     * to skip backwards in the file.
     */
    public void skip(int delta) {
      seek(tell() + delta);
    }

    public int getU1() {
      return mBuffer.get() & 0xFF;
    }

    public int getU2() {
      return mBuffer.getShort() & 0xFFFF;
    }

    public int getU4() {
      return mBuffer.getInt();
    }

    public long getId() {
      return mBuffer.getInt() & 0xFFFFFFFFL;
    }

    public boolean getBool() {
      return mBuffer.get() != 0;
    }

    public char getChar() {
      return mBuffer.getChar();
    }

    public float getFloat() {
      return mBuffer.getFloat();
    }

    public double getDouble() {
      return mBuffer.getDouble();
    }

    public byte getByte() {
      return mBuffer.get();
    }

    public void getBytes(byte[] bytes) {
      mBuffer.get(bytes);
    }

    public short getShort() {
      return mBuffer.getShort();
    }

    public int getInt() {
      return mBuffer.getInt();
    }

    public long getLong() {
      return mBuffer.getLong();
    }

    private static Type[] TYPES = new Type[] {
      null, null, Type.OBJECT, null,
        Type.BOOLEAN, Type.CHAR, Type.FLOAT, Type.DOUBLE,
        Type.BYTE, Type.SHORT, Type.INT, Type.LONG
    };

    public Type getType() throws HprofFormatException {
      int id = getU1();
      Type type = id < TYPES.length ? TYPES[id] : null;
      if (type == null) {
        throw new HprofFormatException("Invalid basic type id: " + id);
      }
      return type;
    }

    public Type getPrimitiveType() throws HprofFormatException {
      Type type = getType();
      if (type == Type.OBJECT) {
        throw new HprofFormatException("Expected primitive type, but found type 'Object'");
      }
      return type;
    }

    /**
     * Get a value from the hprof file, using the given instances map to
     * convert instance ids to their corresponding AhatInstance objects.
     */
    public Value getValue(Type type, Instances instances) {
      switch (type) {
        case OBJECT:  return Value.pack(instances.get(getId()));
        case BOOLEAN: return Value.pack(getBool());
        case CHAR: return Value.pack(getChar());
        case FLOAT: return Value.pack(getFloat());
        case DOUBLE: return Value.pack(getDouble());
        case BYTE: return Value.pack(getByte());
        case SHORT: return Value.pack(getShort());
        case INT: return Value.pack(getInt());
        case LONG: return Value.pack(getLong());
        default: throw new AssertionError("unsupported enum member");
      }
    }

    /**
     * Get a value from the hprof file. AhatInstance values are returned as
     * DefferredInstanceValues rather than their corresponding AhatInstance
     * objects.
     */
    public Value getDeferredValue(Type type) {
      switch (type) {
        case OBJECT: return new DeferredInstanceValue(getId());
        case BOOLEAN: return Value.pack(getBool());
        case CHAR: return Value.pack(getChar());
        case FLOAT: return Value.pack(getFloat());
        case DOUBLE: return Value.pack(getDouble());
        case BYTE: return Value.pack(getByte());
        case SHORT: return Value.pack(getShort());
        case INT: return Value.pack(getInt());
        case LONG: return Value.pack(getLong());
        default: throw new AssertionError("unsupported enum member");
      }
    }
  }
}
