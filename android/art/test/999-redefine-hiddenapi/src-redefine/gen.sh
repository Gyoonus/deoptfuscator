#!/bin/bash
#
# Copyright 2018 The Android Open Source Project
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

set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TMP=`mktemp -d`

CLASS "art/Test999"

(cd "$TMP" && javac -d "${TMP}" "$DIR/${CLASS}.java" && d8 --output . "$TMP/${CLASS}.class")

echo '  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode('
base64 "${TMP}/${CLASS}.class" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'
echo '  private static final byte[] DEX_BYTES = Base64.getDecoder().decode('
base64 "${TMP}/classes.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

rm -rf "$TMP"
