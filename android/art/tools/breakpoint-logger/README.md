# breakpointlogger

breakpointlogger is a JVMTI agent that lets one set breakpoints that are logged
when they are hit.

# Usage
### Build
>    `make libbreakpointlogger`  # or 'make libbreakpointloggerd' with debugging checks enabled

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line

The agent is loaded using -agentpath like normal. It takes arguments in the
following format:
>     `:class_descriptor:->:methodName::method_sig:@:breakpoint_location:,[...]`

* The breakpoint\_location is a number that's a valid jlocation for the runtime
  being used. On ART this is a dex-pc. Dex-pcs can be found using tools such as
  dexdump and are uint16\_t-offsets from the start of the method. On other
  runtimes jlocations might represent other things.

* Multiple breakpoints can be included in the options, separated with ','s.

* Unlike with most normal debuggers the agent will load the class immediately to
  set the breakpoint. This means that classes might be initialized earlier than
  one might expect. This also means that one cannot set breakpoints on classes
  that cannot be found using standard or bootstrap classloader at startup.

* Deviating from this format or including a breakpoint that cannot be found at
  startup will cause the runtime to abort.

#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libbreakpointlogger.so=Lclass/Name;->methodName()V@0' -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

#### RI
>    `java '-agentpath:libbreakpointlogger.so=Lclass/Name;->methodName()V@0' -cp tmp/helloworld/classes helloworld`

### Output
A normal run will look something like this:

    % ./test/run-test --host --dev --with-agent 'libbreakpointlogger.so=LMain;->main([Ljava/lang/String;)V@0' 001-HelloWorld
    <normal output removed>
    dalvikvm32 W 10-25 10:39:09 18063 18063 breakpointlogger.cc:277] Breakpoint at location: 0x00000000 in method LMain;->main([Ljava/lang/String;)V (source: Main.java:13) thread: main
    Hello, world!

    % ./test/run-test --jvm --dev --with-agent 'libbreakpointlogger.so=LMain;->main([Ljava/lang/String;)V@0' 001-HelloWorld
    <normal output removed>
    java W 10-25 10:39:09 18063 18063 breakpointlogger.cc:277] Breakpoint at location: 0x00000000 in method LMain;->main([Ljava/lang/String;)V (source: Main.java:13) thread: main
    Hello, world!
