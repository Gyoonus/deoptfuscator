# /*
#  * Copyright (C) 2015 The Android Open Source Project
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  */
#
# // Methods are sorted in alphabetical order in dex file. We need 10 padding
# // methods to ensure the 11'th target lines up to the same vtable slot as the
# // first Subtype virtual method (the other 10 are the java/lang/Object;
# // methods).
# interface Iface {
#   public default void fakeMethod_A() {}
#   public default void fakeMethod_B() {}
#   public default void fakeMethod_C() {}
#   public default void fakeMethod_D() {}
#   public default void fakeMethod_E() {}
#   public default void fakeMethod_F() {}
#   public default void fakeMethod_G() {}
#   public default void fakeMethod_H() {}
#   public default void fakeMethod_I() {}
#   public default void fakeMethod_J() {}
#   public default void fakeMethod_K() {}
#   public default void fakeMethod_Target() {}
# }

.class public abstract interface LIface;

.super Ljava/lang/Object;

# // 1
.method public fakeMethod_A()V
  .locals 0
  return-void
.end method

# // 2
.method public fakeMethod_B()V
  .locals 0
  return-void
.end method

# // 3
.method public fakeMethod_C()V
  .locals 0
  return-void
.end method

# // 4
.method public fakeMethod_D()V
  .locals 0
  return-void
.end method

# // 5
.method public fakeMethod_E()V
  .locals 0
  return-void
.end method

# // 5
.method public fakeMethod_F()V
  .locals 0
  return-void
.end method

# // 6
.method public fakeMethod_G()V
  .locals 0
  return-void
.end method

# // 7
.method public fakeMethod_H()V
  .locals 0
  return-void
.end method

# // 8
.method public fakeMethod_I()V
  .locals 0
  return-void
.end method

# // 9
.method public fakeMethod_J()V
  .locals 0
  return-void
.end method

# // 10
.method public fakeMethod_K()V
  .locals 0
  return-void
.end method

# // 11
.method public fakeMethod_Target()V
  .locals 0
  return-void
.end method
