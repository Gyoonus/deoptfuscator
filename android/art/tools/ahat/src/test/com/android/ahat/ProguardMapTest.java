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

import com.android.ahat.proguard.ProguardMap;
import java.io.IOException;
import java.io.StringReader;
import java.text.ParseException;
import org.junit.Test;
import static org.junit.Assert.assertEquals;

public class ProguardMapTest {
  private static final String TEST_MAP =
    "class.that.is.Empty -> a:\n"
    + "class.that.is.Empty$subclass -> b:\n"
    + "class.with.only.Fields -> c:\n"
    + "    int prim_type_field -> a\n"
    + "    int[] prim_array_type_field -> b\n"
    + "    class.that.is.Empty class_type_field -> c\n"
    + "    class.that.is.Empty[] array_type_field -> d\n"
    + "    int longObfuscatedNameField -> abc\n"
    + "class.with.Methods -> d:\n"
    + "    int some_field -> a\n"
    + "    12:23:void <clinit>() -> <clinit>\n"
    + "    42:43:void boringMethod() -> m\n"
    + "    45:48:void methodWithPrimArgs(int,float) -> m\n"
    + "    49:50:void methodWithPrimArrArgs(int[],float) -> m\n"
    + "    52:55:void methodWithClearObjArg(class.not.in.Map) -> m\n"
    + "    57:58:void methodWithClearObjArrArg(class.not.in.Map[]) -> m\n"
    + "    59:61:void methodWithObfObjArg(class.with.only.Fields) -> m\n"
    + "    64:66:class.with.only.Fields methodWithObfRes() -> n\n"
    + "    80:80:void lineObfuscatedMethod():8:8 -> o\n"
    + "    90:90:void lineObfuscatedMethod2():9 -> p\n"
    ;

  @Test
  public void proguardMap() throws IOException, ParseException {
    ProguardMap map = new ProguardMap();

    // An empty proguard map should not deobfuscate anything.
    assertEquals("foo.bar.Sludge", map.getClassName("foo.bar.Sludge"));
    assertEquals("fooBarSludge", map.getClassName("fooBarSludge"));
    assertEquals("myfield", map.getFieldName("foo.bar.Sludge", "myfield"));
    assertEquals("myfield", map.getFieldName("fooBarSludge", "myfield"));
    ProguardMap.Frame frame = map.getFrame(
        "foo.bar.Sludge", "mymethod", "(Lfoo/bar/Sludge;)V", "SourceFile.java", 123);
    assertEquals("mymethod", frame.method);
    assertEquals("(Lfoo/bar/Sludge;)V", frame.signature);
    assertEquals("SourceFile.java", frame.filename);
    assertEquals(123, frame.line);

    // Read in the proguard map.
    map.readFromReader(new StringReader(TEST_MAP));

    // It should still not deobfuscate things that aren't in the map
    assertEquals("foo.bar.Sludge", map.getClassName("foo.bar.Sludge"));
    assertEquals("fooBarSludge", map.getClassName("fooBarSludge"));
    assertEquals("myfield", map.getFieldName("foo.bar.Sludge", "myfield"));
    assertEquals("myfield", map.getFieldName("fooBarSludge", "myfield"));
    frame = map.getFrame("foo.bar.Sludge", "mymethod", "(Lfoo/bar/Sludge;)V",
        "SourceFile.java", 123);
    assertEquals("mymethod", frame.method);
    assertEquals("(Lfoo/bar/Sludge;)V", frame.signature);
    assertEquals("SourceFile.java", frame.filename);
    assertEquals(123, frame.line);

    // Test deobfuscation of class names
    assertEquals("class.that.is.Empty", map.getClassName("a"));
    assertEquals("class.that.is.Empty$subclass", map.getClassName("b"));
    assertEquals("class.with.only.Fields", map.getClassName("c"));
    assertEquals("class.with.Methods", map.getClassName("d"));

    // Test deobfuscation of array classes.
    assertEquals("class.with.Methods[]", map.getClassName("d[]"));
    assertEquals("class.with.Methods[][]", map.getClassName("d[][]"));

    // Test deobfuscation of methods
    assertEquals("prim_type_field", map.getFieldName("class.with.only.Fields", "a"));
    assertEquals("prim_array_type_field", map.getFieldName("class.with.only.Fields", "b"));
    assertEquals("class_type_field", map.getFieldName("class.with.only.Fields", "c"));
    assertEquals("array_type_field", map.getFieldName("class.with.only.Fields", "d"));
    assertEquals("longObfuscatedNameField", map.getFieldName("class.with.only.Fields", "abc"));
    assertEquals("some_field", map.getFieldName("class.with.Methods", "a"));

    // Test deobfuscation of frames
    frame = map.getFrame("class.with.Methods", "<clinit>", "()V", "SourceFile.java", 13);
    assertEquals("<clinit>", frame.method);
    assertEquals("()V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(13, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "()V", "SourceFile.java", 42);
    assertEquals("boringMethod", frame.method);
    assertEquals("()V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(42, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "(IF)V", "SourceFile.java", 45);
    assertEquals("methodWithPrimArgs", frame.method);
    assertEquals("(IF)V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(45, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "([IF)V", "SourceFile.java", 49);
    assertEquals("methodWithPrimArrArgs", frame.method);
    assertEquals("([IF)V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(49, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "(Lclass/not/in/Map;)V",
        "SourceFile.java", 52);
    assertEquals("methodWithClearObjArg", frame.method);
    assertEquals("(Lclass/not/in/Map;)V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(52, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "([Lclass/not/in/Map;)V",
        "SourceFile.java", 57);
    assertEquals("methodWithClearObjArrArg", frame.method);
    assertEquals("([Lclass/not/in/Map;)V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(57, frame.line);

    frame = map.getFrame("class.with.Methods", "m", "(Lc;)V", "SourceFile.java", 59);
    assertEquals("methodWithObfObjArg", frame.method);
    assertEquals("(Lclass/with/only/Fields;)V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(59, frame.line);

    frame = map.getFrame("class.with.Methods", "n", "()Lc;", "SourceFile.java", 64);
    assertEquals("methodWithObfRes", frame.method);
    assertEquals("()Lclass/with/only/Fields;", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(64, frame.line);

    frame = map.getFrame("class.with.Methods", "o", "()V", "SourceFile.java", 80);
    assertEquals("lineObfuscatedMethod", frame.method);
    assertEquals("()V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(8, frame.line);

    frame = map.getFrame("class.with.Methods", "p", "()V", "SourceFile.java", 94);
    assertEquals("lineObfuscatedMethod2", frame.method);
    assertEquals("()V", frame.signature);
    assertEquals("Methods.java", frame.filename);
    assertEquals(13, frame.line);

    // Some methods may not have been obfuscated. We should still be able
    // to compute the filename properly.
    frame = map.getFrame("class.with.Methods", "unObfuscatedMethodName",
        "()V", "SourceFile.java", 0);
    assertEquals("Methods.java", frame.filename);
  }
}
