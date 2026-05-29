## Last Run

Date: 2026-05-29

Goal: make the SobelMotion debug flow use real camera data and print software-vs-hardware timing.

Changes made:
- The temporary pclk pipeline stage in `modules/camera/verilog/camera.v` was reverted because the camera pclk domain already passed timing and the remaining failures were in `s_systemClock`/`s_systemClockX2`.
- Removed the synthetic gradient, synthetic rectangle, standalone motion4, threshold sweep, and cycle timing debug tests from the active SobelMotion test flow.
- Default debug sequence now runs real camera data only:
  - TEST 1 raw RGB565 camera view.
  - TEST 2 hardware grayscale camera view.
  - TEST 3 software Sobel from one real RGB565 camera capture, with timing for capture, RGB565-to-gray, Sobel-only, and CPU total after capture.
  - TEST 4 hardware streaming Sobel camera view, with average capture timing.
  - TEST 5 motion4 on real streaming Sobel camera frames, with average hardware capture and full-frame compare timing.
- Cycle counter CI ID 9 is used only to measure elapsed cycles, never to control loops or phase changes.
- Timer/counter CI is not used for program flow.

Verification:
- `cd programs/sobelMotion && make clean mem TARGET=OR1420` passed and regenerated `sobelMotion.mem`.
- `cd systems/singleCore/sandbox && ../scripts/synthesizeOr1420.sh` produced `or1420SingleCore.bit`; upload failed here because no FTDI/JTAG device was available.

Observed risks:
- Yosys reported SCCs during ABC9, although final `check` reported 0 problems.
- nextpnr timing still fails for `s_systemClock` at the 74.25 MHz constraint, reporting about 45.75 MHz after routing. This may be an existing VP timing issue or may need pipelining/constraint review before final bitstream confidence.

Next board test:
- Program the board locally.
- Confirm TEST 1 raw RGB565 image.
- Confirm TEST 2 grayscale camera mode.
- Confirm TEST 3 software Sobel timing from the real camera capture.
- Confirm TEST 4 streaming Sobel timing.
- Confirm TEST 5 real-frame motion4 timing/counts.
