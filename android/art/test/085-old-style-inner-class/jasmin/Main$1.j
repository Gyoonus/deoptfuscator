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

.source                  Main.java
.class                   final Main$1
.super                   java/lang/Object
.implements              java/lang/Runnable

; new Runnable() {
;   public void run() { }
; };

.method                  <init>()V
   .limit stack          1
   .limit locals         1
   .line                 23
   aload_0
   invokespecial         java/lang/Object/<init>()V
   return
.end method

.method                  public run()V
   .limit stack          0
   .limit locals         1
   .line                 24
   return
.end method

