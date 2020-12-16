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
# Note: Requires $ANDROID_BUILD_TOP/build/envsetup.sh to have been run.
#
# This script takes in a logcat containing Sanitizer traces and outputs several
# files, prints information regarding the traces, and plots information as well.
ALL_PIDS=false
USE_TEMP=true
DO_REDO=false
PACKAGE_NAME=""
BAKSMALI_NUM=0
# EXACT_ARG and MIN_ARG are passed to prune_sanitizer_output.py
EXACT_ARG=""
MIN_ARG=()
OFFSET_ARGS=()
TIME_ARGS=()
usage() {
  echo "Usage: $0 [options] [LOGCAT_FILE] [CATEGORIES...]"
  echo "    -a"
  echo "        Forces all pids associated with registered dex"
  echo "        files in the logcat to be processed."
  echo "        default: only the last pid is processed"
  echo
  echo "    -b  [DEX_FILE_NUMBER]"
  echo "        Outputs data for the specified baksmali"
  echo "        dump if -p is provided."
  echo "        default: first baksmali dump in order of dex"
  echo "          file registration"
  echo
  echo "    -d  OUT_DIRECTORY"
  echo "        Puts all output in specified directory."
  echo "        If not given, output will be put in a local"
  echo "        temp folder which will be deleted after"
  echo "        execution."
  echo
  echo "    -e"
  echo "        All traces will have exactly the same number"
  echo "        of categories which is specified by either"
  echo "        the -m argument or by prune_sanitizer_output.py"
  echo
  echo "    -f"
  echo "        Forces redo of all commands even if output"
  echo "        files exist. Steps are skipped if their output"
  echo "        exist already and this is not enabled."
  echo
  echo "    -m  [MINIMUM_CALLS_PER_TRACE]"
  echo "        Filters out all traces that do not have"
  echo "        at least MINIMUM_CALLS_PER_TRACE lines."
  echo "        default: specified by prune_sanitizer_output.py"
  echo
  echo "    -o  [OFFSET],[OFFSET]"
  echo "        Filters out all Dex File offsets outside the"
  echo "        range between provided offsets. 'inf' can be"
  echo "        provided for infinity."
  echo "        default: 0,inf"
  echo
  echo "    -p  [PACKAGE_NAME]"
  echo "        Using the package name, uses baksmali to get"
  echo "        a dump of the Dex File format for the package."
  echo
  echo "    -t  [TIME_OFFSET],[TIME_OFFSET]"
  echo "        Filters out all time offsets outside the"
  echo "        range between provided offsets. 'inf' can be"
  echo "        provided for infinity."
  echo "        default: 0,inf"
  echo
  echo "    CATEGORIES are words that are expected to show in"
  echo "       a large subset of symbolized traces. Splits"
  echo "       output based on each word."
  echo
  echo "    LOGCAT_FILE is the piped output from adb logcat."
  echo
}


while getopts ":ab:d:efm:o:p:t:" opt ; do
case ${opt} in
  a)
    ALL_PIDS=true
    ;;
  b)
    if ! [[ "$OPTARG" -eq "$OPTARG" ]]; then
      usage
      exit
    fi
    BAKSMALI_NUM=$OPTARG
    ;;
  d)
    USE_TEMP=false
    OUT_DIR=$OPTARG
    ;;
  e)
    EXACT_ARG='-e'
    ;;
  f)
    DO_REDO=true
    ;;
  m)
    if ! [[ "$OPTARG" -eq "$OPTARG" ]]; then
      usage
      exit
    fi
    MIN_ARG=( "-m" "$OPTARG" )
    ;;
  o)
    set -f
    old_ifs=$IFS
    IFS=","
    OFFSET_ARGS=( $OPTARG )
    if [[ "${#OFFSET_ARGS[@]}" -ne 2 ]]; then
      usage
      exit
    fi
    OFFSET_ARGS=( "--offsets" "${OFFSET_ARGS[@]}" )
    IFS=$old_ifs
    set +f
    ;;
  t)
    set -f
    old_ifs=$IFS
    IFS=","
    TIME_ARGS=( $OPTARG )
    if [[ "${#TIME_ARGS[@]}" -ne 2 ]]; then
      usage
      exit
    fi
    TIME_ARGS=( "--times" "${TIME_ARGS[@]}" )
    IFS=$old_ifs
    set +f
    ;;
  p)
    PACKAGE_NAME=$OPTARG
    ;;
  \?)
    usage
    exit
esac
done
shift $((OPTIND -1))

