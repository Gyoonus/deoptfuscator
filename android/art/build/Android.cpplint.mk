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

include art/build/Android.common_build.mk

# Use upstream cpplint (toolpath from .repo/manifests/GLOBAL-PREUPLOAD.cfg).
ART_CPPLINT := external/google-styleguide/cpplint/cpplint.py

# This file previously configured many cpplint settings.
# Everything that could be moved to CPPLINT.cfg has moved there.
# Please add new settings to CPPLINT.cfg over adding new flags in this file.

ART_CPPLINT_FLAGS :=
# No output when there are no errors.
ART_CPPLINT_QUIET := --quiet

#  1) Get list of all .h & .cc files in the art directory.
#  2) Prepends 'art/' to each of them to make the full name.
ART_CPPLINT_SRC := $(addprefix $(LOCAL_PATH)/, $(call all-subdir-named-files,*.h) $(call all-subdir-named-files,*$(ART_CPP_EXTENSION)))

#  1) Get list of all CPPLINT.cfg files in the art directory.
#  2) Prepends 'art/' to each of them to make the full name.
ART_CPPLINT_CFG := $(addprefix $(LOCAL_PATH)/, $(call all-subdir-named-files,CPPLINT.cfg))

# "mm cpplint-art" to verify we aren't regressing
# - files not touched since the last build are skipped (quite fast).
.PHONY: cpplint-art
cpplint-art: cpplint-art-phony

# "mm cpplint-art-all" to manually execute cpplint.py on all files (very slow).
.PHONY: cpplint-art-all
cpplint-art-all:
	$(ART_CPPLINT) $(ART_CPPLINT_FLAGS) $(ART_CPPLINT_SRC)

OUT_CPPLINT := $(TARGET_COMMON_OUT_ROOT)/cpplint

# Build up the list of all targets for linting the ART source files.
ART_CPPLINT_TARGETS :=

define declare-art-cpplint-target
art_cpplint_file := $(1)
art_cpplint_touch := $$(OUT_CPPLINT)/$$(subst /,__,$$(art_cpplint_file))

$$(art_cpplint_touch): $$(art_cpplint_file) $(ART_CPPLINT) $(ART_CPPLINT_CFG) art/build/Android.cpplint.mk
	$(hide) $(ART_CPPLINT) $(ART_CPPLINT_QUIET) $(ART_CPPLINT_FLAGS) $$<
	$(hide) mkdir -p $$(dir $$@)
	$(hide) touch $$@

ART_CPPLINT_TARGETS += $$(art_cpplint_touch)
endef

$(foreach file, $(ART_CPPLINT_SRC), $(eval $(call declare-art-cpplint-target,$(file))))
#$(info $(call declare-art-cpplint-target,$(firstword $(ART_CPPLINT_SRC))))

include $(CLEAR_VARS)
LOCAL_MODULE := cpplint-art-phony
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(ART_CPPLINT_TARGETS)
include $(BUILD_PHONY_PACKAGE)
