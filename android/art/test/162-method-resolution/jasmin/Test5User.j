; Copyright (C) 2017 The Android Open Source Project
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

.class public Test5User
.super java/lang/Object

.method public static test()V
    .limit stack 2
    .limit locals 1
    new Test5Derived
    dup
    invokespecial Test5Derived.<init>()V
    astore_0

    ; Call an unresolved method bar() to force verification at runtime
    ; to populate the dex cache entry for Test5Base.foo()V.
    ; try { b.bar(); } catch (IncompatibleClassChangeError icce) { }
    aload_0
    dup ; Bogus operand to be swallowed by the pop in the non-exceptional path.
  catch_begin:
    invokevirtual Test5Derived.bar()V
  catch_end:
    pop ; Pops the exception or the bogus operand from above.
  .catch java/lang/IncompatibleClassChangeError from catch_begin to catch_end using catch_end

    aload_0
    invokevirtual Test5Derived.foo()V
    return
.end method
