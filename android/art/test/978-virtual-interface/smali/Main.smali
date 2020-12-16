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
# public class Main {
#   public static void main(String[] s) {
#     Subtype s = new Subtype();
#     try {
#       s.callPackage();
#       System.out.println("No error thrown!");
#     } catch (IncompatibleClassChangeError e) {
#       System.out.println("Recieved expected ICCE error!");
#     }
#   }
# }

.class public LMain;

.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
    .locals 3

    new-instance v0, LSubtype;
    invoke-direct {v0}, LSubtype;-><init>()V
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    :try_start
        invoke-virtual {v0}, LSubtype;->callPackage()V
        const-string v1, "No error thrown!"
        invoke-virtual {v2, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
        return-void
    :try_end
    .catch Ljava/lang/IncompatibleClassChangeError; {:try_start .. :try_end} :error_start
    :error_start
        const-string v1, "Recieved expected ICCE error!"
        invoke-virtual {v2, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
        return-void
.end method
