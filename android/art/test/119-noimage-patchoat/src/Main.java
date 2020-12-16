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
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    boolean hasImage = hasImage();
    System.out.println(
        "Has image is " + hasImage + ", is image dex2oat enabled is "
        + isImageDex2OatEnabled() + ".");

    if (hasImage && !isImageDex2OatEnabled()) {
      throw new Error("Image with dex2oat disabled runs with an oat file");
    } else if (!hasImage && isImageDex2OatEnabled()) {
      throw new Error("Image with dex2oat enabled runs without an oat file");
    }
  }

  private native static boolean hasImage();

  private native static boolean isImageDex2OatEnabled();
}
