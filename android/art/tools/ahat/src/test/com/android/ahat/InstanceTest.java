/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.ahat;

import com.android.ahat.heapdump.AhatClassObj;
import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.PathElement;
import com.android.ahat.heapdump.Size;
import com.android.ahat.heapdump.Value;
import java.io.IOException;
import java.util.List;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class InstanceTest {
  @Test
  public void asStringBasic() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString());
  }

  @Test
  public void asStringNonAscii() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("nonAscii");
    assertEquals("Sigma (Ʃ) is not ASCII", str.asString());
  }

  @Test
  public void asStringEmbeddedZero() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("embeddedZero");
    assertEquals("embedded\0...", str.asString());
  }

  @Test
  public void asStringCharArray() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString());
  }

  @Test
  public void asStringTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello", str.asString(5));
  }

  @Test
  public void asStringTruncatedNonAscii() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("nonAscii");
    assertEquals("Sigma (Ʃ)", str.asString(9));
  }

  @Test
  public void asStringTruncatedEmbeddedZero() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("embeddedZero");
    assertEquals("embed", str.asString(5));
  }

  @Test
  public void asStringCharArrayTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char ", str.asString(5));
  }

  @Test
  public void asStringExactMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(12));
  }

  @Test
  public void asStringExactMaxNonAscii() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("nonAscii");
    assertEquals("Sigma (Ʃ) is not ASCII", str.asString(22));
  }

  @Test
  public void asStringExactMaxEmbeddedZero() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("embeddedZero");
    assertEquals("embedded\0...", str.asString(12));
  }

  @Test
  public void asStringCharArrayExactMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(10));
  }

  @Test
  public void asStringNotTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(50));
  }

  @Test
  public void asStringNotTruncatedNonAscii() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("nonAscii");
    assertEquals("Sigma (Ʃ) is not ASCII", str.asString(50));
  }

  @Test
  public void asStringNotTruncatedEmbeddedZero() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("embeddedZero");
    assertEquals("embedded\0...", str.asString(50));
  }

  @Test
  public void asStringCharArrayNotTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(50));
  }

  @Test
  public void asStringNegativeMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(-3));
  }

  @Test
  public void asStringNegativeMaxNonAscii() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("nonAscii");
    assertEquals("Sigma (Ʃ) is not ASCII", str.asString(-3));
  }

  @Test
  public void asStringNegativeMaxEmbeddedZero() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("embeddedZero");
    assertEquals("embedded\0...", str.asString(-3));
  }

  @Test
  public void asStringCharArrayNegativeMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(-3));
  }

  @Test
  public void asStringNull() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("nullString");
    assertNull(obj);
  }

  @Test
  public void asStringNotString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertNotNull(obj);
    assertNull(obj.asString());
  }

  @Test
  public void basicReference() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance pref = dump.getDumpedAhatInstance("aPhantomReference");
    AhatInstance wref = dump.getDumpedAhatInstance("aWeakReference");
    AhatInstance nref = dump.getDumpedAhatInstance("aNullReferentReference");
    AhatInstance referent = dump.getDumpedAhatInstance("anObject");
    assertNotNull(pref);
    assertNotNull(wref);
    assertNotNull(nref);
    assertNotNull(referent);
    assertEquals(referent, pref.getReferent());
    assertEquals(referent, wref.getReferent());
    assertNull(nref.getReferent());
    assertNull(referent.getReferent());
  }

  @Test
  public void unreachableReferent() throws IOException {
    // The test dump program should never be under enough GC pressure for the
    // soft reference to be cleared. Ensure that ahat will show the soft
    // reference as having a non-null referent.
    TestDump dump = TestDump.getTestDump();
    AhatInstance ref = dump.getDumpedAhatInstance("aSoftReference");
    AhatInstance referent = ref.getReferent();
    assertNotNull(referent);
    assertTrue(referent.isWeaklyReachable());
  }

  @Test
  public void gcRootPath() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatClassObj main = dump.findClass("Main");
    AhatInstance gcPathArray = dump.getDumpedAhatInstance("gcPathArray");
    Value value = gcPathArray.asArrayInstance().getValue(2);
    AhatInstance base = value.asAhatInstance();
    AhatInstance left = base.getRefField("left");
    AhatInstance right = base.getRefField("right");
    AhatInstance target = left.getRefField("right");

    List<PathElement> path = target.getPathFromGcRoot();
    assertEquals(6, path.size());

    assertEquals(main, path.get(0).instance);
    assertEquals(".stuff", path.get(0).field);
    assertTrue(path.get(0).isDominator);

    assertEquals(".gcPathArray", path.get(1).field);
    assertTrue(path.get(1).isDominator);

    assertEquals(gcPathArray, path.get(2).instance);
    assertEquals("[2]", path.get(2).field);
    assertTrue(path.get(2).isDominator);

    assertEquals(base, path.get(3).instance);
    assertTrue(path.get(3).isDominator);

    // There are two possible paths. Either it can go through the 'left' node,
    // or the 'right' node.
    if (path.get(3).field.equals(".left")) {
      assertEquals(".left", path.get(3).field);

      assertEquals(left, path.get(4).instance);
      assertEquals(".right", path.get(4).field);
      assertFalse(path.get(4).isDominator);

    } else {
      assertEquals(".right", path.get(3).field);

      assertEquals(right, path.get(4).instance);
      assertEquals(".left", path.get(4).field);
      assertFalse(path.get(4).isDominator);
    }

    assertEquals(target, path.get(5).instance);
    assertEquals("", path.get(5).field);
    assertTrue(path.get(5).isDominator);
  }

  @Test
  public void gcRootPathNotWeak() throws IOException {
    TestDump dump = TestDump.getTestDump();

    // The test dump is set up to have the following graph:
    //  -S-> strong1 -S-> strong2 -S-> strong3 -S-> object
    //  -S-> weak1 -W-> weak2 ------------------S->-/
    // The gc root path should go through the longer chain of strong
    // references, not the shorter chain with weak references (even though the
    // last element in the shorter chain is a strong reference).

    AhatInstance strong1 = dump.getDumpedAhatInstance("aLongStrongPathToSamplePathObject");
    AhatInstance strong2 = strong1.getField("referent").asAhatInstance();
    AhatInstance strong3 = strong2.getField("referent").asAhatInstance();
    AhatInstance object = strong3.getField("referent").asAhatInstance();

    List<PathElement> path = object.getPathFromGcRoot();
    assertEquals(strong3, path.get(path.size() - 2).instance);
  }

  @Test
  public void retainedSize() throws IOException {
    TestDump dump = TestDump.getTestDump();

    // anObject should not be an immediate dominator of any other object. This
    // means its retained size should be equal to its size for the heap it was
    // allocated on, and should be 0 for all other heaps.
    AhatInstance anObject = dump.getDumpedAhatInstance("anObject");
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    Size size = anObject.getSize();
    assertEquals(size, anObject.getTotalRetainedSize());
    assertEquals(size, anObject.getRetainedSize(anObject.getHeap()));
    for (AhatHeap heap : snapshot.getHeaps()) {
      if (!heap.equals(anObject.getHeap())) {
        assertEquals(String.format("For heap '%s'", heap.getName()),
            Size.ZERO, anObject.getRetainedSize(heap));
      }
    }
  }

  @Test
  public void objectNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertNull(obj.asBitmap());
  }

  @Test
  public void arrayNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("gcPathArray");
    assertNull(obj.asBitmap());
  }

  @Test
  public void classObjNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.findClass("Main");
    assertNull(obj.asBitmap());
  }

  @Test
  public void classInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("aPhantomReference");
    long id = obj.getId();
    assertEquals(String.format("java.lang.ref.PhantomReference@%08x", id), obj.toString());
  }

  @Test
  public void classObjToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.findClass("Main");
    assertEquals("class Main", obj.toString());
  }

  @Test
  public void arrayInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("gcPathArray");
    long id = obj.getId();

    // There's a bug in perfib's proguard deobfuscation for arrays.
    // To work around that bug for the time being, only test the suffix of
    // the toString result. Ideally we test for string equality against
    // "Main$ObjectTree[4]@%08x", id.
    assertTrue(obj.toString().endsWith(String.format("[4]@%08x", id)));
  }

  @Test
  public void primArrayInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("bigArray");
    long id = obj.getId();
    assertEquals(String.format("byte[1000000]@%08x", id), obj.toString());
  }

  @Test
  public void isNotRoot() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertFalse(obj.isRoot());
    assertNull(obj.getRootTypes());
  }

  @Test
  public void weakRefToGcRoot() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance ref = dump.getDumpedAhatInstance("aWeakRefToGcRoot");

    // The weak reference points to Main.class, which we expect will be marked
    // as a GC root. In theory Main.class doesn't have to be a GC root, in
    // which case this test case will need to be revised.
    AhatInstance root = ref.getField("referent").asAhatInstance();
    assertTrue(root.isRoot());

    // We had a bug in the past where weak references to GC roots caused the
    // roots to be incorrectly be considered weakly reachable.
    assertTrue(root.isStronglyReachable());
    assertFalse(root.isWeaklyReachable());
  }

  @Test
  public void weakReferenceChain() throws IOException {
    // If the only reference to a chain of strongly referenced objects is a
    // weak reference, then all of the objects should be considered weakly
    // reachable.
    TestDump dump = TestDump.getTestDump();
    AhatInstance ref = dump.getDumpedAhatInstance("aWeakChain");
    AhatInstance weak1 = ref.getField("referent").asAhatInstance();
    AhatInstance weak2 = weak1.getField("referent").asAhatInstance();
    AhatInstance weak3 = weak2.getField("referent").asAhatInstance();
    assertTrue(ref.isStronglyReachable());
    assertTrue(weak1.isWeaklyReachable());
    assertTrue(weak2.isWeaklyReachable());
    assertTrue(weak3.isWeaklyReachable());
  }

  @Test
  public void reverseReferences() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    AhatInstance ref = dump.getDumpedAhatInstance("aReference");
    AhatInstance weak = dump.getDumpedAhatInstance("aWeakReference");
    assertTrue(obj.getHardReverseReferences().contains(ref));
    assertFalse(obj.getHardReverseReferences().contains(weak));
    assertFalse(obj.getSoftReverseReferences().contains(ref));
    assertTrue(obj.getSoftReverseReferences().contains(weak));
  }

  @Test
  public void asStringEmbedded() throws IOException {
    // On Android L, image strings were backed by a single big char array.
    // Verify we show just the relative part of the string, not the entire
    // char array.
    TestDump dump = TestDump.getTestDump("L.hprof", null, null);
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    // java.lang.String@0x6fe17050 is an image string "char" backed by a
    // shared char array.
    AhatInstance str = snapshot.findInstance(0x6fe17050);
    assertEquals("char", str.asString());
  }

  @Test
  public void nonDefaultHeapRoot() throws IOException {
    TestDump dump = TestDump.getTestDump("O.hprof", null, null);
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    // java.util.HashMap@6004fdb8 is marked as a VM INTERNAL root.
    // Previously we had a bug where roots not on the default heap were not
    // properly treated as roots (b/65356532).
    AhatInstance map = snapshot.findInstance(0x6004fdb8);
    assertEquals("java.util.HashMap", map.getClassName());
    assertTrue(map.isRoot());
  }

  @Test
  public void threadRoot() throws IOException {
    TestDump dump = TestDump.getTestDump("O.hprof", null, null);
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    // java.lang.Thread@12c03470 is marked as a thread root.
    // Previously we had a bug where thread roots were not properly treated as
    // roots (b/65356532).
    AhatInstance thread = snapshot.findInstance(0x12c03470);
    assertEquals("java.lang.Thread", thread.getClassName());
    assertTrue(thread.isRoot());
  }

  @Test
  public void classOfClass() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    AhatClassObj cls = obj.getClassObj();
    AhatClassObj clscls = cls.getClassObj();
    assertNotNull(clscls);
    assertEquals("java.lang.Class", clscls.getName());
  }

  @Test
  public void nullValueString() throws IOException {
    TestDump dump = TestDump.getTestDump("RI.hprof", null, null);
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    // java.lang.String@500001a8 has a null 'value' field, which should not
    // cause ahat to crash.
    AhatInstance str = snapshot.findInstance(0x500001a8);
    assertEquals("java.lang.String", str.getClassName());
    assertNull(str.asString());
  }
}
