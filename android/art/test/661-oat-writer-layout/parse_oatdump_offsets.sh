#!/bin/bash
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Quick and dirty helper tool to print the sorted oat code offsets from oatdump.
#
# Usage:
#
# oatdump --oat-file=661-oat-writer-layout.odex | $ANDROID_BUILD_TOP/art/test/661-oat-writer-layout/parse_oatdump_offsets.sh
#

found_method=""
tmp_file="$(mktemp)"
while read -r line; do

  if [[ $line == *dex_method_idx=* ]]; then
    found_method=$line
  fi

  if [[ $line == *"code_offset: "* ]]; then
    echo $line $found_method >> "$tmp_file"
  fi
done

sort "$tmp_file"
