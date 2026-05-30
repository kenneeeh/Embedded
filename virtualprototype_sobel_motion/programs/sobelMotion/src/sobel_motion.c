#include <stdio.h>
#include <stdint.h>
#include <ov7670.h>
#include <swap.h>
#include <vga.h>
#include <sobel.h>

#define IMG_W_MAX 640
#define IMG_H_MAX 480
#define MOTION_PRINT_EVERY 5

#define CPU_KHZ_FALLBACK 74249
#define SW_BASELINE_FRAMES 5
#define CAMERA_DEBUG_FRAMES 60
#define SHOW_SOFTWARE_BASELINE_VIEW 1

/* Sobel threshold presets:
 * Lower = more edges/noise, higher = fewer stronger edges.
 */
#define SOBEL_THRESHOLD_SW 160
#define SOBEL_THRESHOLD_HW_LOW 40
#define SOBEL_THRESHOLD_HW_MEDIUM 80
#define SOBEL_THRESHOLD_HW_HIGH 120
#define SOBEL_THRESHOLD_HW SOBEL_THRESHOLD_HW_LOW

/* Motion threshold presets:
 * Lower = more sensitive, higher = fewer false positives.
 */
#define MOTION_TILE_ACTIVE_THRESHOLD_LOW 3
#define MOTION_TILE_ACTIVE_THRESHOLD_MEDIUM 4
#define MOTION_TILE_ACTIVE_THRESHOLD_HIGH 8
#define MOTION_ACTIVE_TILES_THRESHOLD_LOW 2
#define MOTION_ACTIVE_TILES_THRESHOLD_MEDIUM 4
#define MOTION_ACTIVE_TILES_THRESHOLD_HIGH 5
#define MOTION_TILE_ACTIVE_THRESHOLD MOTION_TILE_ACTIVE_THRESHOLD_HIGH
#define MOTION_ACTIVE_TILES_THRESHOLD MOTION_ACTIVE_TILES_THRESHOLD_HIGH

#define MOTION_TILE_SIZE 16
#define MOTION_TILE_SAMPLE_STEP 8
#define MOTION_MAX_TILE_COLUMNS (IMG_W_MAX / MOTION_TILE_SIZE)
#define MOTION_MAX_TILE_ROWS (IMG_H_MAX / MOTION_TILE_SIZE)

typedef struct {
  uint8_t min;
  uint8_t max;
  uint32_t mean;
  uint32_t nonzero;
  uint32_t bright;
  uint32_t border_nonzero;
  uint32_t interior_nonzero;
  uint32_t checksum;
} image_stats_t;

typedef struct {
  uint32_t changed_samples;
  uint32_t active_tiles;
  uint32_t motion_detected;
} motion_stats_t;

