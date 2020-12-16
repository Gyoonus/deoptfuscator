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

// Widget class for reference type accessor tests.
public class Widget {
    protected int requisitionNumber;

    public Widget(int requisitionNumber) {
        this.requisitionNumber = requisitionNumber;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof Widget)) {
            return false;
        }
        Widget wo = (Widget) o;
        return requisitionNumber == wo.requisitionNumber;
    }

    public static final Widget ONE = new Widget(1);
    public static final Widget TWO = new Widget(2);
}
