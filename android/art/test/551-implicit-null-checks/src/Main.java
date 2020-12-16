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

public class Main {

  private class Inner {
    private long i1;
    private double i2;
  }
  private Inner inst;

  public static void main(String args[]) throws Exception {
    Main m = new Main();
    try {
      m.$opt$noinline$testGetLong();
    } catch (NullPointerException ex) {
      System.out.println("NPE from GetLong");
    }
    try {
      m.$opt$noinline$testPutLong(778899112233L);
    } catch (NullPointerException ex) {
      System.out.println("NPE from PutLong");
    }
    try {
      m.$opt$noinline$testGetDouble();
    } catch (NullPointerException ex) {
      System.out.println("NPE from GetDouble");
    }
    try {
      m.$opt$noinline$testPutDouble(1.0);
    } catch (NullPointerException ex) {
      System.out.println("NPE from PutDouble");
    }
  }

  public void $opt$noinline$testGetLong() throws Exception {
    long result = inst.i1;
    throw new Exception();  // prevent inline
  }

  public void $opt$noinline$testPutLong(long a) throws Exception {
    inst.i1 = a;
    throw new Exception();  // prevent inline
  }

  public void $opt$noinline$testGetDouble() throws Exception {
    double result = inst.i2;
    throw new Exception();  // prevent inline
  }

  public void $opt$noinline$testPutDouble(double a) throws Exception {
    inst.i2 = a;
    throw new Exception();  // prevent inline
  }
}
