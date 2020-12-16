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
import com.android.ahat.heapdump.AhatSnapshot;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import javax.imageio.ImageIO;

class BitmapHandler implements HttpHandler {
  private AhatSnapshot mSnapshot;

  public BitmapHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  @Override
  public void handle(HttpExchange exchange) throws IOException {
    try {
      Query query = new Query(exchange.getRequestURI());
      long id = query.getLong("id", 0);
      BufferedImage bitmap = null;
      AhatInstance inst = mSnapshot.findInstance(id);
      if (inst != null) {
        bitmap = inst.asBitmap();
      }

      if (bitmap != null) {
        exchange.getResponseHeaders().add("Content-Type", "image/png");
        exchange.sendResponseHeaders(200, 0);
        OutputStream os = exchange.getResponseBody();
        ImageIO.write(bitmap, "png", os);
        os.close();
      } else {
        exchange.getResponseHeaders().add("Content-Type", "text/html");
        exchange.sendResponseHeaders(404, 0);
        PrintStream ps = new PrintStream(exchange.getResponseBody());
        HtmlDoc doc = new HtmlDoc(ps, DocString.text("ahat"), DocString.uri("style.css"));
        doc.big(DocString.text("No bitmap found for the given request."));
        doc.close();
      }
    } catch (RuntimeException e) {
      // Print runtime exceptions to standard error for debugging purposes,
      // because otherwise they are swallowed and not reported.
      System.err.println("Exception when handling " + exchange.getRequestURI() + ": ");
      e.printStackTrace();
      throw e;
    }
  }
}
