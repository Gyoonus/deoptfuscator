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
import java.lang.invoke.CallSite;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;

/**
 * Describes a bootstrap method that performs method handle resolution on behalf of an
 * invoke-dynamic instruction.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.ANNOTATION_TYPE)
public @interface BootstrapMethod {
    /** The class containing the bootstrap method. */
    Class<?> enclosingType();

    /** The bootstrap method name. */
    String name();

    /** The return type of the bootstrap method. */
    Class<?> returnType() default CallSite.class;

    /** The parameter types of the bootstrap method. */
    Class<?>[] parameterTypes() default {Lookup.class, String.class, MethodType.class};
}
