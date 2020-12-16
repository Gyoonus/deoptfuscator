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

import java.lang.annotation.*;

@AnnoClass1(AnnoClass2.class)
@AnnoClass2(AnnoClass3.class)
@AnnoClass3(AnnoClass1.class)
public class AnnotationThread implements Runnable {
    public void run() {
        for (int i = 0; i < 20; i++) {
            Annotation[] annotations = AnnotationThread.class.getAnnotations();
            if (annotations == null) {
                System.out.println("error: AnnotationThread class has no annotations");
                return;
            }
        }
    }
}
