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

.class public LB30458218;
.super Ljava/io/InterruptedIOException;

.method public static run()V
    .registers 2
    new-instance v0, LB30458218;
    invoke-direct {v0}, LB30458218;-><init>()V

    # IGET used to wrongly cache 'InterruptedIOException' class under the key 'LB30458218;'
    iget v1, v0, LB30458218;->bytesTransferred:I

    return-void
.end method
