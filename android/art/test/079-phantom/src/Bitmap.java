/*
 * Copyright (C) 2009 The Android Open Source Project
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

import java.lang.ref.ReferenceQueue;
import java.lang.ref.PhantomReference;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

public class Bitmap {
    String mName;           /* for debugging */
    int mWidth, mHeight;
    Bitmap.NativeWrapper mNativeWrapper;

    private static int sSerial = 100;
    private static ArrayList sPhantomList = new ArrayList<PhantomWrapper>();
    private static ReferenceQueue<PhantomWrapper> sPhantomQueue =
            new ReferenceQueue<PhantomWrapper>();
    private static BitmapWatcher sWatcher = new BitmapWatcher(sPhantomQueue);
    static {
        sWatcher.setDaemon(true);
        sWatcher.start();
    };

    Bitmap(String name, int width, int height, Bitmap.NativeWrapper nativeData) {
        mName = name;
        mWidth = width;
        mHeight = height;
        mNativeWrapper = nativeData;

        System.out.println("Created " + this);
    }

    public String toString() {
        return "Bitmap " + mName + ": " + mWidth + "x" + mHeight + " (" +
                mNativeWrapper.mNativeData + ")";
    }

    public void drawAt(int x, int y) {
        System.out.println("Drawing " + this);
    }

    public static void shutDown() {
        sWatcher.shutDown();
        try {
            sWatcher.join();
        } catch (InterruptedException ie) {
            System.out.println("join intr");
        }
        System.out.println("Bitmap has shut down");
    }

    /*
     * Pretend we're allocating native storage.  Just returns a unique
     * serial number.
     */
    static Bitmap.NativeWrapper allocNativeStorage(int width, int height) {
        int nativeData;

        synchronized (Bitmap.class) {
            nativeData = sSerial++;
        }

        Bitmap.NativeWrapper wrapper = new Bitmap.NativeWrapper(nativeData);
        PhantomWrapper phan = new PhantomWrapper(wrapper, sPhantomQueue,
                nativeData);
        sPhantomList.add(phan);
        wrapper.mPhantomWrapper = phan;
        return wrapper;
    }

    static void freeNativeStorage(int nativeDataPtr, CountDownLatch freeSignal) {
        System.out.println("freeNativeStorage: " + nativeDataPtr);
        // Wake up the main thread that is [or will be] blocked until this native data is freed.
        freeSignal.countDown();
    }

    /*
     * Wraps a native data pointer in an object.  When this object is no
     * longer referenced, we free the native data.
     */
    static class NativeWrapper {
        public NativeWrapper(int nativeDataPtr) {
            mNativeData = nativeDataPtr;
        }
        public int mNativeData;

        // The PhantomWrapper corresponding to this NativeWrapper.
        public PhantomWrapper mPhantomWrapper;

        /*
        @Override
        protected void finalize() throws Throwable {
            System.out.println("finalized " + mNativeData);
        }
        */
    }
}

/*
 * Keep an eye on the native data.
 *
 * We keep a copy of the native data pointer value, and set the wrapper
 * as our referent.  We need the copy because you can't get the referred-to
 * object back out of a PhantomReference.
 */
class PhantomWrapper extends PhantomReference {
    PhantomWrapper(Bitmap.NativeWrapper wrapper,
        ReferenceQueue<PhantomWrapper> queue, int nativeDataPtr)
    {
        super(wrapper, queue);
        mNativeData = nativeDataPtr;
    }

    public int mNativeData;
    // This will be signaled once mNativeData has been freed.
    public CountDownLatch mFreeSignal = new CountDownLatch(1);
}

/*
 * Thread that watches for un-referenced bitmap data.
 */
class BitmapWatcher extends Thread {
    ReferenceQueue<PhantomWrapper> mQueue;

    BitmapWatcher(ReferenceQueue<PhantomWrapper> queue) {
        mQueue = queue;
        setName("Bitmap Watcher");
    }

    public void run() {
        while (true) {
            try {
                PhantomWrapper ref = (PhantomWrapper) mQueue.remove();
                //System.out.println("dequeued ref " + ref.mNativeData +
                //    " - " + ref);
                Bitmap.freeNativeStorage(ref.mNativeData, ref.mFreeSignal);
            } catch (InterruptedException ie) {
                System.out.println("intr");
                break;
            }
        }
    }

    public void shutDown() {
        interrupt();
    }
}
