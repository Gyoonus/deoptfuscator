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

import java.util.Arrays;
import java.util.List;
import java.util.Optional;

public class Main {
    public static void main(String[] args) {
        int requiredLength = 3;
        List<String> list = Arrays.asList("A", "B", "C", "D", "EEE");
        Optional<String> result = list.stream().filter(x -> x.length() >= requiredLength).findAny();
        if (result.isPresent()) {
            System.out.println("Result is " + result.get());
        } else {
            System.out.println("Result is not there.");
        }
    }
}
