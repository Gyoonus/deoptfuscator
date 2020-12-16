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
import android.view.WindowManager;
import android.widget.TextView;

class ThreadedRendererUse {

  private TextView mTextView;

  /**
   * Cause a threaded renderer EGL allocation to be used, with given
   * dimensions.
   */
  public ThreadedRendererUse(Context context, int width, int height) {
    mTextView = new TextView(context);
    mTextView.setText("TRU");
    mTextView.setBackgroundColor(0xffff0000);

    // Adding a view to the WindowManager (as opposed to the app's root view
    // hierarchy) causes a ThreadedRenderer and EGL allocations under the cover.
    // We use a TextView here to trigger the use case, but we could use any
    // other kind of view as well.
    WindowManager wm = context.getSystemService(WindowManager.class);
    WindowManager.LayoutParams layout = new WindowManager.LayoutParams();
    layout.width = width;
    layout.height = height;
    wm.addView(mTextView, layout);

    mTextView.post(new CycleRunnable());
  }

  // To force as many graphics buffers as will ever be used to actually be
  // used, we cycle the text of the text view a handful of times right
  // when things start up.
  private class CycleRunnable implements Runnable {
    private int mCycles = 0;

    public void run() {
      if (mCycles < 10) {
        mCycles++;
        mTextView.setText("TRU " + mCycles);
        mTextView.post(this);
      }
    }
  }
}

