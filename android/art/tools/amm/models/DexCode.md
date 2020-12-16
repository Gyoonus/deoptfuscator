# Dex Code Model

The value of the Dex Code model is the sum of the original uncompressed file
sizes of all loaded dex files. It is calculated using the best approximation
of the dex file size available to us on device. On Android O, for example,
this can be approximated as the virtual size of the corresponding memory
mapped `.vdex` file read from `/proc/self/maps`. Different Android platform
versions and scenarios may require different approximations.

The actionable breakdown of the dex code model is a breakdown by
`dalvik.system.DexFile` instance. Further breakdown of individual dex files
can be achieved using tools such as dexdump.

For example, for an application `AmmTest.apk` that has a single `classes.dex` file
that is 500 KB uncompressed, the `DexFile` instance for
`/data/app/com.android.amm.test-_uHI4CJWpeoztbjN6Tr-Nw==/base.apk` is shown as
Taking up 500 KB (or the best available approximation thereof).
