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
#

LOCAL_PATH := $(my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libart_fake
LOCAL_INSTALLED_MODULE_STEM := libart.so
LOCAL_SDK_VERSION := 9
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := fake.cc
LOCAL_CFLAGS := -Wall -Werror
LOCAL_SHARED_LIBRARIES := liblog

ifdef TARGET_2ND_ARCH
    LOCAL_MODULE_PATH_32 := $(TARGET_OUT)/fake-libs
    LOCAL_MODULE_PATH_64 := $(TARGET_OUT)/fake-libs64
else
    LOCAL_MODULE_PATH := $(TARGET_OUT)/fake-libs
endif

include $(BUILD_SHARED_LIBRARY)
