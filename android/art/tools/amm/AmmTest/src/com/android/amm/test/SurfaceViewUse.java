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
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

class SurfaceViewUse {

  private SurfaceView mSurfaceView;

  /**
   * Constructs a SurfaceView object with given dimensions.
   * The surface view is added to the given ViewGroup object, which should be
   * included in the main display.
   */
  public SurfaceViewUse(Context context, ViewGroup vg, int width, int height) {
    mSurfaceView = new SurfaceView(context);
    vg.addView(mSurfaceView, width, height);
    mSurfaceView.post(new CycleRunnable());
  }

  // To force as many graphics buffers as will ever be used to actually be
  // used, we cycle the color of the surface view a handful of times right
  // when things start up.
  private class CycleRunnable implements Runnable {
    private int mCycles = 0;
    private int mRed = 255;
    private int mGreen = 0;
    private int mBlue = 255;

    public void run() {
      if (mCycles < 10) {
        mCycles++;
        updateSurfaceView();
        mSurfaceView.post(this);
      }
    }

    private void updateSurfaceView() {
      SurfaceHolder holder = mSurfaceView.getHolder();
      Canvas canvas = holder.lockHardwareCanvas();
      if (canvas != null) {
        canvas.drawRGB(mRed, mGreen, mBlue);
        int tmp = mRed;
        holder.unlockCanvasAndPost(canvas);
        mRed = mGreen;
        mGreen = mBlue;
        mBlue = tmp;
      }
    }
  }
}

