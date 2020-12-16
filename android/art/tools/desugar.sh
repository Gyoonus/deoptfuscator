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

#
# Calls desugar.jar with the --bootclasspath_entry values passed in automatically.
# (This avoids having to manually set a boot class path).
#
#
# Script-specific args:
#   --mode=[host|target]: Select between host or target bootclasspath (default target).
#   --core-only:          Use only "core" bootclasspath (e.g. do not include framework).
#   --show-commands:      Print the desugar command being executed.
#   --help:               Print above list of args.
#
# All other args are forwarded to desugar.jar
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP=$DIR/../..

pushd "$TOP" >/dev/null # back to android root.

out=${OUT_DIR:-out}
desugar_jar=$out/host/linux-x86/framework/desugar.jar

if ! [[ -f $desugar_jar ]]; then
  echo "Error: Missing $desugar_jar; did you do a build?" >&2
  exit 1
fi

desugar_jar=$(readlink -f "$desugar_jar") # absolute path to desugar jar
popd >/dev/null

bootjars_args=
mode=target
showcommands=n
while true; do
  case $1 in
    --help)
      echo "Usage: $0 [--mode=host|target] [--core-only] [--show-commands] <desugar args>"
      exit 0
      ;;
    --mode=host)
      bootjars_args="$bootjars_args --host"
      ;;
    --mode=target)
      bootjars_args="$bootjars_args --target"
      ;;
    --mode=*)
      echo "Unsupported $0 usage with --mode=$1" >&2
      exit 1
      ;;
    --core-only)
      bootjars_args="$bootjars_args --core"
      ;;
    --show-commands)
      showcommands=y
      ;;
    *)
      break
      ;;
  esac
  shift
done

desugar_args=(--min_sdk_version=10000)
boot_class_path_list=$($TOP/art/tools/bootjars.sh $bootjars_args --path)

for path in $boot_class_path_list; do
  desugar_args+=(--bootclasspath_entry="$path")
done

if [[ ${#desugar_args[@]} -eq 0 ]]; then
  echo "FATAL: Missing bootjars.sh file path list" >&2
  exit 1
fi

if [[ $showcommands == y ]]; then
  echo java -jar "$desugar_jar" "${desugar_args[@]}" "$@"
fi

java -jar "$desugar_jar" "${desugar_args[@]}" "$@"
