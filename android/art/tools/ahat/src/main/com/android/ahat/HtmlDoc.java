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

import java.io.PrintStream;
import java.net.URI;
import java.util.List;

/**
 * An Html implementation of Doc.
 */
class HtmlDoc implements Doc {
  private PrintStream ps;
  private Column[] mCurrentTableColumns;

  /**
   * Create an HtmlDoc that writes to the given print stream.
   * @param title - The main page title.
   * @param style - A URI link to a stylesheet to link to.
   */
  public HtmlDoc(PrintStream ps, DocString title, URI style) {
    this.ps = ps;

    ps.println("<!DOCTYPE html>");
    ps.println("<html>");
    ps.println("<head>");
    ps.format("<title>%s</title>\n", title.html());
    ps.format("<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n",
        style.toASCIIString());
    ps.println("</head>");
    ps.println("<body>");
  }

  @Override
  public void title(String format, Object... args) {
    ps.print("<h1>");
    ps.print(DocString.text(String.format(format, args)).html());
    ps.println("</h1>");
  }

  @Override
  public void menu(DocString string) {
    ps.format("<div class=\"menu\">%s</div>", string.html());
  }

  @Override
  public void section(String title) {
    ps.print("<h2>");
    ps.print(DocString.text(title).html());
    ps.println(":</h2>");
  }

  @Override
  public void println(DocString string) {
    ps.print(string.html());
    ps.println("<br />");
  }

  @Override
  public void big(DocString str) {
    ps.print("<h2>");
    ps.print(str.html());
    ps.println("</h2>");
  }

  @Override
  public void table(Column... columns) {
    if (columns.length == 0) {
      throw new IllegalArgumentException("No columns specified");
    }

    mCurrentTableColumns = columns;
    ps.println("<table>");
    for (int i = 0; i < columns.length - 1; i++) {
      if (columns[i].visible) {
        ps.format("<th>%s</th>", columns[i].heading.html());
      }
    }

    // Align the last header to the left so it's easier to see if the last
    // column is very wide.
    if (columns[columns.length - 1].visible) {
      ps.format("<th align=\"left\">%s</th>", columns[columns.length - 1].heading.html());
    }
  }

  @Override
  public void table(DocString description, List<Column> subcols, List<Column> cols) {
    mCurrentTableColumns = new Column[subcols.size() + cols.size()];
    int j = 0;
    int visibleSubCols = 0;
    for (Column col : subcols) {
      if (col.visible) {
        visibleSubCols++;
      }
      mCurrentTableColumns[j] = col;
      j++;
    }
    for (Column col : cols) {
      mCurrentTableColumns[j] = col;
      j++;
    }

    ps.println("<table>");
    ps.format("<tr><th colspan=\"%d\">%s</th>", visibleSubCols, description.html());
    for (int i = 0; i < cols.size() - 1; i++) {
      if (cols.get(i).visible) {
        ps.format("<th rowspan=\"2\">%s</th>", cols.get(i).heading.html());
      }
    }
    if (!cols.isEmpty()) {
      // Align the last column header to the left so it can still be seen if
      // the last column is very wide.
      Column col = cols.get(cols.size() - 1);
      if (col.visible) {
        ps.format("<th align=\"left\" rowspan=\"2\">%s</th>", col.heading.html());
      }
    }
    ps.println("</tr>");

    ps.print("<tr>");
    for (Column subcol : subcols) {
      if (subcol.visible) {
        ps.format("<th>%s</th>", subcol.heading.html());
      }
    }
    ps.println("</tr>");
  }

  @Override
  public void row(DocString... values) {
    if (mCurrentTableColumns == null) {
      throw new IllegalStateException("table method must be called before row");
    }

    if (mCurrentTableColumns.length != values.length) {
      throw new IllegalArgumentException(String.format(
          "Wrong number of row values. Expected %d, but got %d",
          mCurrentTableColumns.length, values.length));
    }

    ps.print("<tr>");
    for (int i = 0; i < values.length; i++) {
      if (mCurrentTableColumns[i].visible) {
      ps.print("<td");
        if (mCurrentTableColumns[i].align == Column.Align.RIGHT) {
          ps.print(" align=\"right\"");
        }
        ps.format(">%s</td>", values[i].html());
      }
    }
    ps.println("</tr>");
  }

  @Override
  public void descriptions() {
    ps.println("<table>");
  }

  @Override
  public void description(DocString key, DocString value) {
    ps.format("<tr><th align=\"left\">%s:</th><td>%s</td></tr>", key.html(), value.html());
  }

  @Override
  public void end() {
    ps.println("</table>");
    mCurrentTableColumns = null;
  }

  @Override
  public void close() {
    ps.println("</body>");
    ps.println("</html>");
    ps.close();
  }
}
