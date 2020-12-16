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

import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Diff;
import com.android.ahat.heapdump.HprofFormatException;
import com.android.ahat.heapdump.Parser;
import com.android.ahat.proguard.ProguardMap;
import com.sun.net.httpserver.HttpServer;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.text.ParseException;
import java.util.concurrent.Executors;

/**
 * Contains the main entry point for the ahat heap dump viewer.
 */
public class Main {
  private Main() {
  }

  private static void help(PrintStream out) {
    out.println("java -jar ahat.jar [OPTIONS] FILE");
    out.println("  Launch an http server for viewing the given Android heap dump FILE.");
    out.println("");
    out.println("OPTIONS:");
    out.println("  -p <port>");
    out.println("     Serve pages on the given port. Defaults to 7100.");
    out.println("  --proguard-map FILE");
    out.println("     Use the proguard map FILE to deobfuscate the heap dump.");
    out.println("  --baseline FILE");
    out.println("     Diff the heap dump against the given baseline heap dump FILE.");
    out.println("  --baseline-proguard-map FILE");
    out.println("     Use the proguard map FILE to deobfuscate the baseline heap dump.");
    out.println("");
  }

  /**
   * Load the given heap dump file.
   * Prints an error message and exits the application on failure to load the
   * heap dump.
   */
  private static AhatSnapshot loadHeapDump(File hprof, ProguardMap map) {
    System.out.println("Processing '" + hprof + "' ...");
    try {
      return Parser.parseHeapDump(hprof, map);
    } catch (IOException e) {
      System.err.println("Unable to load '" + hprof + "':");
      e.printStackTrace();
    } catch (HprofFormatException e) {
      System.err.println("'" + hprof + "' does not appear to be a valid Java heap dump:");
      e.printStackTrace();
    }
    System.exit(1);
    throw new AssertionError("Unreachable");
  }

  /**
   * Main entry for ahat heap dump viewer.
   * Launches an http server on localhost for viewing a given heap dump.
   * See the ahat README or pass "--help" as one of the arguments to see a
   * description of what arguments and options are expected.
   *
   * @param args the command line arguments
   */
  public static void main(String[] args) {
    int port = 7100;
    for (String arg : args) {
      if (arg.equals("--help")) {
        help(System.out);
        return;
      }
    }

    File hprof = null;
    File hprofbase = null;
    ProguardMap map = new ProguardMap();
    ProguardMap mapbase = new ProguardMap();
    for (int i = 0; i < args.length; i++) {
      if ("-p".equals(args[i]) && i + 1 < args.length) {
        i++;
        port = Integer.parseInt(args[i]);
      } else if ("--proguard-map".equals(args[i]) && i + 1 < args.length) {
        i++;
        try {
          map.readFromFile(new File(args[i]));
        } catch (IOException|ParseException ex) {
          System.out.println("Unable to read proguard map: " + ex);
          System.out.println("The proguard map will not be used.");
        }
      } else if ("--baseline-proguard-map".equals(args[i]) && i + 1 < args.length) {
        i++;
        try {
          mapbase.readFromFile(new File(args[i]));
        } catch (IOException|ParseException ex) {
          System.out.println("Unable to read baseline proguard map: " + ex);
          System.out.println("The proguard map will not be used.");
        }
      } else if ("--baseline".equals(args[i]) && i + 1 < args.length) {
        i++;
        if (hprofbase != null) {
          System.err.println("multiple baseline heap dumps.");
          help(System.err);
          return;
        }
        hprofbase = new File(args[i]);
      } else {
        if (hprof != null) {
          System.err.println("multiple input files.");
          help(System.err);
          return;
        }
        hprof = new File(args[i]);
      }
    }

    if (hprof == null) {
      System.err.println("no input file.");
      help(System.err);
      return;
    }

    // Launch the server before parsing the hprof file so we get
    // BindExceptions quickly.
    InetAddress loopback = InetAddress.getLoopbackAddress();
    InetSocketAddress addr = new InetSocketAddress(loopback, port);
    System.out.println("Preparing " + addr + " ...");
    HttpServer server = null;
    try {
      server = HttpServer.create(addr, 0);
    } catch (IOException e) {
      System.err.println("Unable to setup ahat server:");
      e.printStackTrace();
      System.exit(1);
    }

    AhatSnapshot ahat = loadHeapDump(hprof, map);
    if (hprofbase != null) {
      AhatSnapshot base = loadHeapDump(hprofbase, mapbase);

      System.out.println("Diffing heap dumps ...");
      Diff.snapshots(ahat, base);
    }

    server.createContext("/", new AhatHttpHandler(new OverviewHandler(ahat, hprof, hprofbase)));
    server.createContext("/rooted", new AhatHttpHandler(new RootedHandler(ahat)));
    server.createContext("/object", new AhatHttpHandler(new ObjectHandler(ahat)));
    server.createContext("/objects", new AhatHttpHandler(new ObjectsHandler(ahat)));
    server.createContext("/site", new AhatHttpHandler(new SiteHandler(ahat)));
    server.createContext("/bitmap", new BitmapHandler(ahat));
    server.createContext("/style.css", new StaticHandler("style.css", "text/css"));
    server.setExecutor(Executors.newFixedThreadPool(1));
    System.out.println("Server started on localhost:" + port);

    server.start();
  }
}

