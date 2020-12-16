
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
# public interface Iface {
#   public default void sayHi() {
#     System.out.println(getHiWords());
#   }
#
#   // Synthetic method
#   private String getHiWords() {
#     return "HELLO!";
#   }
# }

.class public abstract interface LIface;
.super Ljava/lang/Object;

.method public sayHi()V
    .locals 2
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-direct {p0}, LIface;->getHiWords()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method private synthetic getHiWords()Ljava/lang/String;
    .locals 1
    const-string v0, "HELLO!"
    return-object v0
.end method
