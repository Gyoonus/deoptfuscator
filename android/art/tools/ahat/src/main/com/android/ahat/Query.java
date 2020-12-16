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

import java.net.URI;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

/**
 * A class for getting and modifying query parameters.
 */
class Query {
  private URI mUri;

  // Map from parameter name to value. If the same parameter appears multiple
  // times, only the last value is used.
  private Map<String, String> mParams;

  public Query(URI uri) {
    mUri = uri;
    mParams = new HashMap<String, String>();

    String query = uri.getQuery();
    if (query != null) {
      for (String param : query.split("&")) {
        int i = param.indexOf('=');
        if (i < 0) {
          mParams.put(param, "");
        } else {
          mParams.put(param.substring(0, i), param.substring(i + 1));
        }
      }
    }
  }

  /**
   * Return the value of a query parameter with the given name.
   * If there is no query parameter with that name, returns the default value.
   * If there are multiple query parameters with that name, the value of the
   * last query parameter is returned.
   * If the parameter is defined with an empty value, "" is returned.
   */
  public String get(String name, String defaultValue) {
    String value = mParams.get(name);
    return (value == null) ? defaultValue : value;
  }

  /**
   * Return the long value of a query parameter with the given name.
   */
  public long getLong(String name, long defaultValue) {
    String value = get(name, null);
    return value == null ? defaultValue : Long.decode(value);
  }

  /**
   * Return the int value of a query parameter with the given name.
   */
  public int getInt(String name, int defaultValue) {
    String value = get(name, null);
    return value == null ? defaultValue : Integer.decode(value);
  }

  /**
   * Return a uri suitable for an href target that links to the current
   * page, except with the named query parameter set to the new value.
   *
   * The generated parameters will be sorted alphabetically so it is easier to
   * test.
   */
  public URI with(String name, String value) {
    StringBuilder newQuery = new StringBuilder();
    newQuery.append(mUri.getRawPath());
    newQuery.append('?');

    Map<String, String> params = new TreeMap<String, String>(mParams);
    params.put(name, value);
    String and = "";
    for (Map.Entry<String, String> entry : params.entrySet()) {
      newQuery.append(and);
      newQuery.append(entry.getKey());
      newQuery.append('=');
      newQuery.append(entry.getValue());
      and = "&";
    }
    return DocString.uri(newQuery.toString());
  }

  /**
   * Return a uri suitable for an href target that links to the current
   * page, except with the named query parameter set to the new long value.
   */
  public URI with(String name, long value) {
    return with(name, String.valueOf(value));
  }
}
