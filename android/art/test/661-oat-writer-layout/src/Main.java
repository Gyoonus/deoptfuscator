// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;

public class Main {

  static class OatMethodAndOffset implements Comparable<OatMethodAndOffset> {
    Method method;
    long codeOffset;

    public OatMethodAndOffset(Method method, long codeOffset) {
      this.method = method;
      this.codeOffset = codeOffset;
    }

    // e.g. "Foo::Bar()"
    public String methodReferenceString() {
      return method.getDeclaringClass().getName() + "::" + method.getName();
    }

    @Override
    public int compareTo(OatMethodAndOffset other) {
      return Long.compareUnsigned(codeOffset, other.codeOffset);
    }
  }

  // Print the list of methods in Generated.class, sorted by their OAT code address.
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    // Make sure to check "Test.class" because Main.class still has JNI which could be compiled
    // even if the rest of the classes are not.
    if (!hasOatCompiledCode(Test.class)) {
      System.out.println("No OAT class");
      return;
    }

    // We only care about explicitly defined methods from Generated.java.
    Method[] interesting_methods;
    try {
      interesting_methods = Test.getTestMethods();
    } catch (NoSuchMethodException e) {
      e.printStackTrace();
      return;
    }

    // Get the list of oat code methods for each Java method.
    ArrayList<OatMethodAndOffset> offsets_list = new ArrayList<OatMethodAndOffset>();
    for (Method m : interesting_methods) {
      offsets_list.add(new OatMethodAndOffset(m, getOatMethodQuickCode(m)));
    }

    // Sort by the offset address.
    Collections.sort(offsets_list);

    // Print each method as a method reference string.
    for (OatMethodAndOffset m : offsets_list) {
      System.out.println(m.methodReferenceString());
    }
  }

  // Does Main.class have an OatClass with actually compiled code?
  private static native boolean hasOatCompiledCode(Class kls);
  // Get the OatMethod's pointer to code. We get 'real' memory address, not relative offset,
  // but it's still good since we never compare multiple OAT files here.
  private static native long getOatMethodQuickCode(Method method);
}
