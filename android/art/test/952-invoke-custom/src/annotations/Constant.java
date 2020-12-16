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

package annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Describes an annotation that allows passing a constant extra argument to a linker method. */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.ANNOTATION_TYPE)
public @interface Constant {
    boolean[] booleanValue() default {};

    byte[] byteValue() default {};

    char[] charValue() default {};

    short[] shortValue() default {};

    int[] intValue() default {};

    float[] floatValue() default {};

    double[] doubleValue() default {};

    long[] longValue() default {};

    Class<?>[] classValue() default {};

    String[] stringValue() default {};
}
