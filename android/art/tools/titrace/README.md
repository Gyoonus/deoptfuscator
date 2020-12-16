# Titrace

Titrace is a bytecode instruction tracing tool that uses JVMTI and works on both ART and the RI.

# Usage
### Build
>    `make libtitrace`  # or 'make libtitraced' with debugging checks enabled

The libraries will be built for 32-bit, 64-bit, host and target. Below examples assume you want to use the 64-bit version.
### Command Line
#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so -agentpath:$ANDROID_HOST_OUT/lib64/libtitrace.so -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise libtitrace agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.
#### Reference Implementation
>    `java  -agentpath:$ANDROID_HOST_OUT/lib64/libtitrace.so helloworld`

Only needs `-agentpath` to be specified.
### Android Applications
Replace __com.littleinc.orm_benchmark__ with the name of your application below.
#### Enable permissions for attaching an agent
Normal applications require that `debuggable=true` to be set in their AndroidManifest.xml.

By using a *eng* or *userdebug* build of Android, we can override this requirement:
> `adb root`
> `adb shell setprop dalvik.vm.dex2oat-flags --debuggable`

Then restart the runtime to pick it up.
> `adb shell stop && adb shell start`

If this step is skipped, attaching the agent will not succeed.
#### Deploy agent to device
The agent must be located in an app-accessible directory.

> `adb push $ANDROID_PRODUCT_OUT/system/lib64/libtitrace.so  /data/local/tmp`

Upload to device first (it gets shell/root permissions).

> `adb shell run-as com.littleinc.orm_benchmark 'cp /data/local/tmp/libtitrace.so /data/data/com.littleinc.orm_benchmark/files/libtitrace.so'`

Copy the agent into an app-accessible directory, and make the file owned by the app.

#### Attach agent to application

##### Option 1: Attach the agent before any app code runs.
> `adb shell am start --attach-agent /data/data/com.littleinc.orm_benchmark/files/libtitrace.so com.littleinc.orm_benchmark/.MainActivity`

Note: To determine the arguments to `am start`, launch the application manually first and then look for this in logcat:

> 09-14 13:28:08.680  7584  8192 I ActivityManager: Start proc 17614:com.littleinc.orm_benchmark/u0a138 for activity **com.littleinc.orm_benchmark/.MainActivity**

##### Option 2: Attach the agent to an already-running app.
> `adb shell am attach-agent $(pid com.littleinc.orm_benchmark)  /data/data/com.littleinc.orm_benchmark/files/libtitrace.so`

### Printing the Results
All statitics gathered during the trace are printed automatically when the program normally exists. In the case of Android applications, they are always killed, so we need to manually print the results.

>    `kill -SIGQUIT $(pid com.littleinc.orm_benchmark)`

Will initiate a dump of the agent (to logcat).

