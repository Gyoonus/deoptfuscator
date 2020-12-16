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

// Simple test for field accesses.

public class Main extends TestCase {
  public static void main(String[] args) {
    $opt$testAll();
  }

  static void $opt$testAll() {
    AllFields fields = new AllFields();

    assertEquals(false, fields.iZ);
    assertEquals(0, fields.iB);
    assertEquals(0, fields.iC);
    assertEquals(0, fields.iI);
    assertEquals(0, fields.iJ);
    assertEquals(0, fields.iS);
    assertEquals(0.0f, fields.iF);
    assertEquals(0.0, fields.iD);
    assertNull(fields.iObject);

    long longValue = -1122198787987987987L;
    fields.iZ = true;
    fields.iB = -2;
    fields.iC = 'c';
    fields.iI = 42;
    fields.iJ = longValue;
    fields.iS = 68;
    fields.iObject = fields;
    fields.iF = 2.3f;
    fields.iD = 5.3;

    assertEquals(true, fields.iZ);
    assertEquals(-2, fields.iB);
    assertEquals('c', fields.iC);
    assertEquals(42, fields.iI);
    assertEquals(longValue, fields.iJ);
    assertEquals(68, fields.iS);
    assertEquals(fields, fields.iObject);
    assertEquals(2.3f, fields.iF);
    assertEquals(5.3, fields.iD);
  }

  static class AllFields {
    boolean iZ;
    byte iB;
    char iC;
    double iD;
    float iF;
    int iI;
    long iJ;
    short iS;
    Object iObject;
  }
}
