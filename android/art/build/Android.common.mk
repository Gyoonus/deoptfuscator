#
# Copyright (C) 2011 The Android Open Source Project
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

ifndef ART_ANDROID_COMMON_MK
ART_ANDROID_COMMON_MK = true

ART_TARGET_SUPPORTED_ARCH := arm arm64 mips mips64 x86 x86_64
ART_HOST_SUPPORTED_ARCH := x86 x86_64

ifneq ($(HOST_OS),darwin)
  ART_HOST_SUPPORTED_ARCH := x86 x86_64
else
  # Mac OS doesn't support low-4GB allocation in a 64-bit process. So we won't be able to create
  # our heaps.
  ART_HOST_SUPPORTED_ARCH := x86
endif

ART_COVERAGE := false

ifeq ($(ART_COVERAGE),true)
# https://gcc.gnu.org/onlinedocs/gcc/Cross-profiling.html
GCOV_PREFIX := /data/local/tmp/gcov
# GCOV_PREFIX_STRIP is an integer that defines how many levels should be
# stripped off the beginning of the path. We want the paths in $GCOV_PREFIX to
# be relative to $ANDROID_BUILD_TOP so we can just adb pull from the top and not
# have to worry about placing things ourselves.
GCOV_PREFIX_STRIP := $(shell echo $(ANDROID_BUILD_TOP) | grep -o / | wc -l)
GCOV_ENV := GCOV_PREFIX=$(GCOV_PREFIX) GCOV_PREFIX_STRIP=$(GCOV_PREFIX_STRIP)
else
GCOV_ENV :=
endif

ifeq (,$(filter $(TARGET_ARCH),$(ART_TARGET_SUPPORTED_ARCH)))
$(warning unsupported TARGET_ARCH=$(TARGET_ARCH))
endif
ifeq (,$(filter $(HOST_ARCH),$(ART_HOST_SUPPORTED_ARCH)))
$(warning unsupported HOST_ARCH=$(HOST_ARCH))
endif

# Primary vs. secondary
2ND_TARGET_ARCH := $(TARGET_2ND_ARCH)
TARGET_INSTRUCTION_SET_FEATURES := $(DEX2OAT_TARGET_INSTRUCTION_SET_FEATURES)
2ND_TARGET_INSTRUCTION_SET_FEATURES := $($(TARGET_2ND_ARCH_VAR_PREFIX)DEX2OAT_TARGET_INSTRUCTION_SET_FEATURES)
ifdef TARGET_2ND_ARCH
  ifneq ($(filter %64,$(TARGET_ARCH)),)
    ART_PHONY_TEST_TARGET_SUFFIX := 64
    2ND_ART_PHONY_TEST_TARGET_SUFFIX := 32
  else
    # TODO: ???
    $(warning Do not know what to do with this multi-target configuration!)
    ART_PHONY_TEST_TARGET_SUFFIX := 32
    2ND_ART_PHONY_TEST_TARGET_SUFFIX :=
  endif
else
  ifneq ($(filter %64,$(TARGET_ARCH)),)
    ART_PHONY_TEST_TARGET_SUFFIX := 64
    2ND_ART_PHONY_TEST_TARGET_SUFFIX :=
  else
    ART_PHONY_TEST_TARGET_SUFFIX := 32
    2ND_ART_PHONY_TEST_TARGET_SUFFIX :=
  endif
endif

ART_HOST_SHLIB_EXTENSION := $(HOST_SHLIB_SUFFIX)
ART_HOST_SHLIB_EXTENSION ?= .so
ifeq ($(HOST_PREFER_32_BIT),true)
  ART_PHONY_TEST_HOST_SUFFIX := 32
  2ND_ART_PHONY_TEST_HOST_SUFFIX :=
  ART_HOST_ARCH := x86
  2ND_ART_HOST_ARCH :=
  2ND_HOST_ARCH :=
  ART_HOST_OUT_SHARED_LIBRARIES := $(2ND_HOST_OUT_SHARED_LIBRARIES)
  2ND_ART_HOST_OUT_SHARED_LIBRARIES :=
else
  ART_PHONY_TEST_HOST_SUFFIX := 64
  2ND_ART_PHONY_TEST_HOST_SUFFIX := 32
  ART_HOST_ARCH := x86_64
  2ND_ART_HOST_ARCH := x86
  2ND_HOST_ARCH := x86
  ART_HOST_OUT_SHARED_LIBRARIES := $(HOST_OUT_SHARED_LIBRARIES)
  2ND_ART_HOST_OUT_SHARED_LIBRARIES := $(2ND_HOST_OUT_SHARED_LIBRARIES)
endif

endif # ART_ANDROID_COMMON_MK
