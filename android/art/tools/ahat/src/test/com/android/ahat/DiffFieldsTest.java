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

package com.android.ahat;

import com.android.ahat.heapdump.DiffFields;
import com.android.ahat.heapdump.DiffedFieldValue;
import com.android.ahat.heapdump.FieldValue;
import com.android.ahat.heapdump.Type;
import com.android.ahat.heapdump.Value;
import java.util.ArrayList;
import java.util.List;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

public class DiffFieldsTest {
  // Give more convenient abstract names for different types.
  private static final Type t0 = Type.OBJECT;
  private static final Type t1 = Type.BOOLEAN;
  private static final Type t2 = Type.CHAR;
  private static final Type t3 = Type.FLOAT;
  private static final Type t4 = Type.DOUBLE;
  private static final Type t5 = Type.BYTE;
  private static final Type t6 = Type.SHORT;
  private static final Type t7 = Type.INT;
  private static final Type t8 = Type.LONG;

  @Test
  public void normalMatchedDiffedFieldValues() {
    FieldValue normal1 = new FieldValue("name", t0, Value.pack(1));
    FieldValue normal2 = new FieldValue("name", t0, Value.pack(2));

    DiffedFieldValue x = DiffedFieldValue.matched(normal1, normal2);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertEquals(Value.pack(1), x.current);
    assertEquals(Value.pack(2), x.baseline);
    assertEquals(DiffedFieldValue.Status.MATCHED, x.status);
  }

  @Test
  public void nulledMatchedDiffedFieldValues() {
    FieldValue normal = new FieldValue("name", t0, Value.pack(1));
    FieldValue nulled = new FieldValue("name", t0, null);

    DiffedFieldValue x = DiffedFieldValue.matched(normal, nulled);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertEquals(Value.pack(1), x.current);
    assertNull(x.baseline);
    assertEquals(DiffedFieldValue.Status.MATCHED, x.status);

    DiffedFieldValue y = DiffedFieldValue.matched(nulled, normal);
    assertEquals("name", y.name);
    assertEquals(t0, y.type);
    assertNull(y.current);
    assertEquals(Value.pack(1), y.baseline);
    assertEquals(DiffedFieldValue.Status.MATCHED, y.status);
  }

  @Test
  public void normalAddedDiffedFieldValues() {
    FieldValue normal = new FieldValue("name", t0, Value.pack(1));

    DiffedFieldValue x = DiffedFieldValue.added(normal);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertEquals(Value.pack(1), x.current);
    assertEquals(DiffedFieldValue.Status.ADDED, x.status);
  }

  @Test
  public void nulledAddedDiffedFieldValues() {
    FieldValue nulled = new FieldValue("name", t0, null);

    DiffedFieldValue x = DiffedFieldValue.added(nulled);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertNull(x.current);
    assertEquals(DiffedFieldValue.Status.ADDED, x.status);
  }

  @Test
  public void normalDeletedDiffedFieldValues() {
    FieldValue normal = new FieldValue("name", t0, Value.pack(1));

    DiffedFieldValue x = DiffedFieldValue.deleted(normal);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertEquals(Value.pack(1), x.baseline);
    assertEquals(DiffedFieldValue.Status.DELETED, x.status);
  }

  @Test
  public void nulledDeletedDiffedFieldValues() {
    FieldValue nulled = new FieldValue("name", t0, null);

    DiffedFieldValue x = DiffedFieldValue.deleted(nulled);
    assertEquals("name", x.name);
    assertEquals(t0, x.type);
    assertNull(x.baseline);
    assertEquals(DiffedFieldValue.Status.DELETED, x.status);
  }

  @Test
  public void basicDiff() {
    List<FieldValue> a = new ArrayList<FieldValue>();
    a.add(new FieldValue("n0", t0, null));
    a.add(new FieldValue("n2", t2, null));
    a.add(new FieldValue("n3", t3, null));
    a.add(new FieldValue("n4", t4, null));
    a.add(new FieldValue("n5", t5, null));
    a.add(new FieldValue("n6", t6, null));

    List<FieldValue> b = new ArrayList<FieldValue>();
    b.add(new FieldValue("n0", t0, null));
    b.add(new FieldValue("n1", t1, null));
    b.add(new FieldValue("n2", t2, null));
    b.add(new FieldValue("n3", t3, null));
    b.add(new FieldValue("n5", t5, null));
    b.add(new FieldValue("n6", t6, null));
    b.add(new FieldValue("n7", t7, null));

    // Note: The expected result makes assumptions about the implementation of
    // field diff to match the order of the returned fields. If the
    // implementation changes, this test may need to be generalized to accept
    // the new implementation.
    List<DiffedFieldValue> expected = new ArrayList<DiffedFieldValue>();
    expected.add(DiffedFieldValue.matched(a.get(0), b.get(0)));
    expected.add(DiffedFieldValue.deleted(b.get(1)));
    expected.add(DiffedFieldValue.matched(a.get(1), b.get(2)));
    expected.add(DiffedFieldValue.matched(a.get(2), b.get(3)));
    expected.add(DiffedFieldValue.added(a.get(3)));
    expected.add(DiffedFieldValue.matched(a.get(4), b.get(4)));
    expected.add(DiffedFieldValue.matched(a.get(5), b.get(5)));
    expected.add(DiffedFieldValue.deleted(b.get(6)));

    List<DiffedFieldValue> diffed = DiffFields.diff(a, b);
    assertEquals(expected, diffed);
  }

  @Test
  public void reorderedDiff() {
    List<FieldValue> a = new ArrayList<FieldValue>();
    a.add(new FieldValue("n0", t0, null));
    a.add(new FieldValue("n1", t1, null));
    a.add(new FieldValue("n2", t2, null));
    a.add(new FieldValue("n3", t3, null));
    a.add(new FieldValue("n4", t4, null));
    a.add(new FieldValue("n5", t5, null));
    a.add(new FieldValue("n6", t6, null));

    List<FieldValue> b = new ArrayList<FieldValue>();
    b.add(new FieldValue("n4", t4, null));
    b.add(new FieldValue("n1", t1, null));
    b.add(new FieldValue("n3", t3, null));
    b.add(new FieldValue("n0", t0, null));
    b.add(new FieldValue("n5", t5, null));
    b.add(new FieldValue("n2", t2, null));
    b.add(new FieldValue("n6", t6, null));

    // Note: The expected result makes assumptions about the implementation of
    // field diff to match the order of the returned fields. If the
    // implementation changes, this test may need to be generalized to accept
    // the new implementation.
    List<DiffedFieldValue> expected = new ArrayList<DiffedFieldValue>();
    expected.add(DiffedFieldValue.matched(a.get(0), b.get(3)));
    expected.add(DiffedFieldValue.matched(a.get(1), b.get(1)));
    expected.add(DiffedFieldValue.matched(a.get(2), b.get(5)));
    expected.add(DiffedFieldValue.matched(a.get(3), b.get(2)));
    expected.add(DiffedFieldValue.matched(a.get(4), b.get(0)));
    expected.add(DiffedFieldValue.matched(a.get(5), b.get(4)));
    expected.add(DiffedFieldValue.matched(a.get(6), b.get(6)));

    List<DiffedFieldValue> diffed = DiffFields.diff(a, b);
    assertEquals(expected, diffed);
  }
}
