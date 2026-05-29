# Sobel movement detector project changes

This version implements a Sobel-based movement detector with both a software-only baseline and an accelerated real-time path.

## Added hardware

### `modules/camera/verilog/camera.v`

The camera module still supports the original 8-bit grayscale streaming mode. Two new camera custom-instruction commands were added under the existing camera CI ID `7`:

- `A = 8, B = 0`: stream normal 8-bit grayscale frames.
- `A = 8, B = 1`: enable streaming Sobel mode.
- `A = 9, B = threshold`: set the Sobel threshold. The reset default is `128`.

In Sobel mode, the camera path performs:

```text
RGB565 camera stream -> 8-bit grayscale -> 3-line Sobel window -> binary edge pixels -> framebuffer
```

The Sobel output is packed as four 8-bit edge pixels per 32-bit word, matching the grayscale framebuffer mode used by the HDMI/VGA controller.

### `modules/motion4Ci/verilog/motion4Ise.v`

A new custom instruction with ID `8` was added. It compares four packed 8-bit pixels from two frames:

```c
result = motion4_ci(current_word, previous_word);
```

Result format:

- `result[3:0]`: changed-pixel mask for the four bytes.
- `result[6:4]`: number of changed bytes, from 0 to 4.

This keeps meaningful software running on the OpenRISC CPU: the Sobel image is produced by the stream accelerator, while the CPU performs frame-to-frame motion analysis using the new custom instruction.

## Added software

### `programs/sobelMotion/`

This program runs two phases:

1. **Software-only baseline:** captures an RGB565 frame, converts it to grayscale in C, and runs the original software Sobel implementation. It counts how many frames are processed in 5 seconds.
2. **Accelerated path:** enables camera streaming Sobel mode, captures edge frames, compares current and previous edge frames with the `motion4` custom instruction, and prints motion statistics.

Build from:

```bash
cd programs/sobelMotion
make clean mem TARGET=OR1420
```

or use the target name expected by your local course scripts if they wrap the makefile.

## Modified synthesis scripts

The Yosys scripts now include:

- `modules/grayscaleCi/verilog/rgb565Grayscale.v`
- `modules/motion4Ci/verilog/motion4Ise.v`

The main active top-level in the provided VP is:

```text
systems/singleCore/verilog/or1420SingleCore.v
```

I also patched the extra counter top-level that was present in the uploaded zip:

```text
systems/verilog/or1420SingleCore_counter.v
```

## Notes

- I could not run Yosys or the OR1K compiler in this environment because those tools are not installed here.
- The Sobel stream output is vertically delayed by the nature of the 3-line streaming window. This is normal for streaming 3x3 filters and is acceptable for the movement detection use case.
- The software baseline still uses the original `programs/camera/src/sobel.c` algorithm style: `|Gx| + |Gy|` followed by thresholding.
