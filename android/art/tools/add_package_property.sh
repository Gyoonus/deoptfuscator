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
# Sets the property of an Android package

if [ "$#" -ne 2 ] ; then
  echo "USAGE: sh add_package_property.sh [PACKAGE_NAME] [PROPERTY_SCRIPT_PATH]"
  exit 1
fi
PACKAGE_NAME=$1
PROPERTY_SCRIPT_PATH=$2
PROPERTY_SCRIPT_NAME=`basename $PROPERTY_SCRIPT_PATH`
adb push $PROPERTY_SCRIPT_PATH /data/data/$PACKAGE_NAME/
adb shell chmod o+x /data/data/$PACKAGE_NAME/$PROPERTY_SCRIPT_NAME
adb shell restorecon /data/data/$PACKAGE_NAME/$PROPERTY_SCRIPT_NAME
adb shell setprop wrap.$PACKAGE_NAME /data/data/$PACKAGE_NAME/$PROPERTY_SCRIPT_NAME
