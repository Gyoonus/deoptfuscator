// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package art

import (
	"android/soong/android"
	"android/soong/cc"
	"fmt"
	"sync"

	"github.com/google/blueprint/proptools"
)

var supportedArches = []string{"arm", "arm64", "mips", "mips64", "x86", "x86_64"}

func globalFlags(ctx android.BaseContext) ([]string, []string) {
	var cflags []string
	var asflags []string

	opt := envDefault(ctx, "ART_NDEBUG_OPT_FLAG", "-O3")
	cflags = append(cflags, opt)

	tlab := false

	gcType := envDefault(ctx, "ART_DEFAULT_GC_TYPE", "CMS")

	if envTrue(ctx, "ART_TEST_DEBUG_GC") {
		gcType = "SS"
		tlab = true
	}

	cflags = append(cflags, "-DART_DEFAULT_GC_TYPE_IS_"+gcType)
	if tlab {
		cflags = append(cflags, "-DART_USE_TLAB=1")
	}

	imtSize := envDefault(ctx, "ART_IMT_SIZE", "43")
	cflags = append(cflags, "-DIMT_SIZE="+imtSize)

	if envTrue(ctx, "ART_HEAP_POISONING") {
		cflags = append(cflags, "-DART_HEAP_POISONING=1")
		asflags = append(asflags, "-DART_HEAP_POISONING=1")
	}

	if !envFalse(ctx, "ART_USE_READ_BARRIER") && ctx.AConfig().ArtUseReadBarrier() {
		// Used to change the read barrier type. Valid values are BAKER, BROOKS,
		// TABLELOOKUP. The default is BAKER.
		barrierType := envDefault(ctx, "ART_READ_BARRIER_TYPE", "BAKER")
		cflags = append(cflags,
			"-DART_USE_READ_BARRIER=1",
			"-DART_READ_BARRIER_TYPE_IS_"+barrierType+"=1")
		asflags = append(asflags,
			"-DART_USE_READ_BARRIER=1",
			"-DART_READ_BARRIER_TYPE_IS_"+barrierType+"=1")
	}

  cdexLevel := envDefault(ctx, "ART_DEFAULT_COMPACT_DEX_LEVEL", "fast")
  cflags = append(cflags, "-DART_DEFAULT_COMPACT_DEX_LEVEL="+cdexLevel)

	// We need larger stack overflow guards for ASAN, as the compiled code will have
	// larger frame sizes. For simplicity, just use global not-target-specific cflags.
	// Note: We increase this for both debug and non-debug, as the overflow gap will
	//       be compiled into managed code. We always preopt (and build core images) with
	//       the debug version. So make the gap consistent (and adjust for the worst).
	if len(ctx.AConfig().SanitizeDevice()) > 0 || len(ctx.AConfig().SanitizeHost()) > 0 {
		cflags = append(cflags,
			"-DART_STACK_OVERFLOW_GAP_arm=8192",
			"-DART_STACK_OVERFLOW_GAP_arm64=8192",
			"-DART_STACK_OVERFLOW_GAP_mips=16384",
			"-DART_STACK_OVERFLOW_GAP_mips64=16384",
			"-DART_STACK_OVERFLOW_GAP_x86=16384",
			"-DART_STACK_OVERFLOW_GAP_x86_64=20480")
	} else {
		cflags = append(cflags,
			"-DART_STACK_OVERFLOW_GAP_arm=8192",
			"-DART_STACK_OVERFLOW_GAP_arm64=8192",
			"-DART_STACK_OVERFLOW_GAP_mips=16384",
			"-DART_STACK_OVERFLOW_GAP_mips64=16384",
			"-DART_STACK_OVERFLOW_GAP_x86=8192",
			"-DART_STACK_OVERFLOW_GAP_x86_64=8192")
	}

	if envTrue(ctx, "ART_ENABLE_ADDRESS_SANITIZER") {
		// Used to enable full sanitization, i.e., user poisoning, under ASAN.
		cflags = append(cflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
		asflags = append(asflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
	}

	if envTrue(ctx, "ART_MIPS32_CHECK_ALIGNMENT") {
		// Enable the use of MIPS32 CHECK_ALIGNMENT macro for debugging purposes
		asflags = append(asflags, "-DART_MIPS32_CHECK_ALIGNMENT")
	}

	if envTrueOrDefault(ctx, "USE_D8_DESUGAR") {
		cflags = append(cflags, "-DUSE_D8_DESUGAR=1")
	}

	return cflags, asflags
}

func debugFlags(ctx android.BaseContext) []string {
	var cflags []string

	opt := envDefault(ctx, "ART_DEBUG_OPT_FLAG", "-O2")
	cflags = append(cflags, opt)

	return cflags
}

func deviceFlags(ctx android.BaseContext) []string {
	var cflags []string
	deviceFrameSizeLimit := 1736
	if len(ctx.AConfig().SanitizeDevice()) > 0 {
		deviceFrameSizeLimit = 7400
	}
	cflags = append(cflags,
		fmt.Sprintf("-Wframe-larger-than=%d", deviceFrameSizeLimit),
		fmt.Sprintf("-DART_FRAME_SIZE_LIMIT=%d", deviceFrameSizeLimit),
	)

	cflags = append(cflags, "-DART_BASE_ADDRESS="+ctx.AConfig().LibartImgDeviceBaseAddress())
	if envTrue(ctx, "ART_TARGET_LINUX") {
		cflags = append(cflags, "-DART_TARGET_LINUX")
	} else {
		cflags = append(cflags, "-DART_TARGET_ANDROID")
	}
	minDelta := envDefault(ctx, "LIBART_IMG_TARGET_MIN_BASE_ADDRESS_DELTA", "-0x1000000")
	maxDelta := envDefault(ctx, "LIBART_IMG_TARGET_MAX_BASE_ADDRESS_DELTA", "0x1000000")
	cflags = append(cflags, "-DART_BASE_ADDRESS_MIN_DELTA="+minDelta)
	cflags = append(cflags, "-DART_BASE_ADDRESS_MAX_DELTA="+maxDelta)

	return cflags
}

func hostFlags(ctx android.BaseContext) []string {
	var cflags []string
	hostFrameSizeLimit := 1736
	if len(ctx.AConfig().SanitizeHost()) > 0 {
		// art/test/137-cfi/cfi.cc
		// error: stack frame size of 1944 bytes in function 'Java_Main_unwindInProcess'
		hostFrameSizeLimit = 6400
	}
	cflags = append(cflags,
		fmt.Sprintf("-Wframe-larger-than=%d", hostFrameSizeLimit),
		fmt.Sprintf("-DART_FRAME_SIZE_LIMIT=%d", hostFrameSizeLimit),
	)

	cflags = append(cflags, "-DART_BASE_ADDRESS="+ctx.AConfig().LibartImgHostBaseAddress())
	minDelta := envDefault(ctx, "LIBART_IMG_HOST_MIN_BASE_ADDRESS_DELTA", "-0x1000000")
	maxDelta := envDefault(ctx, "LIBART_IMG_HOST_MAX_BASE_ADDRESS_DELTA", "0x1000000")
	cflags = append(cflags, "-DART_BASE_ADDRESS_MIN_DELTA="+minDelta)
	cflags = append(cflags, "-DART_BASE_ADDRESS_MAX_DELTA="+maxDelta)

	if len(ctx.AConfig().SanitizeHost()) > 0 && !envFalse(ctx, "ART_ENABLE_ADDRESS_SANITIZER") {
		// We enable full sanitization on the host by default.
		cflags = append(cflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
	}

	return cflags
}

func globalDefaults(ctx android.LoadHookContext) {
	type props struct {
		Target struct {
			Android struct {
				Cflags []string
			}
			Host struct {
				Cflags []string
			}
		}
		Cflags   []string
		Asflags  []string
		Sanitize struct {
			Recover []string
		}
	}

	p := &props{}
	p.Cflags, p.Asflags = globalFlags(ctx)
	p.Target.Android.Cflags = deviceFlags(ctx)
	p.Target.Host.Cflags = hostFlags(ctx)

	if envTrue(ctx, "ART_DEX_FILE_ACCESS_TRACKING") {
		p.Cflags = append(p.Cflags, "-DART_DEX_FILE_ACCESS_TRACKING")
		p.Sanitize.Recover = []string{
			"address",
		}
	}

	ctx.AppendProperties(p)
}

func debugDefaults(ctx android.LoadHookContext) {
	type props struct {
		Cflags []string
	}

	p := &props{}
	p.Cflags = debugFlags(ctx)
	ctx.AppendProperties(p)
}

func customLinker(ctx android.LoadHookContext) {
	linker := envDefault(ctx, "CUSTOM_TARGET_LINKER", "")
	type props struct {
		DynamicLinker string
	}

	p := &props{}
	if linker != "" {
		p.DynamicLinker = linker
	}

	ctx.AppendProperties(p)
}

func prefer32Bit(ctx android.LoadHookContext) {
	type props struct {
		Target struct {
			Host struct {
				Compile_multilib *string
			}
		}
	}

	p := &props{}
	if envTrue(ctx, "HOST_PREFER_32_BIT") {
		p.Target.Host.Compile_multilib = proptools.StringPtr("prefer32")
	}

	ctx.AppendProperties(p)
}

func testMap(config android.Config) map[string][]string {
	return config.Once("artTests", func() interface{} {
		return make(map[string][]string)
	}).(map[string][]string)
}

func testInstall(ctx android.InstallHookContext) {
	testMap := testMap(ctx.AConfig())

	var name string
	if ctx.Host() {
		name = "host_"
	} else {
		name = "device_"
	}
	name += ctx.Arch().ArchType.String() + "_" + ctx.ModuleName()

	artTestMutex.Lock()
	defer artTestMutex.Unlock()

	tests := testMap[name]
	tests = append(tests, ctx.Path().RelPathString())
	testMap[name] = tests
}

var artTestMutex sync.Mutex

func init() {
	android.RegisterModuleType("art_cc_library", artLibrary)
	android.RegisterModuleType("art_cc_static_library", artStaticLibrary)
	android.RegisterModuleType("art_cc_binary", artBinary)
	android.RegisterModuleType("art_cc_test", artTest)
	android.RegisterModuleType("art_cc_test_library", artTestLibrary)
	android.RegisterModuleType("art_cc_defaults", artDefaultsFactory)
	android.RegisterModuleType("art_global_defaults", artGlobalDefaultsFactory)
	android.RegisterModuleType("art_debug_defaults", artDebugDefaultsFactory)
}

func artGlobalDefaultsFactory() android.Module {
	module := artDefaultsFactory()
	android.AddLoadHook(module, globalDefaults)

	return module
}

func artDebugDefaultsFactory() android.Module {
	module := artDefaultsFactory()
	android.AddLoadHook(module, debugDefaults)

	return module
}

func artDefaultsFactory() android.Module {
	c := &codegenProperties{}
	module := cc.DefaultsFactory(c)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, true) })

	return module
}