volatile uint16_t rgb565[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t grayscale[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t motion_view[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t edge_a[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t edge_b[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));

static uint8_t active_tile_map[MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS];
static uint8_t accumulated_tile_map[MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS];

static inline void camera_set_output_mode(uint32_t mode) {
  /* camera CI command 8:
   *   0 = original RGB565 capture
   *   1 = 8-bit grayscale capture
   *   2 = streaming Sobel edge capture
   */
  asm volatile ("l.nios_rrr r0,%[cmd],%[value],0x7"::[cmd]"r"(8),[value]"r"(mode));
}

static inline void camera_set_sobel_threshold(uint32_t threshold) {
  asm volatile ("l.nios_rrr r0,%[cmd],%[value],0x7"::[cmd]"r"(9),[value]"r"(threshold));
}

static inline uint32_t cycle_counter_ci(void) {
  uint32_t result;
  asm volatile ("l.nios_rrr %[out],r0,r0,0x9":[out]"=r"(result));
  return result;
}

static void print_cycles(const char *label, uint32_t cycles, uint32_t cpu_khz) {
  uint32_t ms = (cpu_khz == 0) ? 0 : cycles / cpu_khz;
  printf("%s: %d cycles, approx %d ms\n", label, cycles, ms);
}

static uint32_t average_cycles(uint32_t total_cycles, uint32_t frames) {
  return (frames == 0) ? 0 : total_cycles / frames;
}

static void print_fps_from_average(const char *label,
                                   uint32_t avg_cycles,
                                   uint32_t cpu_khz) {
  uint32_t avg_ms = (cpu_khz == 0) ? 0 : avg_cycles / cpu_khz;
  uint32_t fps = (avg_ms == 0) ? 0 : 1000 / avg_ms;
  printf("%s: approx %d FPS (%d ms/frame)\n", label, fps, avg_ms);
}

static inline uint32_t motion4_ci(uint32_t current, uint32_t previous) {
  uint32_t result;
  asm volatile ("l.nios_rrr %[out],%[a],%[b],0x8"
                : [out] "=r" (result)
                : [a] "r" (current), [b] "r" (previous));
  return result;
}

static void clear_u8(volatile uint8_t *buffer, uint32_t pixels, uint8_t value) {
  for (uint32_t i = 0; i < pixels; i++) {
    buffer[i] = value;
  }
}

static void show_stable_blank_view(volatile unsigned int *vga,
                                   volatile uint8_t *buffer,
                                   uint32_t pixels) {
  clear_u8(buffer, pixels, 0);
  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) buffer);
}

static image_stats_t image_stats_u8(volatile uint8_t *buffer,
                                    int32_t width,
                                    int32_t height) {
  image_stats_t stats;
  uint32_t sum = 0;

  stats.min = 255;
  stats.max = 0;
  stats.nonzero = 0;
  stats.bright = 0;
  stats.border_nonzero = 0;
  stats.interior_nonzero = 0;
  stats.checksum = 2166136261u;

  for (int32_t y = 0; y < height; y++) {
    for (int32_t x = 0; x < width; x++) {
      uint8_t value = buffer[y * width + x];
      uint32_t is_border = (x < 8 || y < 8 || x >= (width - 8) || y >= (height - 8));

      if (value < stats.min) stats.min = value;
      if (value > stats.max) stats.max = value;
      sum += value;
      stats.checksum = (stats.checksum ^ value) * 16777619u;

      if (value != 0) {
        stats.nonzero++;
        if (is_border) stats.border_nonzero++;
        else stats.interior_nonzero++;
      }
      if (value >= 128) {
        stats.bright++;
      }
    }
  }

  stats.mean = sum / (width * height);
  return stats;
}

static void print_image_stats(const char *label,
                              volatile uint8_t *buffer,
                              int32_t width,
                              int32_t height) {
  image_stats_t stats = image_stats_u8(buffer, width, height);
  printf("%s: min=%d max=%d mean=%d nz=%d bright=%d border=%d interior=%d checksum=0x%08x\n",
         label,
         stats.min,
         stats.max,
         stats.mean,
         stats.nonzero,
         stats.bright,
         stats.border_nonzero,
         stats.interior_nonzero,
         stats.checksum);
}

static void rgb565_to_grayscale_sw(volatile uint16_t *src,
                                   volatile uint8_t *dst,
                                   int32_t width,
                                   int32_t height) {
  for (int32_t line = 0; line < height; line++) {
    for (int32_t pixel = 0; pixel < width; pixel++) {
      uint16_t rgb = swap_u16(src[line * width + pixel]);
      uint32_t red   = ((rgb >> 11) & 0x1F) << 3;
      uint32_t green = ((rgb >> 5)  & 0x3F) << 2;
      uint32_t blue  = (rgb & 0x1F) << 3;
      dst[line * width + pixel] = ((red * 54 + green * 183 + blue * 19) >> 8) & 0xFF;
    }
  }
}

static void print_tile_map_summary(const char *label,
                                   volatile uint8_t *tile_map,
                                   int32_t width,
                                   int32_t height) {
  uint32_t columns = width / MOTION_TILE_SIZE;
  uint32_t rows = height / MOTION_TILE_SIZE;

  printf("%s active tile map (%dx%d tiles, %dx%d pixels each):\n",
         label,
         columns,
         rows,
         MOTION_TILE_SIZE,
         MOTION_TILE_SIZE);

  for (uint32_t ty = 0; ty < rows; ty++) {
    for (uint32_t tx = 0; tx < columns; tx++) {
      putchar(tile_map[ty * columns + tx] ? 'X' : '.');
      if (tx + 1 < columns) {
        putchar(' ');
        putchar(' ');
      }
    }
    putchar('\n');
  }
}

static motion_stats_t motion16_tiles_frame(volatile uint8_t *current,
                                           volatile uint8_t *previous,
                                           int32_t width,
                                           int32_t height) {
  motion_stats_t stats;
  uint32_t tile_row = 0;

  stats.changed_samples = 0;
  stats.active_tiles = 0;
  stats.motion_detected = 0;
  clear_u8(active_tile_map,
           MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS,
           0);

  for (int32_t tile_y = 0; tile_y + MOTION_TILE_SIZE <= height; tile_y += MOTION_TILE_SIZE) {
    uint32_t tile_col = 0;

    for (int32_t tile_x = 0; tile_x + MOTION_TILE_SIZE <= width; tile_x += MOTION_TILE_SIZE) {
      uint32_t tile_motion = 0;

      for (int32_t y = tile_y + (MOTION_TILE_SAMPLE_STEP >> 1);
           y < tile_y + MOTION_TILE_SIZE;
           y += MOTION_TILE_SAMPLE_STEP) {
        for (int32_t x = tile_x; x < tile_x + MOTION_TILE_SIZE; x += MOTION_TILE_SAMPLE_STEP) {
          uint32_t index = y * width + x;
          uint32_t current_word = *((volatile uint32_t *) (current + index));
          uint32_t previous_word = *((volatile uint32_t *) (previous + index));
          uint32_t result = motion4_ci(current_word, previous_word);
          tile_motion += (result >> 4) & 0x7;
        }
      }

      stats.changed_samples += tile_motion;
      if (tile_motion >= MOTION_TILE_ACTIVE_THRESHOLD) {
        active_tile_map[tile_row * (width / MOTION_TILE_SIZE) + tile_col] = 1;
        stats.active_tiles++;
      }

      tile_col++;
    }
    tile_row++;
  }

  stats.motion_detected = (stats.active_tiles >= MOTION_ACTIVE_TILES_THRESHOLD) ? 1 : 0;
  return stats;
}

static uint32_t software_xor_motion_view(volatile uint8_t *current,
                                         volatile uint8_t *previous,
                                         volatile uint8_t *out,
                                         uint32_t pixels) {
  uint32_t changed_pixels = 0;
  uint32_t i = 0;

  for (; i + 3 < pixels; i += 4) {
    uint32_t current_word = *((volatile uint32_t *) (current + i));
    uint32_t previous_word = *((volatile uint32_t *) (previous + i));
    uint32_t diff = current_word ^ previous_word;

    *((volatile uint32_t *) (out + i)) = diff;
    changed_pixels += ((diff & 0x000000FFu) != 0);
    changed_pixels += ((diff & 0x0000FF00u) != 0);
    changed_pixels += ((diff & 0x00FF0000u) != 0);
    changed_pixels += ((diff & 0xFF000000u) != 0);
  }

  for (; i < pixels; i++) {
    uint8_t diff = current[i] ^ previous[i];
    out[i] = diff;
    changed_pixels += (diff != 0);
  }

  return changed_pixels;
}

/* -------------------------------------------------------------------------- */
/* Debug test 3: software-only Sobel first, then XOR motion detection.        */
/* Expected result: white pixels where software Sobel edges changed.          */
/* -------------------------------------------------------------------------- */
static void test_software_sobel_xor_baseline(volatile unsigned int *vga,
                                             uint32_t pixels,
                                             int32_t width,
                                             int32_t height,
                                             uint32_t cpu_khz) {
  uint32_t sw_frames = 0;
  uint32_t total_sw_capture_cycles = 0;
  uint32_t total_sw_grayscale_cycles = 0;
  uint32_t total_sw_clear_cycles = 0;
  uint32_t total_sw_sobel_cycles = 0;
  uint32_t total_sw_xor_cycles = 0;
  uint32_t total_sw_changed_pixels = 0;
  uint32_t last_sw_changed_pixels = 0;
  uint32_t sw_baseline_use_a_as_current = 0;

  printf("\nTEST 3: software Sobel-then-XOR motion baseline for %d frames...\n",
         SW_BASELINE_FRAMES);

  camera_set_output_mode(0);
  show_stable_blank_view(vga, motion_view, pixels);

  clear_u8(edge_a, pixels, 0);
  clear_u8(edge_b, pixels, 0);
  clear_u8(motion_view, pixels, 0);

  /* Prime the previous Sobel frame. This setup pass is not included in the
   * per-frame profile below.
   */
  takeSingleImageBlocking((uint32_t) &rgb565[0]);
  rgb565_to_grayscale_sw(rgb565, grayscale, width, height);
  edgeDetection(grayscale, edge_a, width, height, SOBEL_THRESHOLD_SW);

  for (sw_frames = 0; sw_frames < SW_BASELINE_FRAMES; sw_frames++) {
    volatile uint8_t *current = sw_baseline_use_a_as_current ? edge_a : edge_b;
    volatile uint8_t *previous = sw_baseline_use_a_as_current ? edge_b : edge_a;
    uint32_t start;
    uint32_t capture_cycles;
    uint32_t grayscale_cycles;
    uint32_t clear_cycles;
    uint32_t sobel_cycles;
    uint32_t xor_cycles;

    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
    capture_cycles = cycle_counter_ci() - start;

    start = cycle_counter_ci();
    rgb565_to_grayscale_sw(rgb565, grayscale, width, height);
    grayscale_cycles = cycle_counter_ci() - start;

    start = cycle_counter_ci();
    clear_u8(current, pixels, 0);
    clear_cycles = cycle_counter_ci() - start;

    start = cycle_counter_ci();
    edgeDetection(grayscale, current, width, height, SOBEL_THRESHOLD_SW);
    sobel_cycles = cycle_counter_ci() - start;

    start = cycle_counter_ci();
    last_sw_changed_pixels = software_xor_motion_view(current,
                                                      previous,
                                                      motion_view,
                                                      pixels);
    xor_cycles = cycle_counter_ci() - start;

    total_sw_capture_cycles += capture_cycles;
    total_sw_grayscale_cycles += grayscale_cycles;
    total_sw_clear_cycles += clear_cycles;
    total_sw_sobel_cycles += sobel_cycles;
    total_sw_xor_cycles += xor_cycles;
    total_sw_changed_pixels += last_sw_changed_pixels;

    if (SHOW_SOFTWARE_BASELINE_VIEW) {
      vga[2] = swap_u32(2);
      vga[3] = swap_u32((uint32_t) &motion_view[0]);
    }

    sw_baseline_use_a_as_current ^= 1;
  }

  printf("Software Sobel-XOR frames completed: %d\n", sw_frames);
  printf("This baseline does Sobel first, then XOR motion detection between Sobel frames.\n");
  printf("Software Sobel-XOR last changed pixels: %d\n", last_sw_changed_pixels);
  printf("Software Sobel-XOR average changed pixels: %d\n",
         (sw_frames == 0) ? 0 : total_sw_changed_pixels / sw_frames);

  uint32_t sw_avg_capture = average_cycles(total_sw_capture_cycles, sw_frames);
  uint32_t sw_avg_gray = average_cycles(total_sw_grayscale_cycles, sw_frames);
  uint32_t sw_avg_clear = average_cycles(total_sw_clear_cycles, sw_frames);
  uint32_t sw_avg_sobel = average_cycles(total_sw_sobel_cycles, sw_frames);
  uint32_t sw_avg_xor = average_cycles(total_sw_xor_cycles, sw_frames);
  uint32_t sw_avg_processing = sw_avg_gray + sw_avg_clear + sw_avg_sobel + sw_avg_xor;
  uint32_t sw_avg_total = sw_avg_capture + sw_avg_processing;

  print_cycles("software baseline average capture", sw_avg_capture, cpu_khz);
  print_cycles("software baseline average grayscale", sw_avg_gray, cpu_khz);
  print_cycles("software baseline average clear Sobel buffer", sw_avg_clear, cpu_khz);
  print_cycles("software baseline average Sobel", sw_avg_sobel, cpu_khz);
  print_cycles("software baseline average XOR motion mask", sw_avg_xor, cpu_khz);
  print_cycles("software baseline average processing after capture", sw_avg_processing, cpu_khz);
  print_cycles("software baseline average total with capture", sw_avg_total, cpu_khz);
  print_fps_from_average("software baseline approximate throughput", sw_avg_total, cpu_khz);
}

/* -------------------------------------------------------------------------- */
/* Debug test 1: raw RGB565 camera capture.                                   */
/* Expected result: same live color camera image as the raw camera test.       */
/* This tests camera + VGA without grayscale/Sobel.                           */
/* -------------------------------------------------------------------------- */
static void test_raw_camera_rgb565(volatile unsigned int *vga,
                                   uint32_t cpu_khz) {
  uint32_t total_capture_cycles = 0;
  uint32_t start;

  printf("\nTEST 1: raw RGB565 camera capture for %d frames...\n", CAMERA_DEBUG_FRAMES);

  camera_set_output_mode(0);
  vga[2] = swap_u32(1); /* RGB565 framebuffer */
  vga[3] = swap_u32((uint32_t) &rgb565[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
    total_capture_cycles += cycle_counter_ci() - start;
  }

  printf("Raw RGB565 debug frames: %d\n", CAMERA_DEBUG_FRAMES);
  print_cycles("raw RGB565 average capture",
               total_capture_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_fps_from_average("raw RGB565 approximate throughput",
                         total_capture_cycles / CAMERA_DEBUG_FRAMES,
                         cpu_khz);
}

/* -------------------------------------------------------------------------- */
/* Debug test 2: camera grayscale streaming mode.                              */
/* Expected result: live grayscale camera image.                               */
/* If raw RGB565 works but this fails, the camera output-mode 1 path is broken.*/
/* -------------------------------------------------------------------------- */
static void test_camera_grayscale_mode(volatile unsigned int *vga,
                                       int32_t width,
                                       int32_t height,
                                       uint32_t cpu_khz) {
  uint32_t total_capture_cycles = 0;
  uint32_t start;

  printf("\nTEST 2: camera 8-bit grayscale mode for %d frames...\n", CAMERA_DEBUG_FRAMES);

  camera_set_output_mode(1);
  vga[2] = swap_u32(2); /* 8-bit grayscale framebuffer */
  vga[3] = swap_u32((uint32_t) &grayscale[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &grayscale[0]);
    total_capture_cycles += cycle_counter_ci() - start;
  }

  print_image_stats("camera grayscale", grayscale, width, height);
  printf("Camera grayscale debug frames: %d\n", CAMERA_DEBUG_FRAMES);
  print_cycles("camera grayscale average capture",
               total_capture_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_fps_from_average("camera grayscale approximate throughput",
                         total_capture_cycles / CAMERA_DEBUG_FRAMES,
                         cpu_khz);
}

/* -------------------------------------------------------------------------- */
/* Debug test 4: hardware streaming Sobel + full-frame software XOR.          */
/* Expected result: white pixels where hardware Sobel edges changed.          */
/* This deliberately does not use the motion4 custom instruction.             */
/* -------------------------------------------------------------------------- */
static void test_hardware_sobel_full_frame_xor(volatile unsigned int *vga,
                                               uint32_t pixels,
                                               int32_t width,
                                               int32_t height,
                                               uint32_t cpu_khz) {
  uint32_t start;
  uint32_t last_changed_pixels = 0;
  uint32_t total_changed_pixels = 0;
  uint32_t total_capture_cycles = 0;
  uint32_t total_xor_cycles = 0;
  uint32_t sw_use_a_as_current = 0;

  printf("\nTEST 4: hardware Sobel + full-frame software XOR for %d frames...\n",
         CAMERA_DEBUG_FRAMES);

  clear_u8(edge_a, pixels, 0);
  clear_u8(edge_b, pixels, 0);
  clear_u8(motion_view, pixels, 0);

  camera_set_sobel_threshold(SOBEL_THRESHOLD_HW);
  camera_set_output_mode(2);

  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) &motion_view[0]);

  /* Prime the previous hardware Sobel frame. */
  takeSingleImageBlocking((uint32_t) &edge_a[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    volatile uint8_t *current = sw_use_a_as_current ? edge_a : edge_b;
    volatile uint8_t *previous = sw_use_a_as_current ? edge_b : edge_a;

    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) current);
    total_capture_cycles += cycle_counter_ci() - start;

    start = cycle_counter_ci();
    last_changed_pixels = software_xor_motion_view(current,
                                                   previous,
                                                   motion_view,
                                                   pixels);
    total_xor_cycles += cycle_counter_ci() - start;

    total_changed_pixels += last_changed_pixels;
    vga[3] = swap_u32((uint32_t) &motion_view[0]);

    if (((frames + 1) % MOTION_PRINT_EVERY) == 0) {
      printf("full-frame hardware frame %d: changed pixels=%d\n",
             frames + 1,
             last_changed_pixels);
    }

    sw_use_a_as_current ^= 1;
  }

  print_image_stats("hardware Sobel full-frame XOR", motion_view, width, height);
  printf("Hardware Sobel full-frame last changed pixels: %d\n", last_changed_pixels);
  printf("Hardware Sobel full-frame average changed pixels: %d\n",
         total_changed_pixels / CAMERA_DEBUG_FRAMES);
  print_cycles("hardware Sobel full-frame average capture",
               total_capture_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_cycles("hardware Sobel full-frame average XOR mask",
               total_xor_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_cycles("hardware Sobel full-frame average total",
               (total_capture_cycles + total_xor_cycles) / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_fps_from_average("hardware Sobel full-frame approximate throughput",
                         (total_capture_cycles + total_xor_cycles) / CAMERA_DEBUG_FRAMES,
                         cpu_khz);
}

/* -------------------------------------------------------------------------- */
/* Debug test 5: motion4 on real streaming Sobel camera frames.                */
/* Expected result: live edge view plus UART motion counts from real frames.   */
/* -------------------------------------------------------------------------- */
static void test_camera_motion4_real_frames(volatile unsigned int *vga,
                                            uint32_t pixels,
                                            int32_t width,
                                            int32_t height,
                                            uint32_t cpu_khz) {
  motion_stats_t motion;
  uint32_t last_changed_samples = 0;
  uint32_t last_active_tiles = 0;
  uint32_t total_changed_samples = 0;
  uint32_t total_active_tiles = 0;
  uint32_t total_capture_cycles = 0;
  uint32_t total_compare_cycles = 0;
  uint32_t sw_use_a_as_current = 0;
  uint32_t start;

  printf("\nTEST 5: hardware streaming Sobel + 16x16 tile motion for %d frames...\n",
         CAMERA_DEBUG_FRAMES);

  camera_set_sobel_threshold(SOBEL_THRESHOLD_HW);
  camera_set_output_mode(2);

  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) &edge_a[0]);

  clear_u8(edge_a, pixels, 0);
  clear_u8(edge_b, pixels, 0);
  clear_u8(accumulated_tile_map,
           MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS,
           0);
  takeSingleImageBlocking((uint32_t) &edge_a[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    volatile uint8_t *current = sw_use_a_as_current ? edge_a : edge_b;
    volatile uint8_t *previous = sw_use_a_as_current ? edge_b : edge_a;

    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) current);
    total_capture_cycles += cycle_counter_ci() - start;

    start = cycle_counter_ci();
    motion = motion16_tiles_frame(current, previous, width, height);
    total_compare_cycles += cycle_counter_ci() - start;

    last_changed_samples = motion.changed_samples;
    last_active_tiles = motion.active_tiles;
    total_changed_samples += last_changed_samples;
    total_active_tiles += last_active_tiles;
    if (motion.motion_detected != 0) {
      for (uint32_t i = 0; i < MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS; i++) {
        accumulated_tile_map[i] |= active_tile_map[i];
      }
    }
    vga[3] = swap_u32((uint32_t) current);

    if (((frames + 1) % MOTION_PRINT_EVERY) == 0) {
      printf("debug frame %d: changed samples=%d active tiles=%d motion=%d\n",
             frames + 1,
             motion.changed_samples,
             motion.active_tiles,
             motion.motion_detected);
    }
    sw_use_a_as_current ^= 1;
  }

  printf("Real-frame tile motion last samples: %d\n", last_changed_samples);
  printf("Real-frame tile motion last active tiles: %d\n", last_active_tiles);
  printf("Real-frame tile motion average samples: %d\n",
         total_changed_samples / CAMERA_DEBUG_FRAMES);
  printf("Real-frame tile motion average active tiles: %d\n",
         total_active_tiles / CAMERA_DEBUG_FRAMES);
  print_cycles("hardware Sobel average capture in motion test",
               total_capture_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_cycles("motion4 average 16x16 tile compare",
               total_compare_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_cycles("hardware Sobel + tile motion average total",
               (total_capture_cycles + total_compare_cycles) / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  print_fps_from_average("hardware Sobel + tile motion approximate throughput",
                         (total_capture_cycles + total_compare_cycles) / CAMERA_DEBUG_FRAMES,
                         cpu_khz);
  print_tile_map_summary("Accumulated run", accumulated_tile_map, width, height);
}

static void run_final_demo(volatile unsigned int *vga,
                            int32_t width,
                            int32_t height,
                            uint32_t pixels,
                            uint32_t cpu_khz) {
  printf("\n================ FINAL DEMO START ================\n");
  printf("Demo order: color, grayscale, software motion, hardware Sobel full-frame XOR, hardware Sobel + tiles.\n");

  test_raw_camera_rgb565(vga, cpu_khz);

  test_camera_grayscale_mode(vga, width, height, cpu_khz);

  test_software_sobel_xor_baseline(vga,
                                   pixels,
                                   width,
                                   height,
                                   cpu_khz);

  test_hardware_sobel_full_frame_xor(vga,
                                     pixels,
                                     width,
                                     height,
                                     cpu_khz);

  test_camera_motion4_real_frames(vga,
                                  pixels,
                                  width,
                                  height,
                                  cpu_khz);

  printf("\n================ FINAL DEMO END ==================\n");
}

int main(void) {
  volatile unsigned int *vga = (unsigned int *) 0X50000020;
  camParameters camParams;
  uint32_t result;

  vga_clear();
  printf("Initialising camera (this takes up to 3 seconds)!\n");
  camParams = initOv7670(VGA);
  printf("Done!\n");
  printf("NrOfPixels : %d\n", camParams.nrOfPixelsPerLine);
  result = (camParams.nrOfPixelsPerLine <= 320) ? camParams.nrOfPixelsPerLine | 0x80000000 : camParams.nrOfPixelsPerLine;
  vga[0] = swap_u32(result);
  printf("NrOfLines  : %d\n", camParams.nrOfLinesPerImage);
  result =  (camParams.nrOfLinesPerImage <= 240) ? camParams.nrOfLinesPerImage | 0x80000000 : camParams.nrOfLinesPerImage;
  vga[1] = swap_u32(result);
  printf("PCLK (kHz) : %d\n", camParams.pixelClockInkHz);
  printf("FPS        : %d\n", camParams.framesPerSecond);
  printf("HW Sobel threshold: %d\n", SOBEL_THRESHOLD_HW);
  printf("Motion thresholds: tile=%d active_tiles=%d\n",
         MOTION_TILE_ACTIVE_THRESHOLD,
         MOTION_ACTIVE_TILES_THRESHOLD);

  uint32_t cpu_khz = CPU_KHZ_FALLBACK;
  printf("CPU (kHz)  : %d\n", cpu_khz);

  int32_t width = camParams.nrOfPixelsPerLine;
  int32_t height = camParams.nrOfLinesPerImage;
  uint32_t pixels = width * height;
  printf("Image size : %d x %d (%d pixels)\n", width, height, pixels);

  run_final_demo(vga, width, height, pixels, cpu_khz);

  while (1) {
    /* Keep the final demo image on screen. */
  }
}
