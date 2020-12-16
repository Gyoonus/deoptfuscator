/*
 * Copyright (C) 2016 The Android Open Source Project
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

class Main {
  public String getA() {
    return "A";
  }
  public String getB() {
    return "B";
  }
  public String getC() {
    return "C";
  }
}

class TestInline {
  public int inlineMonomorphic(Super s) {
    return s.getValue();
  }

  public int inlinePolymorphic(Super s) {
    return s.getValue();
  }

  public int inlineMegamorphic(Super s) {
    return s.getValue();
  }

  public int inlineMissingTypes(Super s) {
    return s.getValue();
  }

  public int noInlineCache(Super s) {
    return s.getValue();
  }
}

abstract class Super {
  abstract int getValue();
}

class SubA extends Super {
  int getValue() { return 42; }
}

class SubB extends Super {
  int getValue() { return 38; };
}

class SubD extends Super {
  int getValue() { return 20; };
}

class SubE extends Super {
  int getValue() { return 16; };
}
