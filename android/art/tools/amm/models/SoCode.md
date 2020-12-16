# Shared Native Code Model

The value of the Shared Native Code model is the sum of the virtual memory
sizes of all loaded `.so` files. It is calculated by reading `/proc/self/maps`.

The actionable breakdown of the shared native code model is a breakdown by
library name. Unfortunately, due to technical limitations, this does not
include information about what caused a library to be loaded, whether the
library was loaded by the app or the platform, the library dependency graph,
or what is causing a library to remain loaded. Individual `.so` files can be
further broken down using tools such as `readelf`.

For example, for an application `AmmTest.apk` that includes `libammtestjni.so` as a
native library that loads 36 KB worth of memory regions, `BaseClassLoader` will
be shown with library
`/data/app/com.android.amm.test-_uHI4CJWpeoztbjN6Tr-Nw==/lib/arm64/libammtestjni.so`
taking up 36 KB.
