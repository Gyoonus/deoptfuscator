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
// in the binary.
//
/* FIXME: Use this code instead, when Jack supports repeatable annotations properly.
 *
 * @Calendar(dayOfMonth="last")
 * @Calendar(dayOfWeek="Fri", hour=23)
 */
@Calendars ({
  @Calendar(dayOfMonth="last"),
  @Calendar(dayOfWeek="Fri", hour=23)
})
public class User {

}
