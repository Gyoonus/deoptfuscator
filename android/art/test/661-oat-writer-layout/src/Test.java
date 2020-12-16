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

public class Test {
  // Returns list of all methods in Generated.java
  // This is to avoid having to introspect classes with extra code
  // (for example, we ignore <init> methods).
  public static Method[] getTestMethods() throws NoSuchMethodException, SecurityException {
    ArrayList<Method> all_methods = new ArrayList<Method>();
    all_methods.add(A.class.getDeclaredMethod("m_a$$$"));
    all_methods.add(A.class.getDeclaredMethod("m_a$$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_a$Hot$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_a$$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_a$Hot$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_a$$Startup$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_b$$$"));
    all_methods.add(A.class.getDeclaredMethod("m_b$$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_b$Hot$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_b$$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_b$Hot$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_b$$Startup$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_c$$$"));
    all_methods.add(A.class.getDeclaredMethod("m_c$$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_c$Hot$Startup$"));
    all_methods.add(A.class.getDeclaredMethod("m_c$$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_c$Hot$$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_c$$Startup$Poststartup"));
    all_methods.add(A.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_a$$$"));
    all_methods.add(B.class.getDeclaredMethod("m_a$$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_a$Hot$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_a$$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_a$Hot$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_a$$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_b$$$"));
    all_methods.add(B.class.getDeclaredMethod("m_b$$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_b$Hot$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_b$$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_b$Hot$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_b$$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_c$$$"));
    all_methods.add(B.class.getDeclaredMethod("m_c$$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_c$Hot$Startup$"));
    all_methods.add(B.class.getDeclaredMethod("m_c$$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_c$Hot$$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_c$$Startup$Poststartup"));
    all_methods.add(B.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_a$$$"));
    all_methods.add(C.class.getDeclaredMethod("m_a$$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_a$Hot$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_a$$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_a$Hot$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_a$$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_b$$$"));
    all_methods.add(C.class.getDeclaredMethod("m_b$$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_b$Hot$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_b$$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_b$Hot$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_b$$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_c$$$"));
    all_methods.add(C.class.getDeclaredMethod("m_c$$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_c$Hot$Startup$"));
    all_methods.add(C.class.getDeclaredMethod("m_c$$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_c$Hot$$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_c$$Startup$Poststartup"));
    all_methods.add(C.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup"));
    return all_methods.toArray(new Method[all_methods.size()]);
  }
}

