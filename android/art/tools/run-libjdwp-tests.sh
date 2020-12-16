#!/bin/bash
#
# Copyright (C) 2017 The Android Open Source Project
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

if [[ ! -d libcore ]];  then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

if [[ `uname` != 'Linux' ]];  then
  echo "Script cannot be run on $(uname). It is Linux only."
  exit 2
fi

args=("$@")
debug="no"
has_variant="no"
has_mode="no"
mode="target"
has_timeout="no"

while true; do
  if [[ $1 == "--debug" ]]; then
    debug="yes"
    shift
  elif [[ $1 == --test-timeout-ms ]]; then
    has_timeout="yes"
    shift
    shift
  elif [[ "$1" == "--mode=jvm" ]]; then
    has_mode="yes"
    mode="ri"
    shift
  elif [[ "$1" == --mode=host ]]; then
    has_mode="yes"
    mode="host"
    shift
  elif [[ $1 == --variant=* ]]; then
    has_variant="yes"
    shift
  elif [[ "$1" == "" ]]; then
    break
  else
    shift
  fi
done

if [[ "$has_mode" = "no" ]];  then
  args+=(--mode=device)
fi

if [[ "$has_variant" = "no" ]];  then
  args+=(--variant=X32)
fi

if [[ "$has_timeout" = "no" ]]; then
  # Double the timeout to 20 seconds
  args+=(--test-timeout-ms)
  args+=(20000)
fi

# We don't use full paths since it is difficult to determine them for device
# tests and not needed due to resolution rules of dlopen.
if [[ "$debug" = "yes" ]]; then
  args+=(-Xplugin:libopenjdkjvmtid.so)
else
  args+=(-Xplugin:libopenjdkjvmti.so)
fi

expect_path=$PWD/art/tools/external_oj_libjdwp_art_failures.txt
function verbose_run() {
  echo "$@"
  env "$@"
}

verbose_run ./art/tools/run-jdwp-tests.sh \
            "${args[@]}"                  \
            --jdwp-path "libjdwp.so"      \
            --expectations "$expect_path"
