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

if [[ ! -d art ]]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

ALL_CONFIGS=(linux-ia32 linux-x64 linux-armv8 linux-armv7 android-armv8 android-armv7)

usage() {
  local config
  local golem_target

  (cat << EOF
  Usage: $(basename "${BASH_SOURCE[0]}") [--golem=<target>] --machine-type=MACHINE_TYPE
                 [--tarball[=<target>.tar.gz]]

  Build minimal art binaries required to run golem benchmarks either
  locally or on the golem servers.

  Creates the \$MACHINE_TYPE binaries in your \$OUT_DIR, and if --tarball was specified,
  it also tars the results of the build together into your <target.tar.gz> file.
  --------------------------------------------------------
  Required Flags:
    --machine-type=MT   Specify the machine type that will be built.

  Optional Flags":
    --golem=<target>    Builds with identical commands that Golem servers use.
    --tarball[=o.tgz]   Tar/gz the results. File name defaults to <machine_type>.tar.gz
    -j<num>             Specify how many jobs to use for parallelism.
    --help              Print this help listing.
    --showcommands      Show commands as they are being executed.
    --simulate          Print commands only, don't execute commands.
EOF
  ) | sed -e 's/^[[:space:]][[:space:]]//g' >&2 # Strip leading whitespace from heredoc.

  echo >&2 "Available machine types:"
  for config in "${ALL_CONFIGS[@]}"; do
    echo >&2 "  $config"
  done

  echo >&2
  echo >&2 "Available Golem targets:"
  while IFS='' read -r golem_target; do
    echo >&2 "  $golem_target"
  done < <("$(thisdir)/env" --list-targets)
}

# Check if $1 element is in array $2
contains_element() {
  local e
  for e in "${@:2}"; do [[ "$e" == "$1" ]] && return 0; done
  return 1
}

# Display a command, but don't execute it, if --showcommands was set.
show_command() {
  if [[ $showcommands == "showcommands" ]]; then
    echo "$@"
  fi
}

# Execute a command, displaying it if --showcommands was set.
# If --simulate is used, command is not executed.
execute() {
  show_command "$@"
  execute_noshow "$@"
}

# Execute a command unless --simulate was used.
execute_noshow() {
  if [[ $simulate == "simulate" ]]; then
    return 0
  fi

  local prog="$1"
  shift
  "$prog" "$@"
}

# Export environment variable, echoing it to screen.
setenv() {
  local name="$1"
  local value="$2"

  export $name="$value"
  echo export $name="$value"
}

# Export environment variable, echoing $3 to screen ($3 is meant to be unevaluated).
setenv_escape() {
  local name="$1"
  local value="$2"
  local escaped_value="$3"

  export $name="$value"
  echo export $name="$escaped_value"
}

log_usage_error() {
  echo >&2 "ERROR: " "$@"
  echo >&2 "       See --help for the correct usage information."
  exit 1
}

log_fatal() {
  echo >&2 "FATAL: " "$@"
  exit 2
}

# Get the directory of this script.
thisdir() {
  (\cd "$(dirname "${BASH_SOURCE[0]}")" && pwd )
}

# Get the path to the top of the Android source tree.
gettop() {
  if [[ "x$ANDROID_BUILD_TOP" != "x" ]]; then
    echo "$ANDROID_BUILD_TOP";
  else
    echo "$(thisdir)/../../.."
  fi
}

# Get a build variable from the Android build system.
get_build_var() {
  local varname="$1"

  # include the desired target product/build-variant
  # which won't be set in our env if neither we nor the user first executed
  # source build/envsetup.sh (e.g. if simulating from a fresh shell).
  local extras
  [[ -n $target_product ]] && extras+=" TARGET_PRODUCT=$target_product"
  [[ -n $target_build_variant ]] && extras+=" TARGET_BUILD_VARIANT=$target_build_variant"

  # call dumpvar from the build system.
  (\cd "$(gettop)"; env $extras build/soong/soong_ui.bash --dumpvar-mode $varname)
}

# Defaults from command-line.

mode=""  # blank or 'golem' if --golem was specified.
golem_target="" # --golem=$golem_target
config="" # --machine-type=$config
j_arg=""
showcommands=""
simulate=""
make_tarball=""
tarball=""

# Parse command line arguments

while [[ "$1" != "" ]]; do
  case "$1" in
    --help)
      usage
      exit 1
      ;;
    --golem=*)
      mode="golem"
      golem_target="${1##--golem=}"

      if [[ "x$golem_target" == x ]]; then
        log_usage_error "Missing --golem target type."
      fi

      shift
      ;;
    --machine-type=*)
      config="${1##--machine-type=}"
      if ! contains_element "$config" "${ALL_CONFIGS[@]}"; then
        log_usage_error "Invalid --machine-type value '$config'"
      fi
      shift
      ;;
    --tarball)
      tarball="" # reuse the machine type name.
      make_tarball="make_tarball"
      shift
      ;;
    --tarball=*)
      tarball="${1##--tarball=}"
      make_tarball="make_tarball"
      shift
      ;;
    -j*)
      j_arg="$1"
      shift
      ;;
    --showcommands)
      showcommands="showcommands"
      shift
      ;;
    --simulate)
      simulate="simulate"
      shift
      ;;
    *)
      log_usage_error "Unknown options $1"
      ;;
  esac
done

###################################
###################################
###################################

if [[ -z $config ]]; then
  log_usage_error "--machine-type option is required."
fi

# --tarball defaults to the --machine-type value with .tar.gz.
tarball="${tarball:-$config.tar.gz}"

target_product="$TARGET_PRODUCT"
target_build_variant="$TARGET_BUILD_VARIANT"

