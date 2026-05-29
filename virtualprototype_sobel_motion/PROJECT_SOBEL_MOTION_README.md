# Sobel Motion Detector Project Notes

Date updated: 2026-05-29

This project implements a Sobel-based motion detector on the CS-476 OpenRISC/OR1420 virtual prototype. The final accelerated architecture keeps significant software running on the OpenRISC CPU, but moves the Sobel edge extraction into the camera streaming path.

## Final Architecture

### Hardware Streaming Sobel

Active hardware file:

```text
modules/camera/verilog/camera.v
```

The camera module supports three output modes through the existing camera custom instruction ID `7`:

```text
command 8, value 0: raw RGB565 capture
command 8, value 1: 8-bit grayscale capture
command 8, value 2: streaming Sobel edge capture
command 9, value N: set hardware Sobel threshold
```

In mode 2 the camera path performs:

```text
RGB565 camera stream -> grayscale -> 3-line Sobel window -> binary edge framebuffer
```

The output is an 8-bit framebuffer, packed as four 8-bit pixels per 32-bit word, matching the existing grayscale VGA/HDMI framebuffer mode.

### Motion Custom Instruction

Active hardware file:

```text
modules/motion4Ci/verilog/motion4Ise.v
```

Custom instruction ID `8` implements `motion4_ci(current, previous)`. It compares four packed 8-bit edge pixels from the current and previous edge frame.

Result format:

```text
result[3:0] = changed-pixel mask
result[6:4] = changed-pixel count, 0..4
```

The CPU uses this custom instruction for tile-based frame-to-frame motion detection.

### CPU Tile Motion

Active software file:

```text
programs/sobelMotion/src/sobel_motion.c
```

The final CPU motion step samples 16x16 tiles using `motion16_tiles_frame()`. This avoids a full-frame motion scan while still keeping meaningful OpenRISC software work in the final accelerated design.

Current selected tile settings:

```c
#define MOTION_TILE_SIZE 16
#define MOTION_TILE_SAMPLE_STEP 8
#define MOTION_TILE_ACTIVE_THRESHOLD 6
#define MOTION_ACTIVE_TILES_THRESHOLD 4
#define MOTION_SUMMARY_COLUMNS 16
#define MOTION_SUMMARY_ROWS 16
```

The final UART output prints changed samples, active tiles, motion detected, average timings, and a 16x16 accumulated active-tile summary.

## Software Comparison Modes

### Temporal-Difference Software Sobel

The teammate-style temporal-difference Sobel is integrated as `TEST 3` in the debug flow and is controlled by:

```c
#define RUN_TEMP_DIFF_SW_TEST 1
```

It is software-only and uses raw RGB565 camera frames:

```text
capture RGB565 frame
convert current pixel to grayscale
compare against previous grayscale frame
store absolute temporal difference in three rolling diff lines
run Sobel on the temporal-difference image
display moving-edge result
```

Important: this algorithm is intentionally not implemented in `camera.v`. A hardware version would need previous-frame memory reads, updated grayscale writes, and output writes. The current camera accelerator is mostly a streaming write-only path, so the final hardware design stays as streaming Sobel plus CPU tile motion.

### Old Software Debug Test

The old single-frame software Sobel debug test was removed from the active debug flow. It was replaced by the temporal-difference software Sobel comparison.

### Software Baseline

The slower software-only baseline remains available but is disabled by default:

```c
#define RUN_SOFTWARE_BASELINE 0
```

When enabled, it captures RGB565, converts to grayscale in C, runs normal software Sobel in C, and prints timing. It is useful for final report comparison but takes longer to run.

## Current Test Flow

The active flow is:

```text
initialize camera
print resolution, PCLK, FPS, CPU info, image size

if RUN_DEBUG_TESTS:
  TEST 1: raw RGB565 camera capture
  TEST 2: camera 8-bit grayscale mode
  TEST 3: temporal-difference software Sobel comparison
  TEST 4: hardware streaming Sobel only
  TEST 5: hardware streaming Sobel + motion4 tile test

if RUN_SOFTWARE_BASELINE:
  software-only Sobel baseline

always:
  final hardware Sobel + motion4 tile demo
```

The timer custom instruction is not used for program flow. All loops use fixed frame counts.

## Important Switches

