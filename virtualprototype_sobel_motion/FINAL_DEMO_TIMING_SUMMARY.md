# Final Demo Timing Summary

## Final Demo Flow

The final `sobelMotion` demo runs:

1. Raw camera color / RGB565
2. Camera grayscale
3. Software implementation: RGB capture -> grayscale -> software Sobel -> XOR motion mask
4. Hardware Sobel -> full-frame software XOR motion mask, no `motion4`
5. Hardware Sobel -> 16x16 tile motion using `motion4`

Extra debug-only comparison tests were removed from the main demo flow.

## Tile Layout

The tile grid uses real 16x16 pixel tiles:

```text
640 / 16 = 40 columns
480 / 16 = 30 rows
1200 tiles total
```

The printed tile map is the real 40x30 tile map. Cells are spaced for readability:

```text
.  .  X  .  .
```

The final tile summary only accumulates active tiles from frames where `motion = 1`.
Frames with `motion = 0` do not contribute `X`s to the printed map.

## Camera Timing

The camera reports:

```text
FPS: 15
```

So the theoretical camera frame period is:

```text
1000 / 15 = 66.7 ms
```

However, measured `takeSingleImageBlocking()` capture time is closer to two frame
periods, so the observed blocking capture throughput is around 7 FPS.

## Measured Timings

### Raw RGB565 Camera

```text
raw RGB565 average capture: 9,867,070 cycles, approx 132 ms
raw RGB565 approximate throughput: approx 7 FPS
```

### Camera Grayscale

```text
camera grayscale average capture: 9,859,901 cycles, approx 132 ms
camera grayscale approximate throughput: approx 7 FPS
```

### Software Sobel-XOR Baseline

```text
software baseline average capture: 8,744,303 cycles, approx 117 ms
software baseline average grayscale: 27,565,396 cycles, approx 371 ms
software baseline average clear Sobel buffer: 9,727,639 cycles, approx 131 ms
software baseline average Sobel: 344,231,363 cycles, approx 4636 ms
software baseline average XOR motion mask: 10,620,985 cycles, approx 143 ms
software baseline average processing after capture: 392,145,383 cycles, approx 5281 ms
software baseline average total with capture: 400,889,686 cycles, approx 5399 ms
software baseline approximate throughput: approx 0 FPS
```

### Hardware Sobel + Full-Frame Software XOR

```text
hardware Sobel full-frame average capture: 9,111,946 cycles, approx 122 ms
hardware Sobel full-frame average XOR mask: 10,607,280 cycles, approx 142 ms
hardware Sobel full-frame average total: 19,719,227 cycles, approx 265 ms
hardware Sobel full-frame approximate throughput: approx 3 FPS
```

### Hardware Sobel + Tile Motion Using `motion4`

```text
hardware Sobel average capture in motion test: 9,072,836 cycles, approx 122 ms
motion4 average 16x16 tile compare: 607,284 cycles, approx 8 ms
hardware Sobel + tile motion average total: 9,680,120 cycles, approx 130 ms
hardware Sobel + tile motion approximate throughput: approx 7 FPS
```

### Removed CI-vs-Software Tile Comparison Test

This was a debug comparison test and is not part of the final demo flow.

```text
Tile CI-vs-SW mismatch frames: 0
tile compare average hardware Sobel capture: 8,543,169 cycles, approx 115 ms
tile compare average motion4 CI: 488,246 cycles, approx 6 ms
tile compare average software byte count: 744,326 cycles, approx 10 ms
```

The tile compare speedup from `motion4` was:

```text
744,326 / 488,246 = about 1.52x
```

Equivalently, `motion4` reduced tile compare time by about 34%.

## Key Conclusions

The camera reports 15 FPS, but the blocking capture function behaves like about
7 FPS because each blocking capture often spans roughly two camera frames.

Software Sobel is by far the main bottleneck:

```text
about 4636 ms just for Sobel
```

Hardware Sobel removes that bottleneck.

Full-frame XOR display is visually useful but still expensive:

```text
about 142 ms just for full-frame XOR/mask write
```

Tile motion is much faster because it samples far fewer pixels. The `motion4`
custom instruction helps specifically because the tile algorithm needs changed-byte
counts, not a full display image.

The best final path is:

```text
hardware streaming Sobel + tile motion using motion4
```

Measured performance:

```text
about 130 ms/frame, about 7 FPS
```
