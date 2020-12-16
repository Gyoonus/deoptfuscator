# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# We cannot implement Main in Java, as this would require to run
# dexmerger (to merge the Dex file produced from Smali code and the
# Dex file produced from Java code), which loops indefinitely when
# processing class B28685551, as this class inherits from itself.  As
# a workaround, implement Main using Smali (we could also have used
# multidex, but this requires a custom build script).

.class public LMain;
.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
    .registers 3
    .param p0, "args"

    invoke-static {}, LMain;->test()V
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "Done!"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
.end method

.method static test()V
    .registers 4

    :try_start
    const-string v2, "B28685551"
    invoke-static {v2}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
    :try_end
    .catch Ljava/lang/ClassCircularityError; {:try_start .. :try_end} :catch

    move-result-object v0

    :goto_7
    return-void

    :catch
    move-exception v1
    .local v1, "e":Ljava/lang/ClassCircularityError;
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "Caught ClassCircularityError"
    invoke-virtual {v2, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    goto :goto_7
.end method