```c
#define RUN_DEBUG_TESTS 1
#define RUN_TEMP_DIFF_SW_TEST 1
#define RUN_SOFTWARE_BASELINE 0
#define CAMERA_DEBUG_FRAMES 30
#define TEMP_DIFF_FRAMES 30
#define HW_STREAM_FRAMES 60
#define TEMP_DIFF_SOBEL_THRESHOLD 80
#define TEMP_DIFF_OUTPUT_BINARY 1
#define SOBEL_THRESHOLD_HW SOBEL_THRESHOLD_HW_LOW
```

Current hardware Sobel threshold presets:

```c
#define SOBEL_THRESHOLD_HW_LOW 40
#define SOBEL_THRESHOLD_HW_MEDIUM 80
#define SOBEL_THRESHOLD_HW_HIGH 120
```

## Latest Measured Results

Run date: 2026-05-29

Camera/CPU information:

```text
Resolution: 640 x 480, 307200 pixels
Camera PCLK: 11999 kHz
Camera reported FPS: 15
CPU: about 74249 kHz
Hardware Sobel threshold: 40
Motion thresholds: tile=6, active_tiles=4
```

### Temporal-Difference Software Sobel

Latest coherent measurement:

```text
tempdiff average capture: 6,781,421 cycles, approx 91 ms
tempdiff average grayscale diff/update: 50,251,395 cycles, approx 676 ms
tempdiff average Sobel-on-diff: 116,041,285 cycles, approx 1562 ms
tempdiff average processing after capture: 166,292,680 cycles, approx 2239 ms
tempdiff average total with capture: 173,074,101 cycles, approx 2330 ms
tempdiff software approximate throughput: approx 0.4 FPS
```

This matches the visible behavior: the temporal software Sobel is very slow because it processes the full 640x480 image on the OpenRISC CPU.

### Hardware Streaming Sobel Only

```text
hardware Sobel average capture: 9,857,125 cycles, approx 132 ms
```

This timing is dominated by `takeSingleImageBlocking()`. The Sobel operation is performed during the camera stream, so this is not software Sobel time added after capture.

### Hardware Sobel + Tile Motion

Debug test result:

```text
hardware Sobel average capture in motion test: 9,409,618 cycles, approx 126 ms
motion4 average 16x16 tile compare: 484,718 cycles, approx 6 ms
```

Final demo result:

```text
final hardware average capture: 9,225,115 cycles, approx 124 ms
final motion4 tile average compare: 478,487 cycles, approx 6 ms
final hardware+motion average total: 9,703,602 cycles, approx 130 ms
final hardware+motion approximate throughput: approx 7 FPS
```

The camera reports 15 FPS, or about 66 ms per camera frame. The blocking single-image capture function often spans close to two frame periods, so the measured demo loop is around 7 FPS even though the camera stream itself is 15 FPS.

### Speedups

End-to-end measured speedup using the temporal-difference software comparison:

```text
2330 ms / 130 ms = about 18x faster
```

Motion post-processing improvement from full-frame motion4 to tile-sampled motion4:

```text
old full-frame motion4 compare: about 9,049,321 cycles, approx 121 ms
new tile-sampled motion4 compare: about 478,487 cycles, approx 6 ms
speedup: about 20x
```

## Build Commands

Hardware:

```bash
cd systems/singleCore/sandbox
../scripts/synthesizeOr1420.sh
```

Software:

```bash
cd programs/sobelMotion
make clean mem TARGET=OR1420
```

The latest software build completed successfully after the profiling and test-flow fixes.

## Notes And Fixes Made

- Kept all edits in the `_MINE` project folder.
- Replaced the old software debug test with temporal-difference software Sobel as `TEST 3`.
- Removed duplicate standalone temporal-difference run after debug tests, so it runs only once when debug tests are enabled.
- Fixed bad software profiling caused by 32-bit total overflow and avoided 64-bit profiling because it caused board-side runtime issues.
- Fixed a width/height argument issue by passing plain `width`, `height`, and `pixels` into `run_debug_tests()` instead of passing `camParameters` by value.
- Added `Image size : 640 x 480 (307200 pixels)` printout to verify dimensions at runtime.
- Left VGA text output disabled in platform support to avoid HDMI/display flicker; UART remains the main debug output.
- Reverted the experimental camera pipeline stage. The final camera Sobel path is the non-pipelined streaming version.

## Report Summary

The final project demonstrates that the OpenRISC CPU-only temporal-difference Sobel path is not real-time at VGA resolution, while the hardware streaming Sobel path plus CPU tile motion is much faster. The accelerator computes Sobel edges inline with the camera stream, and the CPU performs reduced tile-based motion analysis using the custom `motion4` instruction.
