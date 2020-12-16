# /*
#  * Copyright (C) 2016 The Android Open Source Project
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
# public interface Iface1 {
#   public default void sayHi() {
#       System.out.println("FAILED: We should never invoke this method!");
#   }
# }

.class public abstract interface LIface1;
.super Ljava/lang/Object;

.method public sayHi()V
    .locals 2
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "FAILED: We should never invoke this method!"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
