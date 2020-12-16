/*
 * Copyright (C) 2018 The Android Open Source Project
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

public class TypeCheckBenchmark {
    public void timeCheckCastLevel1ToLevel1(int count) {
        Object[] arr = arr1;
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) arr[i & 1023];
        }
    }

    public void timeCheckCastLevel2ToLevel1(int count) {
        Object[] arr = arr2;
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) arr[i & 1023];
        }
    }

    public void timeCheckCastLevel3ToLevel1(int count) {
        Object[] arr = arr3;
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) arr[i & 1023];
        }
    }

    public void timeCheckCastLevel9ToLevel1(int count) {
        Object[] arr = arr9;
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) arr[i & 1023];
        }
    }

    public void timeCheckCastLevel9ToLevel2(int count) {
        Object[] arr = arr9;
        for (int i = 0; i < count; ++i) {
            Level2 l2 = (Level2) arr[i & 1023];
        }
    }

    public void timeInstanceOfLevel1ToLevel1(int count) {
        int sum = 0;
        Object[] arr = arr1;
        for (int i = 0; i < count; ++i) {
            if (arr[i & 1023] instanceof Level1) {
              ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel2ToLevel1(int count) {
        int sum = 0;
        Object[] arr = arr2;
        for (int i = 0; i < count; ++i) {
            if (arr[i & 1023] instanceof Level1) {
              ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel3ToLevel1(int count) {
        int sum = 0;
        Object[] arr = arr3;
        for (int i = 0; i < count; ++i) {
            if (arr[i & 1023] instanceof Level1) {
              ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel9ToLevel1(int count) {
        int sum = 0;
        Object[] arr = arr9;
        for (int i = 0; i < count; ++i) {
            if (arr[i & 1023] instanceof Level1) {
              ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel9ToLevel2(int count) {
        int sum = 0;
        Object[] arr = arr9;
        for (int i = 0; i < count; ++i) {
            if (arr[i & 1023] instanceof Level2) {
              ++sum;
            }
        }
        result = sum;
    }

    public static Object[] createArray(int level) {
        try {
            Class<?>[] ls = {
                    null,
                    Level1.class,
                    Level2.class,
                    Level3.class,
                    Level4.class,
                    Level5.class,
                    Level6.class,
                    Level7.class,
                    Level8.class,
                    Level9.class,
            };
            Class<?> l = ls[level];
            Object[] array = new Object[1024];
            for (int i = 0; i < array.length; ++i) {
                array[i] = l.newInstance();
            }
            return array;
        } catch (Exception unexpected) {
            throw new Error("Initialization failure!");
        }
    }
    Object[] arr1 = createArray(1);
    Object[] arr2 = createArray(2);
    Object[] arr3 = createArray(3);
    Object[] arr9 = createArray(9);
    int result;
}

class Level1 { }
class Level2 extends Level1 { }
class Level3 extends Level2 { }
class Level4 extends Level3 { }
class Level5 extends Level4 { }
class Level6 extends Level5 { }
class Level7 extends Level6 { }
class Level8 extends Level7 { }
class Level9 extends Level8 { }
