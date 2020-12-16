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

package com.android.amm.test;

import android.content.Context;
import android.graphics.Canvas;
import android.view.TextureView;
import android.view.ViewGroup;

class TextureViewUse {

  private TextureView mTextureView;

  /**
   * Constructs a TextureView object with given dimensions.
   * The texture view is added to the given ViewGroup object, which should be
   * included in the main display.
   */
  public TextureViewUse(Context context, ViewGroup vg, int width, int height) {
    mTextureView = new TextureView(context);
    vg.addView(mTextureView, width, height);
    mTextureView.post(new CycleRunnable());
  }

  // To force as many graphics buffers as will ever be used to actually be
  // used, we cycle the color of the texture view a handful of times right
  // when things start up.
  private class CycleRunnable implements Runnable {
    private int mCycles = 0;
    private int mRed = 255;
    private int mGreen = 255;
    private int mBlue = 0;

    public void run() {
      if (mCycles < 10) {
        mCycles++;
        updateTextureView();
        mTextureView.post(this);
      }
    }

    private void updateTextureView() {
      Canvas canvas = mTextureView.lockCanvas();
      if (canvas != null) {
        canvas.drawRGB(mRed, mGreen, mBlue);
        int tmp = mRed;
        mTextureView.unlockCanvasAndPost(canvas);
        mRed = mGreen;
        mGreen = mBlue;
        mBlue = tmp;
      }
    }
  }
}

