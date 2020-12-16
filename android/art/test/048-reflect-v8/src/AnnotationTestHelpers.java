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

import java.lang.annotation.Annotation;

public class AnnotationTestHelpers {
  // Provide custom print function that print a deterministic output.
  // Note that Annotation#toString has unspecified order: it prints out the
  // fields, which is why we can't rely on it.

  public static String asString(Annotation anno) {
    if (anno instanceof Calendar) {
      return asString((Calendar)anno);
    } else if (anno instanceof Calendars) {
      return asString((Calendars)anno);
    } else {
      if (anno == null) {
        return "<null>";
      }
      // Fall-back, usually would only go here in a test failure.
      return anno.toString();
    }
  }

  public static String asString(Annotation[] annos) {
    String msg = "";

    if (annos == null) {
      msg += "<null>";
    } else if (annos.length == 0) {
      msg += "<empty>";
    } else {
      for (int i = 0; i < annos.length; ++i) {
        msg += asString(annos[i]);

        if (i != annos.length - 1) {
          msg += ", ";
        }
      }
    }

    return msg;
  }

  public static String asString(Calendar calendar) {
    if (calendar == null) {
      return "<null>";
    }

    return "@Calendar(dayOfMonth=" + calendar.dayOfMonth() + ", dayOfWeek=" +
      calendar.dayOfWeek() + ", hour=" + calendar.hour() + ")";
  }

  public static String asString(Calendars calendars) {
    if (calendars == null) {
      return "<null>";
    }

    String s = "@Calendars(value=[";

    Calendar[] allValues = calendars.value();
    for (int i = 0; i < allValues.length; ++i) {
      s += asString(allValues[i]);
      if (i != allValues.length - 1) {
        s += ", ";
      }
    }

    s += "])";

    return s;
  }
}
