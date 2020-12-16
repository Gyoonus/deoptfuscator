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

source build/envsetup.sh >&/dev/null # for get_build_var, setpaths
setpaths # include platform prebuilt java, javac, etc in $PATH.

if [[ `uname` != 'Linux' ]];  then
  echo "Script cannot be run on $(uname). It is Linux only."
  exit 2
fi

jdwp_path=${ANDROID_JAVA_HOME}/jre/lib/amd64/libjdwp.so
if [[ ! -f $jdwp_path ]];  then
  echo "Unable to find prebuilts libjdwp.so! Did the version change from jdk8?"
  exit 3
fi

args=("$@")
debug="no"
has_variant="no"
has_mode="no"

while true; do
  if [[ $1 == "--debug" ]]; then
    debug="yes"
    shift
  elif [[ "$1" == --mode=* ]]; then
    has_mode="yes"
    if [[ $1 != "--mode=host" ]];  then
      # Just print out an actually helpful error message.
      echo "Only host tests can be run against prebuilt libjdwp"
      exit 4
    fi
    shift
  elif [[ $1 == --variant=* ]]; then
    has_variant="yes"
    if [[ $1 != "--variant=x64" ]] && [[ $1 != "--variant=X64" ]];  then
      # Just print out an actually helpful error message.
      echo "Only 64bit runs can be tested against the prebuilt libjdwp!"
      exit 5
    fi
    shift
  elif [[ "$1" == "" ]]; then
    break
  else
    shift
  fi
done

if [[ "$has_mode" = "no" ]];  then
  args+=(--mode=host)
fi

if [[ "$has_variant" = "no" ]];  then
  args+=(--variant=X64)
fi

wrapper_name=""
plugin=""
if [[ "$debug" = "yes" ]];  then
  wrapper_name=libwrapagentpropertiesd
  plugin="$ANDROID_HOST_OUT/lib64/libopenjdkjvmtid.so"
else
  wrapper_name=libwrapagentproperties
  plugin="$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so"
fi
wrapper=$ANDROID_HOST_OUT/lib64/${wrapper_name}.so

if [[ ! -f $wrapper ]];  then
  echo "need to build $wrapper to run prebuild-libjdwp-tests!"
  echo "m -j40 ${wrapper/.so/}"
  exit 6
fi

if [[ ! -f $plugin ]];  then
  echo "jvmti plugin not built!"
  exit 7
fi

props_path=$PWD/art/tools/libjdwp-compat.props
expect_path=$PWD/art/tools/prebuilt_libjdwp_art_failures.txt

function verbose_run() {
  echo "$@"
  env "$@"
}

verbose_run LD_LIBRARY_PATH="$(dirname $jdwp_path):$LD_LIBRARY_PATH" \
            ./art/tools/run-jdwp-tests.sh                            \
            "${args[@]}"                                             \
            "-Xplugin:$plugin"                                       \
            --agent-wrapper "${wrapper}"="${props_path}"             \
            --jdwp-path "$jdwp_path"                                 \
            --expectations "$expect_path"
