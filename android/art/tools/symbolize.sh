#!/bin/sh
# Copyright (C) 2014 The Android Open Source Project
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
#
# Symbolize oat files from the dalvik cache of a device.
#
# By default, pulls everything from the dalvik cache. A simple yes/no/quit prompt for each file can
# be requested by giving "--interactive" as a parameter.

INTERACTIVE="no"
if [ "x$1" = "x--interactive" ] ; then
  INTERACTIVE="yes"
  shift
fi

# Pull the file from the device and symbolize it.
function one() {
  echo $1 $2
  if [ "x$INTERACTIVE" = "xyes" ] ; then
    echo -n "What to do? [Y/n/q] "
    read -e input
    if [ "x$input" = "xn" ] ; then
      return
    fi
    if [ "x$input" = "xq" ] ; then
      exit 0
    fi
  fi
  adb pull $1/$2 /tmp || exit 1
  mkdir -p $OUT/symbols/$1
  oatdump --symbolize=/tmp/$2 --output=$OUT/symbols/$1/$2
}

# adb shell find seems to output in DOS format (CRLF), which messes up scripting
function adbshellstrip() {
  adb shell $@ | sed 's/\r$//'
}

# Search in all of /data on device.
function all() {
  FILES=$(adbshellstrip find /data -name "'*.oat'" -o -name "'*.dex'" -o -name "'*.odex'")
  for FILE in $FILES ; do
    DIR=$(dirname $FILE)
    NAME=$(basename $FILE)
    one $DIR $NAME
  done
}

if [ "x$1" = "x" ] ; then
  # No further arguments, iterate over all oat files on device.
  all
else
  # Take the parameters as a list of paths on device.
  while (($#)); do
    DIR=$(dirname $1)
    NAME=$(basename $1)
    one $DIR $NAME
    shift
  done
fi
