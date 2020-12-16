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

.class                   public SubClassUsingInaccessibleField
.super                   other/PublicClass

.method                  public <init>()V
   .limit stack          1
   .limit locals         1
   aload_0
   invokespecial         other/PublicClass/<init>()V
   return
.end method

; Regression test for compiler DCHECK() failure (bogus check) when referencing
; a package-private field from an indirectly inherited package-private class,
; using this very class as the declaring class in the FieldId, bug: 27684368 .
.method                  public test()I
   .limit stack          1
   .limit locals         1
   aload_0
   getfield              SubClassUsingInaccessibleField/otherProtectedClassPackageIntInstanceField I
   ireturn
.end method

