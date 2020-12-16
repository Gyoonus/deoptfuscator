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

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import java.io.IOException;
import java.io.PrintStream;

/**
 * AhatHttpHandler.
 *
 * HttpHandler for AhatHandlers.
 */
class AhatHttpHandler implements HttpHandler {

  private AhatHandler mAhatHandler;

  public AhatHttpHandler(AhatHandler handler) {
    mAhatHandler = handler;
  }

  @Override
  public void handle(HttpExchange exchange) throws IOException {
    exchange.getResponseHeaders().add("Content-Type", "text/html;charset=utf-8");
    exchange.sendResponseHeaders(200, 0);
    PrintStream ps = new PrintStream(exchange.getResponseBody());
    try {
      HtmlDoc doc = new HtmlDoc(ps, DocString.text("ahat"), DocString.uri("style.css"));
      doc.menu(Menu.getMenu());
      mAhatHandler.handle(doc, new Query(exchange.getRequestURI()));
      doc.close();
    } catch (RuntimeException e) {
      // Print runtime exceptions to standard error for debugging purposes,
      // because otherwise they are swallowed and not reported.
      System.err.println("Exception when handling " + exchange.getRequestURI() + ": ");
      e.printStackTrace();
      throw e;
    }
    ps.close();
  }
}
