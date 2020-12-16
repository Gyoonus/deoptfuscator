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
    public static void floatTest() {
      float f = 0;
      float nf = -0;
      float fc = 1f;
      for (int i = 0; i < 2; i++) {
        f -= fc;
        f = -f;
        nf -= fc;
        nf = -nf;
      }

      System.out.println(f);
      System.out.println(nf);
      System.out.println(f + 0f);
      System.out.println(f - (-0f));
      System.out.println(-f - (-nf));
      System.out.println(-f + (-nf));
    }

    public static void doubleTest() {
      double d = 0;
      double nd = -0;
      double dc = 1f;
      for (int i = 0; i < 2; i++) {
        d -= dc;
        d = -d;
        nd -= dc;
        nd = -nd;
      }

      System.out.println(d);
      System.out.println(nd);
      System.out.println(d + 0f);
      System.out.println(d - (-0f));
      System.out.println(-d - (-nd));
      System.out.println(-d + (-nd));
    }

    public static void bug_1() {
      int i4=18, i3=-48959;
      float d;
      float f=-0.0f;
      float a=0.0f;

      d = -f + (-a);
      f += i4 * i3;

      System.out.println("d " + d);
    }

    public static void main(String[] args) {
        doubleTest();
        floatTest();
        bug_1();
    }

}
