/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static art.Redefinition.doCommonClassRedefinition;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedList;

public class Main {

  // TODO We should make this run on the RI.
  /**
   * This test cannot be run on the RI.
   */
  private static final byte[] CLASS_BYTES = new byte[0];

  // TODO It might be a good idea to replace this hard-coded Object definition with a
  // retransformation based test.
  /**
   * Base64 encoding of the following smali file.
   *
   *  .class public Ljava/lang/Object;
   *  .source "Object.java"
   *  # instance fields
   *  .field private transient shadow$_klass_:Ljava/lang/Class;
   *      .annotation system Ldalvik/annotation/Signature;
   *          value = {
   *              "Ljava/lang/Class",
   *              "<*>;"
   *          }
   *      .end annotation
   *  .end field
   *
   *  .field private transient shadow$_monitor_:I
   *  # direct methods
   *  .method public constructor <init>()V
   *      .registers 1
   *      .prologue
   *      invoke-static {p0}, Lart/test/TestWatcher;->NotifyConstructed(Ljava/lang/Object;)V
   *      return-void
   *  .end method
   *
   *  .method static identityHashCode(Ljava/lang/Object;)I
   *      .registers 7
   *      .prologue
   *      iget v0, p0, Ljava/lang/Object;->shadow$_monitor_:I
   *      const/high16 v3, -0x40000000    # -2.0f
   *      const/high16 v2, -0x80000000
   *      const v1, 0xfffffff
   *      const/high16 v4, -0x40000000    # -2.0f
   *      and-int/2addr v4, v0
   *      const/high16 v5, -0x80000000
   *      if-ne v4, v5, :cond_15
   *      const v4, 0xfffffff
   *      and-int/2addr v4, v0
   *      return v4
   *      :cond_15
   *      invoke-static {p0}, Ljava/lang/Object;->identityHashCodeNative(Ljava/lang/Object;)I
   *      move-result v4
   *      return v4
   *  .end method
   *
   *  .method private static native identityHashCodeNative(Ljava/lang/Object;)I
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   *
   *  .method private native internalClone()Ljava/lang/Object;
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   *
   *
   *  # virtual methods
   *  .method protected clone()Ljava/lang/Object;
   *      .registers 4
   *      .annotation system Ldalvik/annotation/Throws;
   *          value = {
   *              Ljava/lang/CloneNotSupportedException;
   *          }
   *      .end annotation
   *
   *      .prologue
   *      instance-of v0, p0, Ljava/lang/Cloneable;
   *      if-nez v0, :cond_2d
   *      new-instance v0, Ljava/lang/CloneNotSupportedException;
   *      new-instance v1, Ljava/lang/StringBuilder;
   *      invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V
   *      const-string/jumbo v2, "Class "
   *      invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v1
   *      invoke-virtual {p0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
   *      move-result-object v2
   *      invoke-virtual {v2}, Ljava/lang/Class;->getName()Ljava/lang/String;
   *      move-result-object v2
   *      invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v1
   *      const-string/jumbo v2, " doesn\'t implement Cloneable"
   *      invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v1
   *      invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
   *      move-result-object v1
   *      invoke-direct {v0, v1}, Ljava/lang/CloneNotSupportedException;-><init>(Ljava/lang/String;)V
   *      throw v0
   *      :cond_2d
   *      invoke-direct {p0}, Ljava/lang/Object;->internalClone()Ljava/lang/Object;
   *      move-result-object v0
   *      return-object v0
   *  .end method
   *
   *  .method public equals(Ljava/lang/Object;)Z
   *      .registers 3
   *      .prologue
   *      if-ne p0, p1, :cond_4
   *      const/4 v0, 0x1
   *      :goto_3
   *      return v0
   *      :cond_4
   *      const/4 v0, 0x0
   *      goto :goto_3
   *  .end method
   *
   *  .method protected finalize()V
   *      .registers 1
   *      .annotation system Ldalvik/annotation/Throws;
   *          value = {
   *              Ljava/lang/Throwable;
   *          }
   *      .end annotation
   *      .prologue
   *      return-void
   *  .end method
   *
   *  .method public final getClass()Ljava/lang/Class;
   *      .registers 2
   *      .annotation system Ldalvik/annotation/Signature;
   *          value = {
   *              "()",
   *              "Ljava/lang/Class",
   *              "<*>;"
   *          }
   *      .end annotation
   *      .prologue
   *      iget-object v0, p0, Ljava/lang/Object;->shadow$_klass_:Ljava/lang/Class;
   *      return-object v0
   *  .end method
   *
   *  .method public hashCode()I
   *      .registers 2
   *      .prologue
   *      invoke-static {p0}, Ljava/lang/Object;->identityHashCode(Ljava/lang/Object;)I
   *      move-result v0
   *      return v0
   *  .end method
   *
   *  .method public final native notify()V
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   *
   *  .method public final native notifyAll()V
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   *
   *  .method public toString()Ljava/lang/String;
   *      .registers 3
   *      .prologue
   *      new-instance v0, Ljava/lang/StringBuilder;
   *      invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
   *      invoke-virtual {p0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
   *      move-result-object v1
   *      invoke-virtual {v1}, Ljava/lang/Class;->getName()Ljava/lang/String;
   *      move-result-object v1
   *      invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v0
   *      const-string/jumbo v1, "@"
   *      invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v0
   *      invoke-virtual {p0}, Ljava/lang/Object;->hashCode()I
   *      move-result v1
   *      invoke-static {v1}, Ljava/lang/Integer;->toHexString(I)Ljava/lang/String;
   *      move-result-object v1
   *      invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
   *      move-result-object v0
   *      invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
   *      move-result-object v0
   *      return-object v0
   *  .end method
   *
   *  .method public final native wait()V
   *      .annotation system Ldalvik/annotation/Throws;
   *          value = {
   *              Ljava/lang/InterruptedException;
   *          }
   *      .end annotation
   *
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   *
   *  .method public final wait(J)V
   *      .registers 4
   *      .annotation system Ldalvik/annotation/Throws;
   *          value = {
   *              Ljava/lang/InterruptedException;
   *          }
   *      .end annotation
   *      .prologue
   *      const/4 v0, 0x0
   *      invoke-virtual {p0, p1, p2, v0}, Ljava/lang/Object;->wait(JI)V
   *      return-void
   *  .end method
   *
   *  .method public final native wait(JI)V
   *      .annotation system Ldalvik/annotation/Throws;
   *          value = {
   *              Ljava/lang/InterruptedException;
   *          }
   *      .end annotation
   *
   *      .annotation build Ldalvik/annotation/optimization/FastNative;
   *      .end annotation
   *  .end method
   */
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQDUlMR9j03MYuOKekKs2p7zJzu2IfDb7RlMCgAAcAAAAHhWNBIAAAAAAAAAAIgJAAA6" +
      "AAAAcAAAABEAAABYAQAADQAAAJwBAAACAAAAOAIAABYAAABIAgAAAQAAAPgCAAA0BwAAGAMAABgD" +
      "AAA2AwAAOgMAAEADAABIAwAASwMAAFMDAABWAwAAWgMAAF0DAABgAwAAZAMAAGgDAACAAwAAnwMA" +
      "ALsDAADoAwAA+gMAAA0EAAA1BAAATAQAAGEEAACDBAAAlwQAAKsEAADGBAAA3QQAAPAEAAD9BAAA" +
      "AAUAAAQFAAAJBQAADQUAABAFAAAUBQAAHAUAACMFAAArBQAANQUAAD8FAABIBQAAUgUAAGQFAAB8" +
      "BQAAiwUAAJUFAACnBQAAugUAAM0FAADVBQAA3QUAAOgFAADtBQAA/QUAAA8GAAAcBgAAJgYAAC0G" +
      "AAAGAAAACAAAAAwAAAANAAAADgAAAA8AAAARAAAAEgAAABMAAAAUAAAAFQAAABYAAAAXAAAAGAAA" +
      "ABkAAAAcAAAAIAAAAAYAAAAAAAAAAAAAAAcAAAAAAAAAPAYAAAkAAAAGAAAAAAAAAAkAAAALAAAA" +
      "AAAAAAkAAAAMAAAAAAAAAAoAAAAMAAAARAYAAAsAAAANAAAAVAYAABwAAAAPAAAAAAAAAB0AAAAP" +
      "AAAATAYAAB4AAAAPAAAANAYAAB8AAAAPAAAAPAYAAB8AAAAPAAAAVAYAACEAAAAQAAAAPAYAAAsA" +
      "BgA0AAAACwAAADUAAAACAAoAGgAAAAYABAAnAAAABwALAAMAAAAJAAUANgAAAAsABwADAAAACwAD" +
      "ACMAAAALAAwAJAAAAAsABwAlAAAACwACACYAAAALAAAAKAAAAAsAAQApAAAACwABACoAAAALAAMA" +
      "KwAAAAsABwAxAAAACwAHADIAAAALAAQANwAAAAsABwA5AAAACwAIADkAAAALAAkAOQAAAA0ABwAD" +
      "AAAADQAGACIAAAANAAQANwAAAAsAAAABAAAA/////wAAAAAbAAAA0AYAAD4JAAAAAAAAHCBkb2Vz" +
      "bid0IGltcGxlbWVudCBDbG9uZWFibGUAAigpAAQ8Kj47AAY8aW5pdD4AAUAABkNsYXNzIAABSQAC" +
      "SUwAAUoAAUwAAkxJAAJMTAAWTGFydC90ZXN0L1Rlc3RXYXRjaGVyOwAdTGRhbHZpay9hbm5vdGF0" +
      "aW9uL1NpZ25hdHVyZTsAGkxkYWx2aWsvYW5ub3RhdGlvbi9UaHJvd3M7ACtMZGFsdmlrL2Fubm90" +
      "YXRpb24vb3B0aW1pemF0aW9uL0Zhc3ROYXRpdmU7ABBMamF2YS9sYW5nL0NsYXNzABFMamF2YS9s" +
      "YW5nL0NsYXNzOwAmTGphdmEvbGFuZy9DbG9uZU5vdFN1cHBvcnRlZEV4Y2VwdGlvbjsAFUxqYXZh" +
      "L2xhbmcvQ2xvbmVhYmxlOwATTGphdmEvbGFuZy9JbnRlZ2VyOwAgTGphdmEvbGFuZy9JbnRlcnJ1" +
      "cHRlZEV4Y2VwdGlvbjsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABlM" +
      "amF2YS9sYW5nL1N0cmluZ0J1aWxkZXI7ABVMamF2YS9sYW5nL1Rocm93YWJsZTsAEU5vdGlmeUNv" +
      "bnN0cnVjdGVkAAtPYmplY3QuamF2YQABVgACVkoAA1ZKSQACVkwAAVoAAlpMAAZhcHBlbmQABWNs" +
      "b25lAAZlcXVhbHMACGZpbmFsaXplAAhnZXRDbGFzcwAHZ2V0TmFtZQAIaGFzaENvZGUAEGlkZW50" +
      "aXR5SGFzaENvZGUAFmlkZW50aXR5SGFzaENvZGVOYXRpdmUADWludGVybmFsQ2xvbmUACGxvY2tX" +
      "b3JkABBsb2NrV29yZEhhc2hNYXNrABFsb2NrV29yZFN0YXRlSGFzaAARbG9ja1dvcmRTdGF0ZU1h" +
      "c2sABm1pbGxpcwAGbm90aWZ5AAlub3RpZnlBbGwAA29iagAOc2hhZG93JF9rbGFzc18AEHNoYWRv" +
      "dyRfbW9uaXRvcl8AC3RvSGV4U3RyaW5nAAh0b1N0cmluZwAFdmFsdWUABHdhaXQAAAIAAAABAAAA" +
      "AQAAAAsAAAABAAAAAAAAAAEAAAABAAAAAQAAAAwAAgQBOBwBGAcCBAE4HAEYCgIDATgcAhcQFwIC" +
      "BAE4HAEYDgAFAAIDATgcAxcBFxAXAgAAAAAAAAAAAAEAAABaBgAAAgAAAGIGAAB8BgAAAQAAAGIG" +
      "AAABAAAAagYAAAEAAAB0BgAAAQAAAHwGAAABAAAAfwYAAAAAAAABAAAACgAAAAAAAAAAAAAAsAYA" +
      "AAUAAACUBgAABwAAALgGAAAIAAAAyAYAAAsAAADABgAADAAAAMAGAAANAAAAwAYAAA4AAADABgAA" +
      "EAAAAJwGAAARAAAAqAYAABIAAACcBgAAKAAHDgBwATQHDi0DAC0BLQMDMAEtAwIvATwDAS4BeFsA" +
      "7AEABw5LARoPOsYArAEBNAcOAMUEAAcOAEEABw4AaAAHDgCRAgAHDgCmAwExBw5LAAAAAQABAAEA" +
      "AAA4BwAABAAAAHEQAAAAAA4ABwABAAEAAAA9BwAAGgAAAFJgAQAVAwDAFQIAgBQB////DxUEAMC1" +
      "BBUFAIAzVAcAFAT///8PtQQPBHEQCwAGAAoEDwQEAAEAAgAAAFkHAAAyAAAAIDAIADkAKwAiAAcA" +
      "IgENAHAQEwABABsCBQAAAG4gFAAhAAwBbhAIAAMADAJuEAEAAgAMAm4gFAAhAAwBGwIAAAAAbiAU" +
      "ACEADAFuEBUAAQAMAXAgAgAQACcAcBAMAAMADAARAAMAAgAAAAAAZQcAAAYAAAAzIQQAEhAPABIA" +
      "KP4BAAEAAAAAAGwHAAABAAAADgAAAAIAAQAAAAAAcgcAAAMAAABUEAAAEQAAAAIAAQABAAAAdwcA" +
      "AAUAAABxEAoAAQAKAA8AAAADAAEAAgAAAHwHAAApAAAAIgANAHAQEwAAAG4QCAACAAwBbhABAAEA" +
      "DAFuIBQAEAAMABsBBAAAAG4gFAAQAAwAbhAJAAIACgFxEAMAAQAMAW4gFAAQAAwAbhAVAAAADAAR" +
      "AAAABAADAAQAAACCBwAABQAAABIAbkASACEDDgAAAgQLAIIBAYIBBIGABIwPBgikDwGKAgABggIA" +
      "BQToDwEB3BABBPgQARGMEQEBpBEEkQIAAZECAAEBwBEBkQIAARGkEgGRAgAAABAAAAAAAAAAAQAA" +
      "AAAAAAABAAAAOgAAAHAAAAACAAAAEQAAAFgBAAADAAAADQAAAJwBAAAEAAAAAgAAADgCAAAFAAAA" +
      "FgAAAEgCAAAGAAAAAQAAAPgCAAACIAAAOgAAABgDAAABEAAABQAAADQGAAAEIAAABgAAAFoGAAAD" +
      "EAAACQAAAIwGAAAGIAAAAQAAANAGAAADIAAACQAAADgHAAABIAAACQAAAIwHAAAAIAAAAQAAAD4J" +
      "AAAAEAAAAQAAAIgJAAA=");

  private static final String LISTENER_LOCATION =
      System.getenv("DEX_LOCATION") + "/980-redefine-object-ex.jar";

  private static Method doEnableReporting;
  private static Method doDisableReporting;

  private static void DisableReporting() {
    if (doDisableReporting == null) {
      return;
    }
    try {
      doDisableReporting.invoke(null);
    } catch (Exception e) {
      throw new Error("Unable to disable reporting!");
    }
  }

  private static void EnableReporting() {
    if (doEnableReporting == null) {
      return;
    }
    try {
      doEnableReporting.invoke(null);
    } catch (Exception e) {
      throw new Error("Unable to enable reporting!");
    }
  }

  public static void main(String[] args) {
    doTest();
  }

  private static void ensureTestWatcherInitialized() {
    try {
      // Make sure the TestWatcher class can be found from the Object <init> function.
      addToBootClassLoader(LISTENER_LOCATION);
      // Load TestWatcher from the bootclassloader and make sure it is initialized.
      Class<?> testwatcher_class = Class.forName("art.test.TestWatcher", true, null);
      doEnableReporting = testwatcher_class.getDeclaredMethod("EnableReporting");
      doDisableReporting = testwatcher_class.getDeclaredMethod("DisableReporting");
    } catch (Exception e) {
      throw new Error("Exception while making testwatcher", e);
    }
  }

  // NB This function will cause 2 objects of type "Ljava/nio/HeapCharBuffer;" and
  // "Ljava/nio/HeapCharBuffer;" to be allocated each time it is called.
  private static void safePrintln(Object o) {
    DisableReporting();
    System.out.println("\t" + o);
    EnableReporting();
  }

  private static void throwFrom(int depth) throws Exception {
    if (depth <= 0) {
      throw new Exception("Throwing the exception");
    } else {
      throwFrom(depth - 1);
    }
  }

  public static void doTest() {
    safePrintln("Initializing and loading the TestWatcher class that will (eventually) be " +
                "notified of object allocations");
    // Make sure the TestWatcher class is initialized before we do anything else.
    ensureTestWatcherInitialized();
    safePrintln("Allocating an j.l.Object before redefining Object class");
    // Make sure these aren't shown.
    Object o = new Object();
    safePrintln("Allocating a Transform before redefining Object class");
    Transform t = new Transform();

    // Redefine the Object Class.
    safePrintln("Redefining the Object class to add a hook into the <init> method");
    doCommonClassRedefinition(Object.class, CLASS_BYTES, DEX_BYTES);

    safePrintln("Allocating an j.l.Object after redefining Object class");
    Object o2 = new Object();
    safePrintln("Allocating a Transform after redefining Object class");
    Transform t2 = new Transform();

    // This shouldn't cause the Object constructor to be run.
    safePrintln("Allocating an int[] after redefining Object class");
    int[] abc = new int[12];

    // Try adding stuff to an array list.
    safePrintln("Allocating an array list");
    ArrayList<Object> al = new ArrayList<>();
    safePrintln("Adding a bunch of stuff to the array list");
    al.add(new Object());
    al.add(new Object());
    al.add(o2);
    al.add(o);
    al.add(t);
    al.add(t2);
    al.add(new Transform());

    // Try adding stuff to a LinkedList
    safePrintln("Allocating a linked list");
    LinkedList<Object> ll = new LinkedList<>();
    safePrintln("Adding a bunch of stuff to the linked list");
    ll.add(new Object());
    ll.add(new Object());
    ll.add(o2);
    ll.add(o);
    ll.add(t);
    ll.add(t2);
    ll.add(new Transform());

    // Try making an exception.
    safePrintln("Throwing from down 4 stack frames");
    try {
      throwFrom(4);
    } catch (Exception e) {
      safePrintln("Exception caught.");
    }

    safePrintln("Finishing test!");
  }

  private static native void addToBootClassLoader(String s);
}
