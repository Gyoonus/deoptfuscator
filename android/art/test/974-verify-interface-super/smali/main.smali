# Copyright 2016 The Android Open Source Project
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

.class public LMain;

.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
    .locals 4
    const-string v0, "OK. No exception before invoke!"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    :try_start
        invoke-static {}, LBase;->run()V
        const-string v0, "FAIL: no exception!"
        sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
        invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
        goto :end
    :try_end
    .catch Ljava/lang/LinkageError; {:try_start .. :try_end} :end
    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :error
    :error
        move-exception v0
        sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
        invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
        invoke-virtual {v0}, Ljava/lang/Throwable;->printStackTrace()V
    :end
    return-void
.end method
