# Graphics Models

There are three components to the graphics model, each modeling EGL memory
use:
1. For each `android.view.TextureView` instance:
    2 * (4 * width * height)

2. For each `android.view.Surface$HwuiContext` instance:
    3 * (4 * width * height)

3. For each initialized `android.view.ThreadedRenderer`:
    3 * (4 * width * height)

Note: 4 is the number of bytes per pixel. 2 or 3 is the maximum number of
buffers that may be allocated.

The actionable breakdown is the breakdown by `TextureView`,
`Surface$HwuiContext` and `ThreadedRenderer` instance, with further details
about the width and height associated with each instance.

For example, an application with a single 64x256 `TextureView` instance will
be shown as taking up 128 KB.
