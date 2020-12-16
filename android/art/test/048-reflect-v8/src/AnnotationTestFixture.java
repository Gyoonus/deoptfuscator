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

public class AnnotationTestFixture {

  @Calendar(dayOfWeek="single", hour=23)
  public static void singleUser() {

  }
  @Calendars ({
    @Calendar(dayOfMonth="last"),
    @Calendar(dayOfWeek="Fri", hour=23)
  })
  public static void user() {

  }

  @Calendars ({
    @Calendar(dayOfMonth="z"),
    @Calendar(dayOfMonth="x"),
    @Calendar(dayOfMonth="y")
  })
  public static void user2() {

  }

  @Calendar(dayOfMonth="afirst")
  @Calendars ({
    @Calendar(dayOfMonth="zsecond"),
    @Calendar(dayOfMonth="athird", hour=23)
  })
  public static void userComplex() {

  }
}
