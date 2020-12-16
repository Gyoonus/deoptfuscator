#!/system/bin/sh
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
# NOTE: This script is used by add_package_property.sh and not meant to be executed directly
#
# This script contains the property and the options required to log poisoned
# memory accesses (found in logcat)
ASAN_OPTIONS=halt_on_error=0:verbosity=0:print_legend=0:print_full_thread_history=0:print_stats=0:print_summary=0:suppress_equal_pcs=0:fast_unwind_on_fatal=1 asanwrapper $@
