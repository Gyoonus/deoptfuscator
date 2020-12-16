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

########################################################################
# Rules to build a smaller "core" image to support core libraries
# (that is, non-Android frameworks) testing on the host and target
#
# The main rules to build the default "boot" image are in
# build/core/dex_preopt_libart.mk

include art/build/Android.common_build.mk

LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION :=
ifeq ($(DEX2OAT_HOST_INSTRUCTION_SET_FEATURES),)
  LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=default
else
  LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=$(DEX2OAT_HOST_INSTRUCTION_SET_FEATURES)
endif
LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION :=
ifeq ($($(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES),)
  LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=default
else
  LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=$($(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES)
endif

# Use dex2oat debug version for better error reporting
# $(1): compiler - optimizing, interpreter or interpreter-access-checks.
# $(2): 2ND_ or undefined, 2ND_ for 32-bit host builds.
# $(3): wrapper, e.g., valgrind.
# $(4): dex2oat suffix, e.g, valgrind requires 32 right now.
# $(5): multi-image.
# NB depending on HOST_CORE_DEX_LOCATIONS so we are sure to have the dex files in frameworks for
# run-test --no-image
define create-core-oat-host-rules
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
  core_dex2oat_dependency := $(DEX2OAT_DEPENDENCY)

  ifeq ($(1),optimizing)
    core_compile_options += --compiler-backend=Optimizing
    core_dex2oat_dependency := $(DEX2OAT)
  endif
  ifeq ($(1),interpreter)
    core_compile_options += --compiler-filter=quicken
    core_infix := -interpreter
  endif
  ifeq ($(1),interp-ac)
    core_compile_options += --compiler-filter=extract --runtime-arg -Xverify:softfail
    core_infix := -interp-ac
  endif
  ifneq ($(filter-out interpreter interp-ac optimizing,$(1)),)
    #Technically this test is not precise, but hopefully good enough.
    $$(error found $(1) expected interpreter, interpreter-access-checks, or optimizing)
  endif

  # If $(5) is true, generate a multi-image.
  ifeq ($(5),true)
    core_multi_infix := -multi
    core_multi_param := --multi-image --no-inline-from=core-oj-hostdex.jar
    core_multi_group := _multi
  else
    core_multi_infix :=
    core_multi_param :=
    core_multi_group :=
  endif

  core_image_name := $($(2)HOST_CORE_IMG_OUT_BASE)$$(core_infix)$$(core_multi_infix)$(3)$(CORE_IMG_SUFFIX)
  core_oat_name := $($(2)HOST_CORE_OAT_OUT_BASE)$$(core_infix)$$(core_multi_infix)$(3)$(CORE_OAT_SUFFIX)

  # Using the bitness suffix makes it easier to add as a dependency for the run-test mk.
  ifeq ($(2),)
    $(3)HOST_CORE_IMAGE_$(1)$$(core_multi_group)_64 := $$(core_image_name)
  else
    $(3)HOST_CORE_IMAGE_$(1)$$(core_multi_group)_32 := $$(core_image_name)
  endif
  $(3)HOST_CORE_IMG_OUTS += $$(core_image_name)
  $(3)HOST_CORE_OAT_OUTS += $$(core_oat_name)

  # If we have a wrapper, make the target phony.
  ifneq ($(3),)
.PHONY: $$(core_image_name)
  endif
$$(core_image_name): PRIVATE_CORE_COMPILE_OPTIONS := $$(core_compile_options)
$$(core_image_name): PRIVATE_CORE_IMG_NAME := $$(core_image_name)
$$(core_image_name): PRIVATE_CORE_OAT_NAME := $$(core_oat_name)
$$(core_image_name): PRIVATE_CORE_MULTI_PARAM := $$(core_multi_param)
$$(core_image_name): $$(HOST_CORE_DEX_LOCATIONS) $$(core_dex2oat_dependency)
	@echo "host dex2oat: $$@"
	@mkdir -p $$(dir $$@)
	$$(hide) $(3) $$(DEX2OAT)$(4) --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  --image-classes=$$(PRELOADED_CLASSES) $$(addprefix --dex-file=,$$(HOST_CORE_DEX_FILES)) \
	  $$(addprefix --dex-location=,$$(HOST_CORE_DEX_LOCATIONS)) --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --base=$$(LIBART_IMG_HOST_BASE_ADDRESS) --instruction-set=$$($(2)ART_HOST_ARCH) \
	  $$(LOCAL_$(2)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION) \
	  --host --android-root=$$(HOST_OUT) \
	  --generate-debug-info --generate-build-id --compile-pic \
	  --runtime-arg -XX:SlowDebug=true \
	  $$(PRIVATE_CORE_MULTI_PARAM) $$(PRIVATE_CORE_COMPILE_OPTIONS)

$$(core_oat_name): $$(core_image_name)

  # Clean up locally used variables.
  core_dex2oat_dependency :=
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
endef  # create-core-oat-host-rules

# $(1): compiler - optimizing, interpreter or interpreter-access-checks.
# $(2): wrapper.
# $(3): dex2oat suffix.
# $(4): multi-image.
define create-core-oat-host-rule-combination
  $(call create-core-oat-host-rules,$(1),,$(2),$(3),$(4))

  ifneq ($(HOST_PREFER_32_BIT),true)
    $(call create-core-oat-host-rules,$(1),2ND_,$(2),$(3),$(4))
  endif
endef

$(eval $(call create-core-oat-host-rule-combination,optimizing,,,false))
$(eval $(call create-core-oat-host-rule-combination,interpreter,,,false))
$(eval $(call create-core-oat-host-rule-combination,interp-ac,,,false))
$(eval $(call create-core-oat-host-rule-combination,optimizing,,,true))
$(eval $(call create-core-oat-host-rule-combination,interpreter,,,true))
$(eval $(call create-core-oat-host-rule-combination,interp-ac,,,true))

valgrindHOST_CORE_IMG_OUTS :=
valgrindHOST_CORE_OAT_OUTS :=
$(eval $(call create-core-oat-host-rule-combination,optimizing,valgrind,32,false))
$(eval $(call create-core-oat-host-rule-combination,interpreter,valgrind,32,false))
$(eval $(call create-core-oat-host-rule-combination,interp-ac,valgrind,32,false))

valgrind-test-art-host-dex2oat-host: $(valgrindHOST_CORE_IMG_OUTS)

test-art-host-dex2oat-host: $(HOST_CORE_IMG_OUTS)

define create-core-oat-target-rules
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
  core_dex2oat_dependency := $(DEX2OAT_DEPENDENCY)

  ifeq ($(1),optimizing)
    core_compile_options += --compiler-backend=Optimizing
    # With the optimizing compiler, we want to rerun dex2oat whenever there is
    # a dex2oat change to catch regressions early.
    core_dex2oat_dependency := $(DEX2OAT)
  endif
  ifeq ($(1),interpreter)
    core_compile_options += --compiler-filter=quicken
    core_infix := -interpreter
  endif
  ifeq ($(1),interp-ac)
    core_compile_options += --compiler-filter=extract --runtime-arg -Xverify:softfail
    core_infix := -interp-ac
  endif
  ifneq ($(filter-out interpreter interp-ac optimizing,$(1)),)
    # Technically this test is not precise, but hopefully good enough.
    $$(error found $(1) expected interpreter, interpreter-access-checks, or optimizing)
  endif

  core_image_name := $($(2)TARGET_CORE_IMG_OUT_BASE)$$(core_infix)$(3)$(CORE_IMG_SUFFIX)
  core_oat_name := $($(2)TARGET_CORE_OAT_OUT_BASE)$$(core_infix)$(3)$(CORE_OAT_SUFFIX)

  # Using the bitness suffix makes it easier to add as a dependency for the run-test mk.
  ifeq ($(2),)
    ifdef TARGET_2ND_ARCH
      $(3)TARGET_CORE_IMAGE_$(1)_64 := $$(core_image_name)
    else
      $(3)TARGET_CORE_IMAGE_$(1)_32 := $$(core_image_name)
    endif
  else
    $(3)TARGET_CORE_IMAGE_$(1)_32 := $$(core_image_name)
  endif
  $(3)TARGET_CORE_IMG_OUTS += $$(core_image_name)
  $(3)TARGET_CORE_OAT_OUTS += $$(core_oat_name)

  # If we have a wrapper, make the target phony.
  ifneq ($(3),)
.PHONY: $$(core_image_name)
  endif
$$(core_image_name): PRIVATE_CORE_COMPILE_OPTIONS := $$(core_compile_options)
$$(core_image_name): PRIVATE_CORE_IMG_NAME := $$(core_image_name)
$$(core_image_name): PRIVATE_CORE_OAT_NAME := $$(core_oat_name)
$$(core_image_name): $$(TARGET_CORE_DEX_FILES) $$(core_dex2oat_dependency)
	@echo "target dex2oat: $$@"
	@mkdir -p $$(dir $$@)
	$$(hide) $(4) $$(DEX2OAT)$(5) --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  --image-classes=$$(PRELOADED_CLASSES) $$(addprefix --dex-file=,$$(TARGET_CORE_DEX_FILES)) \
	  $$(addprefix --dex-location=,$$(TARGET_CORE_DEX_LOCATIONS)) --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --base=$$(LIBART_IMG_TARGET_BASE_ADDRESS) --instruction-set=$$($(2)TARGET_ARCH) \
	  --instruction-set-variant=$$($(2)DEX2OAT_TARGET_CPU_VARIANT) \
	  --instruction-set-features=$$($(2)DEX2OAT_TARGET_INSTRUCTION_SET_FEATURES) \
	  --android-root=$$(PRODUCT_OUT)/system \
	  --generate-debug-info --generate-build-id --compile-pic \
	  --runtime-arg -XX:SlowDebug=true \
	  $$(PRIVATE_CORE_COMPILE_OPTIONS) || (rm $$(PRIVATE_CORE_OAT_NAME); exit 1)

$$(core_oat_name): $$(core_image_name)

  # Clean up locally used variables.
  core_dex2oat_dependency :=
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
endef  # create-core-oat-target-rules

# $(1): compiler - optimizing, interpreter or interpreter-access-checks.
# $(2): wrapper.
# $(3): dex2oat suffix.
define create-core-oat-target-rule-combination
  $(call create-core-oat-target-rules,$(1),,$(2),$(3))

  ifdef TARGET_2ND_ARCH
    $(call create-core-oat-target-rules,$(1),2ND_,$(2),$(3))
  endif
endef

$(eval $(call create-core-oat-target-rule-combination,optimizing,,))
$(eval $(call create-core-oat-target-rule-combination,interpreter,,))
$(eval $(call create-core-oat-target-rule-combination,interp-ac,,))

valgrindTARGET_CORE_IMG_OUTS :=
valgrindTARGET_CORE_OAT_OUTS :=
$(eval $(call create-core-oat-target-rule-combination,optimizing,valgrind,32))
$(eval $(call create-core-oat-target-rule-combination,interpreter,valgrind,32))
$(eval $(call create-core-oat-target-rule-combination,interp-ac,valgrind,32))

valgrind-test-art-host-dex2oat-target: $(valgrindTARGET_CORE_IMG_OUTS)

valgrind-test-art-host-dex2oat: valgrind-test-art-host-dex2oat-host valgrind-test-art-host-dex2oat-target

# Define a default core image that can be used for things like gtests that
# need some image to run, but don't otherwise care which image is used.
HOST_CORE_IMAGE_DEFAULT_32 := $(HOST_CORE_IMAGE_optimizing_32)
HOST_CORE_IMAGE_DEFAULT_64 := $(HOST_CORE_IMAGE_optimizing_64)
TARGET_CORE_IMAGE_DEFAULT_32 := $(TARGET_CORE_IMAGE_optimizing_32)
TARGET_CORE_IMAGE_DEFAULT_64 := $(TARGET_CORE_IMAGE_optimizing_64)
