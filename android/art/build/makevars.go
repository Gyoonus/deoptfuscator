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
	"sort"
	"strings"

	"android/soong/android"
)

var (
	pctx = android.NewPackageContext("android/soong/art")
)

func init() {
	android.RegisterMakeVarsProvider(pctx, makeVarsProvider)
}

func makeVarsProvider(ctx android.MakeVarsContext) {
	ctx.Strict("LIBART_IMG_HOST_BASE_ADDRESS", ctx.Config().LibartImgHostBaseAddress())
	ctx.Strict("LIBART_IMG_TARGET_BASE_ADDRESS", ctx.Config().LibartImgDeviceBaseAddress())

	testMap := testMap(ctx.Config())
	var testNames []string
	for name := range testMap {
		testNames = append(testNames, name)
	}

	sort.Strings(testNames)

	for _, name := range testNames {
		ctx.Strict("ART_TEST_LIST_"+name, strings.Join(testMap[name], " "))
	}
}
