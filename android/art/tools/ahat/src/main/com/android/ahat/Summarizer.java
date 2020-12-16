/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.ahat;

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.Site;
import com.android.ahat.heapdump.Value;
import java.net.URI;

/**
 * Class for generating a DocString summary of an instance or value.
 */
class Summarizer {

  // For string literals, we limit the number of characters we show to
  // kMaxChars in case the string is really long.
  private static int kMaxChars = 200;

  /**
   * Creates a DocString representing a summary of the given instance.
   */
  public static DocString summarize(AhatInstance inst) {
    DocString formatted = new DocString();
    if (inst == null) {
      formatted.append("null");
      return formatted;
    }

    // Annotate new objects as new.
    if (inst.getBaseline().isPlaceHolder()) {
      formatted.append(DocString.added("new "));
    }

    // Annotate deleted objects as deleted.
    if (inst.isPlaceHolder()) {
      formatted.append(DocString.removed("del "));
    }

    // Annotate unreachable objects as such.
    if (inst.isWeaklyReachable()) {
      formatted.append("weak ");
    } else if (inst.isUnreachable()) {
      formatted.append("unreachable ");
    }

    // Annotate roots as roots.
    if (inst.isRoot()) {
      formatted.append("root ");
    }

    DocString linkText = DocString.text(inst.toString());
    if (inst.isPlaceHolder()) {
      // Don't make links to placeholder objects.
      formatted.append(linkText);
    } else {
      URI objTarget = DocString.formattedUri("object?id=0x%x", inst.getId());
      formatted.appendLink(objTarget, linkText);
    }

    // Annotate Strings with their values.
    String stringValue = inst.asString(kMaxChars);
    if (stringValue != null) {
      formatted.appendFormat(" \"%s", stringValue);
      formatted.append(kMaxChars == stringValue.length() ? "..." : "\"");
    }

    // Annotate Reference with its referent
    AhatInstance referent = inst.getReferent();
    if (referent != null) {
      formatted.append(" for ");

      // It should not be possible for a referent to refer back to the
      // reference object, even indirectly, so there shouldn't be any issues
      // with infinite recursion here.
      formatted.append(summarize(referent));
    }

    // Annotate DexCache with its location.
    String dexCacheLocation = inst.getDexCacheLocation(kMaxChars);
    if (dexCacheLocation != null) {
      formatted.appendFormat(" for %s", dexCacheLocation);
      if (kMaxChars == dexCacheLocation.length()) {
        formatted.append("...");
      }
    }

    // Annotate bitmaps with a thumbnail.
    AhatInstance bitmap = inst.getAssociatedBitmapInstance();
    String thumbnail = "";
    if (bitmap != null) {
      URI uri = DocString.formattedUri("bitmap?id=0x%x", bitmap.getId());
      formatted.appendThumbnail(uri, "bitmap image");
    }
    return formatted;
  }

  /**
   * Creates a DocString summarizing the given value.
   */
  public static DocString summarize(Value value) {
    if (value == null) {
      return DocString.text("null");
    }
    if (value.isAhatInstance()) {
      return summarize(value.asAhatInstance());
    }
    return DocString.text(value.toString());
  }

  /**
   * Creates a DocString summarizing the given site.
   */
  public static DocString summarize(Site site) {
    DocString text = DocString.text(site.getMethodName());
    text.append(site.getSignature());
    text.append(" - ");
    text.append(site.getFilename());
    if (site.getLineNumber() > 0) {
      text.append(":").append(Integer.toString(site.getLineNumber()));
    }
    URI uri = DocString.formattedUri("site?id=%d", site.getId());
    return DocString.link(uri, text);
  }
}