if [[ $# -lt 1 ]]; then
  usage
  exit
fi

LOGCAT_FILE=$1
NUM_CAT=$(($# - 1))

# Use a temp directory that will be deleted
if [[ $USE_TEMP = true ]]; then
  OUT_DIR=$(mktemp -d --tmpdir="$PWD")
  DO_REDO=true
fi

if [[ ! -d "$OUT_DIR" ]]; then
  mkdir "$OUT_DIR"
  DO_REDO=true
fi

# Note: Steps are skipped if their output exists until -f flag is enabled
echo "Output folder: $OUT_DIR"
# Finds the lines matching pattern criteria and prints out unique instances of
# the 3rd word (PID)
unique_pids=( $(awk '/RegisterDexFile:/ && !/zygote/ {if(!a[$3]++) print $3}' \
  "$LOGCAT_FILE") )
echo "List of pids: ${unique_pids[@]}"
if [[ $ALL_PIDS = false ]]; then
  unique_pids=( ${unique_pids[-1]} )
fi

for pid in "${unique_pids[@]}"
do
  echo
  echo "Current pid: $pid"
  echo
  pid_dir=$OUT_DIR/$pid
  if [[ ! -d "$pid_dir" ]]; then
    mkdir "$pid_dir"
    DO_REDO[$pid]=true
  fi

  intermediates_dir=$pid_dir/intermediates
  results_dir=$pid_dir/results
  logcat_pid_file=$pid_dir/logcat

  if [[ ! -f "$logcat_pid_file" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    awk "{if(\$3 == $pid) print \$0}" "$LOGCAT_FILE" > "$logcat_pid_file"
  fi

  if [[ ! -d "$intermediates_dir" ]]; then
    mkdir "$intermediates_dir"
    DO_REDO[$pid]=true
  fi

  # Step 1 - Only output lines related to Sanitizer
  # Folder that holds all file output
  asan_out=$intermediates_dir/asan_output
  if [[ ! -f "$asan_out" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    echo "Extracting ASAN output"
    grep "app_process64" "$logcat_pid_file" > "$asan_out"
  else
    echo "Skipped: Extracting ASAN output"
  fi

  # Step 2 - Only output lines containing Dex File Start Addresses
  dex_start=$intermediates_dir/dex_start
  if [[ ! -f "$dex_start" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    echo "Extracting Start of Dex File(s)"
    if [[ ! -z "$PACKAGE_NAME" ]]; then
      awk '/RegisterDexFile:/ && /'"$PACKAGE_NAME"'/ && /\/data\/app/' \
        "$logcat_pid_file" > "$dex_start"
    else
      grep "RegisterDexFile:" "$logcat_pid_file" > "$dex_start"
    fi
  else
    echo "Skipped: Extracting Start of Dex File(s)"
  fi

  # Step 3 - Clean Sanitizer output from Step 2 since logcat cannot
  # handle large amounts of output.
  asan_out_filtered=$intermediates_dir/asan_output_filtered
  if [[ ! -f "$asan_out_filtered" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    echo "Filtering/Cleaning ASAN output"
    python "$ANDROID_BUILD_TOP"/art/tools/runtime_memusage/prune_sanitizer_output.py \
      "$EXACT_ARG" "${MIN_ARG[@]}" -d "$intermediates_dir" "$asan_out"
  else
    echo "Skipped: Filtering/Cleaning ASAN output"
  fi

  # Step 4 - Retrieve symbolized stack traces from Step 3 output
  sym_filtered=$intermediates_dir/sym_filtered
  if [[ ! -f "$sym_filtered" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    echo "Retrieving symbolized traces"
    "$ANDROID_BUILD_TOP"/development/scripts/stack "$asan_out_filtered" \
      > "$sym_filtered"
  else
    echo "Skipped: Retrieving symbolized traces"
  fi

  # Step 4.5 - Obtain Dex File Format of dex file related to package
  filtered_dex_start=$intermediates_dir/filtered_dex_start
  baksmali_dmp_ctr=0
  baksmali_dmp_prefix=$intermediates_dir"/baksmali_dex_file_"
  baksmali_dmp_files=( $baksmali_dmp_prefix* )
  baksmali_dmp_arg="--dex-file "${baksmali_dmp_files[$BAKSMALI_NUM]}
  apk_dex_files=( )
  if [[ ! -f "$baksmali_dmp_prefix""$BAKSMALI_NUM" ]] || \
     [[ ! -f "$filtered_dex_start" ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    if [[ ! -z "$PACKAGE_NAME" ]]; then
      DO_REDO[$pid]=true
      # Extracting Dex File path on device from Dex File related to package
      apk_directory=$(dirname "$(tail -n1 "$dex_start" | awk "{print \$8}")")
      for dex_file in $(awk "{print \$8}" "$dex_start"); do
        apk_dex_files+=( $(basename "$dex_file") )
      done
      apk_oat_files=$(adb shell find "$apk_directory" -name "*.?dex" -type f \
        2> /dev/null)
      # Pulls the .odex and .vdex files associated with the package
      for apk_file in $apk_oat_files; do
        base_name=$(basename "$apk_file")
        adb pull "$apk_file" "$intermediates_dir/base.${base_name#*.}"
      done
      oatdump --oat-file="$intermediates_dir"/base.odex \
        --export-dex-to="$intermediates_dir" --output=/dev/null
      for dex_file in "${apk_dex_files[@]}"; do
        exported_dex_file=$intermediates_dir/$dex_file"_export.dex"
        baksmali_dmp_out="$baksmali_dmp_prefix""$((baksmali_dmp_ctr++))"
        baksmali -JXmx1024M dump "$exported_dex_file" \
          > "$baksmali_dmp_out" 2> "$intermediates_dir"/error
        if ! [[ -s "$baksmali_dmp_out" ]]; then
          rm "$baksmali_dmp_prefix"*
          baksmali_dmp_arg=""
          echo "Failed to retrieve Dex File format"
          break
        fi
      done
      baksmali_dmp_files=( "$baksmali_dmp_prefix"* )
      baksmali_dmp_arg="--dex-file "${baksmali_dmp_files[$BAKSMALI_NUM]}
      # Gets the baksmali dump associated with BAKSMALI_NUM
      awk "NR == $((BAKSMALI_NUM + 1))" "$dex_start" > "$filtered_dex_start"
      results_dir=$results_dir"_"$BAKSMALI_NUM
      echo "Skipped: Retrieving Dex File format from baksmali; no package given"
    else
      cp "$dex_start" "$filtered_dex_start"
      baksmali_dmp_arg=""
    fi
  else
    awk "NR == $((BAKSMALI_NUM + 1))" "$dex_start" > "$filtered_dex_start"
    results_dir=$results_dir"_"$BAKSMALI_NUM
    echo "Skipped: Retrieving Dex File format from baksmali"
  fi

  if [[ ! -d "$results_dir" ]]; then
    mkdir "$results_dir"
    DO_REDO[$pid]=true
  fi

  # Step 5 - Using Steps 2, 3, 4 outputs in order to output graph data
  # and trace data
  # Only the category names are needed for the commands giving final output
  shift
  time_output=($results_dir/time_output_*.dat)
  if [[ ! -e ${time_output[0]} ]] || \
     [[ "${DO_REDO[$pid]}" = true ]] || \
     [[ $DO_REDO = true ]]; then
    DO_REDO[$pid]=true
    echo "Creating Categorized Time Table"
    baksmali_dmp_args=( $baksmali_dmp_arg )
    python "$ANDROID_BUILD_TOP"/art/tools/runtime_memusage/symbol_trace_info.py \
      -d "$results_dir" "${OFFSET_ARGS[@]}" "${baksmali_dmp_args[@]}" \
      "${TIME_ARGS[@]}" "$asan_out_filtered" "$sym_filtered" \
      "$filtered_dex_start" "$@"
  else
    echo "Skipped: Creating Categorized Time Table"
  fi

  # Step 6 - Use graph data from Step 5 to plot graph
  # Contains the category names used for legend of gnuplot
  plot_cats="\"Uncategorized $*\""
  package_string=""
  dex_name=""
  if [[ ! -z "$PACKAGE_NAME" ]]; then
    package_string="Package name: $PACKAGE_NAME "
  fi
  if [[ ! -z "$baksmali_dmp_arg" ]]; then
    dex_file_path="$(awk "{print \$8}" "$filtered_dex_start" | tail -n1)"
    dex_name="Dex File name: $(basename "$dex_file_path") "
  fi
  echo "Plotting Categorized Time Table"
  # Plots the information from logcat
  gnuplot --persist -e \
    'filename(n) = sprintf("'"$results_dir"'/time_output_%d.dat", n);
     catnames = '"$plot_cats"';
     set title "'"$package_string""$dex_name"'PID: '"$pid"'";
     set xlabel "Time (milliseconds)";
     set ylabel "Dex File Offset (bytes)";
     plot for [i=0:'"$NUM_CAT"'] filename(i) using 1:2 title word(catnames, i + 1);'

  if [[ $USE_TEMP = true ]]; then
    echo "Removing temp directory and files"
    rm -rf "$OUT_DIR"
  fi
done
