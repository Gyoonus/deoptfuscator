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

//
// Lists several combinations of Classes X Methods X Hotness:
//
// Class A-C:
//   - Ensure method hotness overrides sorting by class_def_idx
//
// Method m_a : m_c
//   - Ensure method hotness overrides sorting by method_id
//
// Method m_a$Hot$Enum$Bits
//   - $X$Y$Z is an encoding of MethodHotness flags ($[Hot]$[Startup]$[Poststartup])
//   - The method name encoding matches the `profile` hotness.
//   - Check all variations of the bits to make sure it sorts by hotness correctly.
//

class A {
  // Note that every method has unique dex code (by using a unique string literal).
  // This is to prevent dex/oat code deduping. Deduped methods do not get distinct bins.
  void m_a$$$() { System.out.println("Don't dedupe me! A::m_a$$$"); }
  void m_a$Hot$$() { System.out.println("Don't dedupe me! A::m_a$Hot$$"); }
  void m_a$$Startup$() { System.out.println("Don't dedupe me! A::m_a$$Startup$"); }
  void m_a$Hot$Startup$() { System.out.println("Don't dedupe me! A::m_a$Hot$Startup$"); }
  void m_a$$$Poststartup() { System.out.println("Don't dedupe me! A::m_a$$$Poststartup"); }
  void m_a$Hot$$Poststartup() { System.out.println("Don't dedupe me! A::m_a$Hot$$Poststartup"); }
  void m_a$$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_a$$Startup$Poststartup"); }
  void m_a$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_a$Hot$Startup$Poststartup"); }
  void m_b$$$() { System.out.println("Don't dedupe me! A::m_b$$$"); }
  void m_b$Hot$$() { System.out.println("Don't dedupe me! A::m_b$Hot$$"); }
  void m_b$$Startup$() { System.out.println("Don't dedupe me! A::m_b$$Startup$"); }
  void m_b$Hot$Startup$() { System.out.println("Don't dedupe me! A::m_b$Hot$Startup$"); }
  void m_b$$$Poststartup() { System.out.println("Don't dedupe me! A::m_b$$$Poststartup"); }
  void m_b$Hot$$Poststartup() { System.out.println("Don't dedupe me! A::m_b$Hot$$Poststartup"); }
  void m_b$$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_b$$Startup$Poststartup"); }
  void m_b$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_b$Hot$Startup$Poststartup"); }
  void m_c$$$() { System.out.println("Don't dedupe me! A::m_c$$$"); }
  void m_c$Hot$$() { System.out.println("Don't dedupe me! A::m_c$Hot$$"); }
  void m_c$$Startup$() { System.out.println("Don't dedupe me! A::m_c$$Startup$"); }
  void m_c$Hot$Startup$() { System.out.println("Don't dedupe me! A::m_c$Hot$Startup$"); }
  void m_c$$$Poststartup() { System.out.println("Don't dedupe me! A::m_c$$$Poststartup"); }
  void m_c$Hot$$Poststartup() { System.out.println("Don't dedupe me! A::m_c$Hot$$Poststartup"); }
  void m_c$$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_c$$Startup$Poststartup"); }
  void m_c$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! A::m_c$Hot$Startup$Poststartup"); }
}
class B {
  void m_a$$$() { System.out.println("Don't dedupe me! B::m_a$$$"); }
  void m_a$Hot$$() { System.out.println("Don't dedupe me! B::m_a$Hot$$"); }
  void m_a$$Startup$() { System.out.println("Don't dedupe me! B::m_a$$Startup$"); }
  void m_a$Hot$Startup$() { System.out.println("Don't dedupe me! B::m_a$Hot$Startup$"); }
  void m_a$$$Poststartup() { System.out.println("Don't dedupe me! B::m_a$$$Poststartup"); }
  void m_a$Hot$$Poststartup() { System.out.println("Don't dedupe me! B::m_a$Hot$$Poststartup"); }
  void m_a$$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_a$$Startup$Poststartup"); }
  void m_a$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_a$Hot$Startup$Poststartup"); }
  void m_b$$$() { System.out.println("Don't dedupe me! B::m_b$$$"); }
  void m_b$Hot$$() { System.out.println("Don't dedupe me! B::m_b$Hot$$"); }
  void m_b$$Startup$() { System.out.println("Don't dedupe me! B::m_b$$Startup$"); }
  void m_b$Hot$Startup$() { System.out.println("Don't dedupe me! B::m_b$Hot$Startup$"); }
  void m_b$$$Poststartup() { System.out.println("Don't dedupe me! B::m_b$$$Poststartup"); }
  void m_b$Hot$$Poststartup() { System.out.println("Don't dedupe me! B::m_b$Hot$$Poststartup"); }
  void m_b$$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_b$$Startup$Poststartup"); }
  void m_b$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_b$Hot$Startup$Poststartup"); }
  void m_c$$$() { System.out.println("Don't dedupe me! B::m_c$$$"); }
  void m_c$Hot$$() { System.out.println("Don't dedupe me! B::m_c$Hot$$"); }
  void m_c$$Startup$() { System.out.println("Don't dedupe me! B::m_c$$Startup$"); }
  void m_c$Hot$Startup$() { System.out.println("Don't dedupe me! B::m_c$Hot$Startup$"); }
  void m_c$$$Poststartup() { System.out.println("Don't dedupe me! B::m_c$$$Poststartup"); }
  void m_c$Hot$$Poststartup() { System.out.println("Don't dedupe me! B::m_c$Hot$$Poststartup"); }
  void m_c$$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_c$$Startup$Poststartup"); }
  void m_c$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! B::m_c$Hot$Startup$Poststartup"); }
}
class C {
  void m_a$$$() { System.out.println("Don't dedupe me! C::m_a$$$"); }
  void m_a$Hot$$() { System.out.println("Don't dedupe me! C::m_a$Hot$$"); }
  void m_a$$Startup$() { System.out.println("Don't dedupe me! C::m_a$$Startup$"); }
  void m_a$Hot$Startup$() { System.out.println("Don't dedupe me! C::m_a$Hot$Startup$"); }
  void m_a$$$Poststartup() { System.out.println("Don't dedupe me! C::m_a$$$Poststartup"); }
  void m_a$Hot$$Poststartup() { System.out.println("Don't dedupe me! C::m_a$Hot$$Poststartup"); }
  void m_a$$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_a$$Startup$Poststartup"); }
  void m_a$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_a$Hot$Startup$Poststartup"); }
  void m_b$$$() { System.out.println("Don't dedupe me! C::m_b$$$"); }
  void m_b$Hot$$() { System.out.println("Don't dedupe me! C::m_b$Hot$$"); }
  void m_b$$Startup$() { System.out.println("Don't dedupe me! C::m_b$$Startup$"); }
  void m_b$Hot$Startup$() { System.out.println("Don't dedupe me! C::m_b$Hot$Startup$"); }
  void m_b$$$Poststartup() { System.out.println("Don't dedupe me! C::m_b$$$Poststartup"); }
  void m_b$Hot$$Poststartup() { System.out.println("Don't dedupe me! C::m_b$Hot$$Poststartup"); }
  void m_b$$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_b$$Startup$Poststartup"); }
  void m_b$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_b$Hot$Startup$Poststartup"); }
  void m_c$$$() { System.out.println("Don't dedupe me! C::m_c$$$"); }
  void m_c$Hot$$() { System.out.println("Don't dedupe me! C::m_c$Hot$$"); }
  void m_c$$Startup$() { System.out.println("Don't dedupe me! C::m_c$$Startup$"); }
  void m_c$Hot$Startup$() { System.out.println("Don't dedupe me! C::m_c$Hot$Startup$"); }
  void m_c$$$Poststartup() { System.out.println("Don't dedupe me! C::m_c$$$Poststartup"); }
  void m_c$Hot$$Poststartup() { System.out.println("Don't dedupe me! C::m_c$Hot$$Poststartup"); }
  void m_c$$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_c$$Startup$Poststartup"); }
  void m_c$Hot$Startup$Poststartup() { System.out.println("Don't dedupe me! C::m_c$Hot$Startup$Poststartup"); }
}
