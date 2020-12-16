/*
 * Copyright (C) 2017 The Android Open Source Project
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

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class HtmlEscaperTest {
  @Test
  public void tests() {
    assertEquals("nothing to escape", HtmlEscaper.escape("nothing to escape"));
    assertEquals("a&lt;b&gt; &amp; &quot;c&apos;d&quot;e", HtmlEscaper.escape("a<b> & \"c\'d\"e"));
    assertEquals("adjacent &lt;&lt;&gt;&gt; x", HtmlEscaper.escape("adjacent <<>> x"));
    assertEquals("&lt; initial", HtmlEscaper.escape("< initial"));
    assertEquals("ending &gt;", HtmlEscaper.escape("ending >"));
  }
}
