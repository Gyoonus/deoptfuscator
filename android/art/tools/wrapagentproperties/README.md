# wrapagentproperties

wrapagentproperties is a JVMTI agent that lets one change the returned values of
an agents GetSystemPropert{y,ies} calls.

# Usage
### Build
>    `make libwrapagentproperties`  # or 'make libwrapagentpropertiesd' with debugging checks enabled

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line
#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so -agentpath:$ANDROID_HOST_OUT/lib64/libwrapagentproperties.so=/path/to/prop.file,/path/to/agent=agent-args -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise libtitrace agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

### prop file format.

The property file is a text file containing the values of java properties you
wish to override. The format is property=value on each line. Blank lines and
lines beginning with "#" are ignored.

#### Example prop file

    # abc.prop
    abc.def=123
    def.hij=a big deal
