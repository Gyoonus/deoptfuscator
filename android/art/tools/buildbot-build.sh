#!/bin/bash
#
# Copyright (C) 2015 The Android Open Source Project
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

if [ ! -d art ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

source build/envsetup.sh >&/dev/null # for get_build_var

# Logic for setting out_dir from build/make/core/envsetup.mk:
if [[ -z $OUT_DIR ]]; then
  if [[ -z $OUT_DIR_COMMON_BASE ]]; then
    out_dir=out
  else
    out_dir=${OUT_DIR_COMMON_BASE}/${PWD##*/}
  fi
else
  out_dir=${OUT_DIR}
fi

using_jack=$(get_build_var ANDROID_COMPILE_WITH_JACK)

java_libraries_dir=${out_dir}/target/common/obj/JAVA_LIBRARIES
common_targets="vogar core-tests apache-harmony-jdwp-tests-hostdex jsr166-tests mockito-target"
mode="target"
j_arg="-j$(nproc)"
showcommands=
make_command=

while true; do
  if [[ "$1" == "--host" ]]; then
    mode="host"
    shift
  elif [[ "$1" == "--target" ]]; then
    mode="target"
    shift
  elif [[ "$1" == -j* ]]; then
    j_arg=$1
    shift
  elif [[ "$1" == "--showcommands" ]]; then
    showcommands="showcommands"
    shift
  elif [[ "$1" == "" ]]; then
    break
  else
    echo "Unknown options $@"
    exit 1
  fi
done

if [[ $using_jack == "true" ]]; then
  common_targets="$common_targets ${out_dir}/host/linux-x86/bin/jack"
fi

# Allow to build successfully in master-art.
extra_args=SOONG_ALLOW_MISSING_DEPENDENCIES=true

if [[ $mode == "host" ]]; then
  make_command="make $j_arg $extra_args $showcommands build-art-host-tests $common_targets"
  make_command+=" dx-tests"
  mode_suffix="-host"
elif [[ $mode == "target" ]]; then
  make_command="make $j_arg $extra_args $showcommands build-art-target-tests $common_targets"
  make_command+=" libjavacrypto-target libnetd_client-target linker toybox toolbox sh"
  make_command+=" ${out_dir}/host/linux-x86/bin/adb libstdc++ "
  make_command+=" ${out_dir}/target/product/${TARGET_PRODUCT}/system/etc/public.libraries.txt"
  mode_suffix="-target"
fi

mode_specific_libraries="libjavacoretests libjdwp libwrapagentproperties libwrapagentpropertiesd"
for LIB in ${mode_specific_libraries} ; do
  make_command+=" $LIB${mode_suffix}"
done



echo "Executing $make_command"
bash -c "$make_command"
