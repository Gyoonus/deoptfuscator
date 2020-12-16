#!/bin/bash
#
# Copyright (C) 2016 The Android Open Source Project
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

# We push art and its dependencies to '/data/local/tmp', but the 'stack'
# script expect things to be in '/'. So we just remove the
# '/data/local/tmp' prefix.
adb logcat -d | sed 's,/data/local/tmp,,g' | development/scripts/stack

# Always return 0 to avoid having the buildbot complain about wrong stacks.
exit 0
