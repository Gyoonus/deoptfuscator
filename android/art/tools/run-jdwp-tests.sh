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

if [ ! -d libcore ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

source build/envsetup.sh >&/dev/null # for get_build_var, setpaths
setpaths # include platform prebuilt java, javac, etc in $PATH.

if [ -z "$ANDROID_HOST_OUT" ] ; then
  ANDROID_HOST_OUT=${OUT_DIR-$ANDROID_BUILD_TOP/out}/host/linux-x86
fi

java_lib_location="${ANDROID_HOST_OUT}/../common/obj/JAVA_LIBRARIES"
make_target_name="apache-harmony-jdwp-tests-hostdex"

vm_args=""
art="/data/local/tmp/system/bin/art"
art_debugee="sh /data/local/tmp/system/bin/art"
args=$@
debuggee_args="-Xcompiler-option --debuggable"
device_dir="--device-dir=/data/local/tmp"
# We use the art script on target to ensure the runner and the debuggee share the same
# image.
vm_command="--vm-command=$art"
image_compiler_option=""
plugin=""
debug="no"
explicit_debug="no"
verbose="no"
image="-Ximage:/data/art-test/core.art"
with_jdwp_path=""
agent_wrapper=""
vm_args=""
# By default, we run the whole JDWP test suite.
has_specific_test="no"
test="org.apache.harmony.jpda.tests.share.AllTests"
mode="target"
# Use JIT compiling by default.
use_jit=true
variant_cmdline_parameter="--variant=X32"
dump_command="/bin/true"
# Timeout of JDWP test in ms.
#
# Note: some tests expect a timeout to check that *no* reply/event is received for a specific case.
# A lower timeout can save up several minutes when running the whole test suite, especially for
# continuous testing. This value can be adjusted to fit the configuration of the host machine(s).
jdwp_test_timeout=10000

gdb_target=
has_gdb="no"

while true; do
  if [[ "$1" == "--mode=host" ]]; then
    mode="host"
    # Specify bash explicitly since the art script cannot, since it has to run on the device
    # with mksh.
    art="bash ${OUT_DIR-out}/host/linux-x86/bin/art"
    art_debugee="bash ${OUT_DIR-out}/host/linux-x86/bin/art"
    # We force generation of a new image to avoid build-time and run-time classpath differences.
    image="-Ximage:/system/non/existent/vogar.art"
    # We do not need a device directory on host.
    device_dir=""
    # Vogar knows which VM to use on host.
    vm_command=""
    shift
  elif [[ "$1" == "--mode=jvm" ]]; then
    mode="ri"
    make_target_name="apache-harmony-jdwp-tests-host"
    art="$(which java)"
    art_debugee="$(which java)"
    # No need for extra args.
    debuggee_args=""
    # No image. On the RI.
    image=""
    # We do not need a device directory on RI.
    device_dir=""
    # Vogar knows which VM to use on RI.
    vm_command=""
    # We don't care about jit with the RI
    use_jit=false
    shift
  elif [[ $1 == --test-timeout-ms ]]; then
    # Remove the --test-timeout-ms from the arguments.
    args=${args/$1}
    shift
    jdwp_test_timeout=$1
    # Remove the argument
    args=${args/$1}
    shift
  elif [[ $1 == --agent-wrapper ]]; then
    # Remove the --agent-wrapper from the arguments.
    args=${args/$1}
    shift
    agent_wrapper=${agent_wrapper}${1},
    # Remove the argument
    args=${args/$1}
    shift
  elif [[ $1 == -Ximage:* ]]; then
    image="$1"
    shift
  elif [[ "$1" == "--no-jit" ]]; then
    use_jit=false
    # Remove the --no-jit from the arguments.
    args=${args/$1}
    shift
  elif [[ $1 == "--no-debug" ]]; then
    explicit_debug="yes"
    debug="no"
    # Remove the --no-debug from the arguments.
    args=${args/$1}
    shift
  elif [[ $1 == "--debug" ]]; then
    explicit_debug="yes"
    debug="yes"
    # Remove the --debug from the arguments.
    args=${args/$1}
    shift
  elif [[ $1 == "--verbose" ]]; then
    verbose="yes"
    # Remove the --verbose from the arguments.
    args=${args/$1}
    shift
  elif [[ $1 == "--gdbserver" ]]; then
    # Remove the --gdbserver from the arguments.
    args=${args/$1}
    has_gdb="yes"
    shift
    gdb_target=$1
    # Remove the target from the arguments.
    args=${args/$1}
    shift
  elif [[ $1 == "--test" ]]; then
    # Remove the --test from the arguments.
    args=${args/$1}
    shift
    has_specific_test="yes"
    test=$1
    # Remove the test from the arguments.
    args=${args/$1}
    shift
  elif [[ "$1" == "--jdwp-path" ]]; then
    # Remove the --jdwp-path from the arguments.
    args=${args/$1}
    shift
    with_jdwp_path=$1
    # Remove the path from the arguments.
    args=${args/$1}
    shift
  elif [[ "$1" == "" ]]; then
    break
  elif [[ $1 == --variant=* ]]; then
    variant_cmdline_parameter=$1
    shift
  elif [[ $1 == -Xplugin:* ]]; then
    plugin="$1"
    args=${args/$1}
    shift
  else
    shift
  fi
done

if [[ $has_gdb = "yes" ]]; then
  if [[ $explicit_debug = "no" ]]; then
    debug="yes"
  fi
fi

if [[ $mode == "ri" ]]; then
  if [[ "x$with_jdwp_path" != "x" ]]; then
    vm_args="${vm_args} --vm-arg -Djpda.settings.debuggeeAgentArgument=-agentpath:${agent_wrapper}"
    vm_args="${vm_args} --vm-arg -Djpda.settings.debuggeeAgentName=$with_jdwp_path"
  fi
  if [[ "x$image" != "x" ]]; then
    echo "Cannot use -Ximage: with --mode=jvm"
    exit 1
  elif [[ $has_gdb = "yes" ]]; then
    echo "Cannot use --gdbserver with --mode=jvm"
    exit 1
  elif [[ $debug == "yes" ]]; then
    echo "Cannot use --debug with --mode=jvm"
    exit 1
  fi
else
  if [[ "$mode" == "host" ]]; then
    dump_command="/bin/kill -3"
  else
    # TODO It would be great to be able to use this on target too but we need to
    # be able to walk /proc to figure out what the actual child pid is.
    dump_command="/system/bin/true"
    # dump_command="/system/xbin/su root /data/local/tmp/system/bin/debuggerd -b"
  fi
  if [[ $has_gdb = "yes" ]]; then
    if [[ $mode == "target" ]]; then
      echo "Cannot use --gdbserver with --mode=target"
      exit 1
    else
      art_debugee="$art_debugee --gdbserver $gdb_target"
      # The tests absolutely require some timeout. We set a ~2 week timeout since we can kill the
      # test with gdb if it goes on too long.
      jdwp_test_timeout="1000000000"
    fi
  fi
  if [[ "x$with_jdwp_path" != "x" ]]; then
    vm_args="${vm_args} --vm-arg -Djpda.settings.debuggeeAgentArgument=-agentpath:${agent_wrapper}"
    vm_args="${vm_args} --vm-arg -Djpda.settings.debuggeeAgentName=${with_jdwp_path}"
  fi
  vm_args="$vm_args --vm-arg -Xcompiler-option --vm-arg --debuggable"
  # Make sure the debuggee doesn't clean up what the debugger has generated.
  art_debugee="$art_debugee --no-clean"
fi

function jlib_name {
  local path=$1
  local str="classes"
  local suffix="jar"
  if [[ $mode == "ri" ]]; then
    str="javalib"
  fi
  echo "$path/$str.$suffix"
}

# Jar containing all the tests.
test_jar=$(jlib_name "${java_lib_location}/${make_target_name}_intermediates")

if [[ ! -f $test_jar ]]; then
  echo "Before running, you must build jdwp tests and vogar:" \
       "make ${make_target_name} vogar"
  exit 1
fi

# For the host:
#
# If, on the other hand, there is a variant set, use it to modify the art_debugee parameter to
# force the fork to have the same bitness as the controller. This should be fine and not impact
# testing (cross-bitness), as the protocol is always 64-bit anyways (our implementation).
#
# Note: this isn't necessary for the device as the BOOTCLASSPATH environment variable is set there
#       and used as a fallback.
if [[ $mode == "host" ]]; then
  variant=${variant_cmdline_parameter:10}
  if [[ $variant == "x32" || $variant == "X32" ]]; then
    art_debugee="$art_debugee --32"
  elif [[ $variant == "x64" || $variant == "X64" ]]; then
    art_debugee="$art_debugee --64"
  else
    echo "Error, do not understand variant $variant_cmdline_parameter."
    exit 1
  fi
fi

if [[ "$image" != "" ]]; then
  vm_args="$vm_args --vm-arg $image"
fi

if [[ "$plugin" != "" ]]; then
  vm_args="$vm_args --vm-arg $plugin"
fi

if $use_jit; then
  vm_args="$vm_args --vm-arg -Xcompiler-option --vm-arg --compiler-filter=quicken"
  debuggee_args="$debuggee_args -Xcompiler-option --compiler-filter=quicken"
fi

if [[ $mode != "ri" ]]; then
  vm_args="$vm_args --vm-arg -Xusejit:$use_jit"
  debuggee_args="$debuggee_args -Xusejit:$use_jit"
fi

if [[ $debug == "yes" ]]; then
  art="$art -d"
  art_debugee="$art_debugee -d"
  vm_args="$vm_args --vm-arg -XXlib:libartd.so --vm-arg -XX:SlowDebug=true"
fi
if [[ $verbose == "yes" ]]; then
  # Enable JDWP logs in the debuggee.
  art_debugee="$art_debugee -verbose:jdwp"
fi

if [[ $mode != "ri" ]]; then
  toolchain_args="--toolchain d8 --language CUR"
  if [[ "x$with_jdwp_path" == "x" ]]; then
    # Need to enable the internal jdwp implementation.
    art_debugee="${art_debugee} -XjdwpProvider:internal"
  fi
else
  toolchain_args="--toolchain javac --language CUR"
fi

# Run the tests using vogar.
vogar $vm_command \
      $vm_args \
      --verbose \
      $args \
      $device_dir \
      $image_compiler_option \
      --timeout 800 \
      --vm-arg -Djpda.settings.verbose=true \
      --vm-arg -Djpda.settings.timeout=$jdwp_test_timeout \
      --vm-arg -Djpda.settings.waitingTime=$jdwp_test_timeout \
      --vm-arg -Djpda.settings.transportAddress=127.0.0.1:55107 \
      --vm-arg -Djpda.settings.dumpProcess="$dump_command" \
      --vm-arg -Djpda.settings.debuggeeJavaPath="$art_debugee $plugin $image $debuggee_args" \
      --classpath "$test_jar" \
      $toolchain_args \
      $test

vogar_exit_status=$?

echo "Killing stalled dalvikvm processes..."
if [[ $mode == "host" ]]; then
  pkill -9 -f /bin/dalvikvm
else
  adb shell pkill -9 -f /bin/dalvikvm
fi
echo "Done."

exit $vogar_exit_status
