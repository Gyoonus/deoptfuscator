/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.annotation.Inherited;
import java.lang.annotation.Repeatable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// This is a plain old non-1.8 annotation. At runtime we can see that it has a
// "Repeatable" annotation if we query with getDeclaredAnnotation(Repeatable.class)
@Retention(RetentionPolicy.RUNTIME)
@Repeatable(Calendars.class)
@Inherited  // note: container must also be @Inherited by JLS.
public @interface Calendar {
    String dayOfMonth() default "unspecified_month";
    String dayOfWeek() default "unspecified_week";
    int hour() default 6;
}