func artLibrary() android.Module {
	m, _ := cc.NewLibrary(android.HostAndDeviceSupported)
	module := m.Init()

	installCodegenCustomizer(module, true)

	return module
}

func artStaticLibrary() android.Module {
	m, library := cc.NewLibrary(android.HostAndDeviceSupported)
	library.BuildOnlyStatic()
	module := m.Init()

	installCodegenCustomizer(module, true)

	return module
}

func artBinary() android.Module {
	binary, _ := cc.NewBinary(android.HostAndDeviceSupported)
	module := binary.Init()

	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	return module
}

func artTest() android.Module {
	test := cc.NewTest(android.HostAndDeviceSupported)
	module := test.Init()

	installCodegenCustomizer(module, false)

	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	return module
}

func artTestLibrary() android.Module {
	test := cc.NewTestLibrary(android.HostAndDeviceSupported)
	module := test.Init()

	installCodegenCustomizer(module, false)

	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	return module
}

func envDefault(ctx android.BaseContext, key string, defaultValue string) string {
	ret := ctx.AConfig().Getenv(key)
	if ret == "" {
		return defaultValue
	}
	return ret
}

func envTrue(ctx android.BaseContext, key string) bool {
	return ctx.AConfig().Getenv(key) == "true"
}

func envFalse(ctx android.BaseContext, key string) bool {
	return ctx.AConfig().Getenv(key) == "false"
}

func envTrueOrDefault(ctx android.BaseContext, key string) bool {
	return ctx.AConfig().Getenv(key) != "false"
}