# If not using --golem, use whatever the user had lunch'd prior to this script.
if [[ $mode == "golem" ]]; then
  # This section is intended solely to be executed by a golem build server.

  target_build_variant=eng
  case "$config" in
    *-armv7)
      target_product="arm_krait"
      ;;
    *-armv8)
      target_product="armv8"
      ;;
    *)
      target_product="sdk"
      ;;
  esac

  if [[ $target_product = arm* ]]; then
    # If using the regular manifest, e.g. 'master'
    # The lunch command for arm will assuredly fail because we don't have device/generic/art.
    #
    # Print a human-readable error message instead of trying to lunch and failing there.
    if ! [[ -d "$(gettop)/device/generic/art" ]]; then
      log_fatal "Missing device/generic/art directory. Perhaps try master-art repo manifest?\n" \
                "       Cannot build ARM targets (arm_krait, armv8) for Golem." >&2
    fi
    # We could try to keep on simulating but it seems brittle because we won't have the proper
    # build variables to output the right strings.
  fi

  # Get this particular target's environment variables (e.g. ART read barrier on/off).
  source "$(thisdir)"/env "$golem_target" || exit 1

  lunch_target="$target_product-$target_build_variant"

  execute 'source' build/envsetup.sh
  # Build generic targets (as opposed to something specific like aosp_angler-eng).
  execute lunch "$lunch_target"
  setenv JACK_SERVER false
  setenv_escape JACK_REPOSITORY "$PWD/prebuilts/sdk/tools/jacks" '$PWD/prebuilts/sdk/tools/jacks'
  # Golem uses master-art repository which is missing a lot of other libraries.
  setenv SOONG_ALLOW_MISSING_DEPENDENCIES true
  # Golem may be missing tools such as javac from its path.
  setenv_escape PATH "/usr/lib/jvm/java-8-openjdk-amd64/bin/:$PATH" '/usr/lib/jvm/java-8-openjdk-amd64/bin/:$PATH'
else
  # Look up the default variables from the build system if they weren't set already.
  [[ -z $target_product ]] && target_product="$(get_build_var TARGET_PRODUCT)"
  [[ -z $target_build_variant ]] && target_build_variant="$(get_build_var TARGET_BUILD_VARIANT)"
fi

# Defaults for all machine types.
make_target="build-art-target-golem"
out_dir="out/x86_64"
root_dir_var="PRODUCT_OUT"
strip_symbols=false
bit64_suffix=""
tar_directories=(system data/art-test)

# Per-machine type overrides
if [[ $config == linux-arm* ]]; then
    setenv ART_TARGET_LINUX true
fi

case "$config" in
  linux-ia32|linux-x64)
    root_dir_var="HOST_OUT"
    # Android strips target builds automatically, but not host builds.
    strip_symbols=true
    make_target="build-art-host-golem"

    if [[ $config == linux-ia32 ]]; then
      out_dir="out/x86"
      setenv HOST_PREFER_32_BIT true
    else
      bit64_suffix="64"
    fi

    tar_directories=(bin framework usr lib${bit64_suffix})
    ;;
  *-armv8)
    bit64_suffix="64"
    ;;
  *-armv7)
    ;;
  *)
    log_fatal "Unsupported machine-type '$config'"
esac

# Golem benchmark run commands expect a certain $OUT_DIR to be set,
# so specify it here.
#
# Note: It is questionable if we want to customize this since users
# could alternatively probably use their own build directly (and forgo this script).
setenv OUT_DIR "$out_dir"
root_dir="$(get_build_var "$root_dir_var")"

if [[ $mode == "golem" ]]; then
  # For golem-style running only.
  # Sets the DT_INTERP to this path in every .so we can run the
  # non-system version of dalvikvm with our own copies of the dependencies (e.g. our own libc++).
  if [[ $config == android-* ]]; then
    # TODO: the linker can be relative to the binaries
    # (which is what we do for linux-armv8 and linux-armv7)
    golem_run_path="/data/local/tmp/runner/"
  else
    golem_run_path=""
  fi

  # Only do this for target builds. Host doesn't need this.
  if [[ $config == *-arm* ]]; then
    setenv CUSTOM_TARGET_LINKER "${golem_run_path}${root_dir}/system/bin/linker${bit64_suffix}"
  fi
fi

#
# Main command execution below here.
# (everything prior to this just sets up environment variables,
#  and maybe calls lunch).
#

execute build/soong/soong_ui.bash --make-mode "${j_arg}" "${make_target}"

if $strip_symbols; then
  # Further reduce size by stripping symbols.
  execute_noshow strip $root_dir/bin/* || true
  show_command strip $root_dir/bin/'*'  '|| true'
  execute_noshow strip $root_dir/lib${bit64_suffix}/'*'
  show_command strip $root_dir/lib${bit64_suffix}/'*'
fi

if [[ "$make_tarball" == "make_tarball" ]]; then
  # Create a tarball which is required for the golem build resource.
  # (In particular, each golem benchmark's run commands depend on a list of resource files
  #  in order to have all the files it needs to actually execute,
  #  and this tarball would satisfy that particular target+machine-type's requirements).
  dirs_rooted=()
  for tar_dir in "${tar_directories[@]}"; do
    dirs_rooted+=("$root_dir/$tar_dir")
  done

  execute tar -czf "${tarball}" "${dirs_rooted[@]}" --exclude .git --exclude .gitignore
  tar_result=$?
  if [[ $tar_result -ne 0 ]]; then
    [[ -f $tarball ]] && rm $tarball
  fi

  show_command '[[ $? -ne 0 ]] && rm' "$tarball"
fi

