#
# Copyright (C) 2018 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

system_stub_dex := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/core_dex_intermediates/classes.dex
$(system_stub_dex): PRIVATE_MIN_SDK_VERSION := 1000
$(system_stub_dex): $(TOPDIR)prebuilts/sdk/system_current/android.jar | $(ZIP2ZIP) $(DX)
	$(transform-classes-d8.jar-to-dex)


oahl_stub_dex := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/oahl_dex_intermediates/classes.dex
$(oahl_stub_dex): PRIVATE_MIN_SDK_VERSION := 1000
$(oahl_stub_dex): $(TOPDIR)prebuilts/sdk/org.apache.http.legacy/org.apache.http.legacy.jar | $(ZIP2ZIP) $(DX)
	$(transform-classes-d8.jar-to-dex)

.PHONY: appcompat

appcompat: $(system_stub_dex) $(oahl_stub_dex) $(HOST_OUT_EXECUTABLES)/veridex \
  ${TARGET_OUT_COMMON_INTERMEDIATES}/PACKAGING/hiddenapi-light-greylist.txt \
  ${TARGET_OUT_COMMON_INTERMEDIATES}/PACKAGING/hiddenapi-dark-greylist.txt \
  ${TARGET_OUT_COMMON_INTERMEDIATES}/PACKAGING/hiddenapi-blacklist.txt
