/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import annotations.Constant;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

class TestLinkerUnrelatedBSM extends TestBase {
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = UnrelatedBSM.class,
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        Class.class
                    },
                    name = "bsm"
                ),
        fieldOrMethodName = "_addf",
        returnType = float.class,
        parameterTypes = {float.class, float.class},
        constantArgumentsForBootstrapMethod = {@Constant(classValue = TestLinkerUnrelatedBSM.class)}
    )
    private static float addf(float a, float b) {
        assertNotReached();
        return Float.MIN_VALUE;
    }

    public static float _addf(float a, float b) {
        return a + b;
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = UnrelatedBSM.class,
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        Class.class
                    },
                    name = "bsm"
                ),
        fieldOrMethodName = "_subf",
        returnType = float.class,
        parameterTypes = {float.class, float.class},
        constantArgumentsForBootstrapMethod = {@Constant(classValue = TestLinkerUnrelatedBSM.class)}
    )
    private static float subf(float a, float b) {
        assertNotReached();
        return Float.MIN_VALUE;
    }

    private static float _subf(float a, float b) {
        return a - b;
    }

    public static void test() {
        System.out.println(TestLinkerUnrelatedBSM.class.getName());
        assertEquals(2.5f, addf(2.0f, 0.5f));
        assertEquals(1.5f, subf(2.0f, 0.5f));
    }
}
