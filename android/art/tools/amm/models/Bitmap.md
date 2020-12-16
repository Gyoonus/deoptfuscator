# Bitmap Model

The value of the Bitmap model is the sum of bytes used for native pixel data
of instances of `android.graphics.Bitmap`. It is calculated by summing for
each instance `x` of `android.graphics.Bitmap`:

    x.getAllocationByteCount()

The actionable breakdown of the Bitmap model is a breakdown by
`android.graphics.Bitmap` instance, including width, height, and ideally a
thumbnail image of each bitmap.

For example, an 800 x 600 bitmap instance using the `ARGB_8888` pixel format
with native pixel data will be shown as an 800 x 600 bitmap instance taking up
1875 kB.
