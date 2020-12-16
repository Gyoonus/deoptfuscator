/*
 * Copyright (C) 2014 The Android Open Source Project
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
    static public void main(String[] args) throws Exception {
      System.out.println(new FillerA().getClass().getName());

      Inf1 second = new Second();
      System.out.println(second.getClass().getName());
      second.zcall();
      second.zcall1();
      second.zcall2();
      second.zcall3();
      second.zcall4();
      second.zcall5();
      second.zcall6();
      second.zcall7();
      second.zcall8();
      second.zcall9();
    }

}
