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

// Stored as a complex annotation Calendars(Calendar,Calendar)
// followed by a Calendar in the binary.
// In other words { Calendars([C,C]), C }
//
// Note that trying to do {C,Calendars,C} or similar
// is illegal by the JLS.
@Calendar(dayOfMonth="afirst")
@Calendars ({
  @Calendar(dayOfMonth="zsecond"),
  @Calendar(dayOfMonth="athird", hour=23)
})
// @Calendar(dayOfMonth="zlast")  // Leave for future ordering test
public class UserComplex {

}
