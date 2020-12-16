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

public class ClassPair {
    public Class<?> first;
    public Class<?> second;

    public ClassPair(Class<?> first, Class<?> second) {
        this.first = first;
        this.second = second;
    }

    public void print() {
        String first_loader_name = first.getClassLoader().getClass().getName();
        System.out.println("first: " + first.getName() + " class loader: " + first_loader_name);
        String second_loader_name = second.getClassLoader().getClass().getName();
        System.out.println("second: " + second.getName() + " class loader: " + second_loader_name);
    }
}
