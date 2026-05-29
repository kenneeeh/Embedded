#include <stdio.h>
#include <stdint.h>
#include <ov7670.h>
#include <swap.h>
#include <vga.h>
#include <sobel.h>

#define IMG_W_MAX 640
#define IMG_H_MAX 480
#define MOTION_PRINT_EVERY 15

/* Set this to 0 when you want to skip the debug screens. */
#define RUN_DEBUG_TESTS 1
#define RUN_TEMP_DIFF_SW_TEST 1
#define RUN_SOFTWARE_BASELINE 0
#define SW_BASELINE_FRAMES 5
#define HW_STREAM_FRAMES 60
#define CAMERA_DEBUG_FRAMES 30
#define TEMP_DIFF_FRAMES 30
#define DEBUG_DELAY_LOOPS 20000000u
#define SHOW_SOFTWARE_BASELINE_VIEW 1

/* Sobel threshold presets:
 * Lower = more edges/noise, higher = fewer stronger edges.
 */
#define SOBEL_THRESHOLD_SW 160
#define TEMP_DIFF_SOBEL_THRESHOLD 80
#define TEMP_DIFF_OUTPUT_BINARY 1
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
#define MOTION_SUMMARY_COLUMNS 16
#define MOTION_SUMMARY_ROWS 16

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

typedef struct {
  uint32_t capture_cycles;
  uint32_t diff_cycles;
  uint32_t sobel_cycles;
  uint32_t total_cycles;
} profile_accum_t;

volatile uint16_t rgb565[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t grayscale[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t sobel_sw[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t edge_a[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));
volatile uint8_t edge_b[IMG_W_MAX * IMG_H_MAX] __attribute__((aligned(16)));

/* The temporal-difference software comparison reuses existing full-frame
 * buffers to avoid adding another large pair of 640x480 images.
 *   grayscale_prev aliases grayscale
 *   tempdiff_edge aliases sobel_sw
 */
#define grayscale_prev grayscale
#define tempdiff_edge sobel_sw
static uint8_t diff_line[3][IMG_W_MAX] __attribute__((aligned(16)));

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

static inline uint32_t cpu_info_ci(void) {
  uint32_t result;
  asm volatile ("l.nios_rrr %[out],r0,r0,0x4":[out]"=r"(result));
  return result;
}

static inline uint32_t cycle_counter_ci(void) {
  uint32_t result;
  asm volatile ("l.nios_rrr %[out],r0,r0,0x9":[out]"=r"(result));
  return result;
}

static uint32_t cpu_khz_from_info(uint32_t cpu_info) {
  uint32_t khz = 0;
  for (int shift = 28; shift >= 8; shift -= 4) {
    khz = (khz * 10) + ((cpu_info >> shift) & 0xF);
  }
  return khz;
}

static void print_cycles(const char *label, uint32_t cycles, uint32_t cpu_khz) {
  uint32_t ms = (cpu_khz == 0) ? 0 : cycles / cpu_khz;
  printf("%s: %d cycles, approx %d ms\n", label, cycles, ms);
}

static uint32_t average_cycles(uint32_t total_cycles, uint32_t frames) {
  return (frames == 0) ? 0 : total_cycles / frames;
}

static void print_average_cycles(const char *label,
                                 uint32_t total_cycles,
                                 uint32_t frames,
                                 uint32_t cpu_khz) {
  print_cycles(label, average_cycles(total_cycles, frames), cpu_khz);
}

static void print_fps_from_average(const char *label,
                                   uint32_t avg_cycles,
                                   uint32_t cpu_khz) {
  uint32_t avg_ms = (cpu_khz == 0) ? 0 : avg_cycles / cpu_khz;
  uint32_t fps = (avg_ms == 0) ? 0 : 1000 / avg_ms;
  printf("%s: approx %d FPS (%d ms/frame)\n", label, fps, avg_ms);
}

static void visual_debug_delay(void) {
  for (volatile uint32_t i = 0; i < DEBUG_DELAY_LOOPS; i++) {
    /* visual pause only; do not use timer CI for control flow */
  }
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

static inline uint8_t rgb565_pixel_to_grayscale_sw(uint16_t stored_rgb565) {
  uint16_t rgb = swap_u16(stored_rgb565);
  uint32_t red   = ((rgb >> 11) & 0x1F) << 3;
  uint32_t green = ((rgb >> 5)  & 0x3F) << 2;
  uint32_t blue  = (rgb & 0x1F) << 3;
  return ((red * 54 + green * 183 + blue * 19) >> 8) & 0xFF;
}

static inline uint8_t abs_diff_u8(uint8_t a, uint8_t b) {
  return (a > b) ? (a - b) : (b - a);
}

static inline uint8_t tempdiff_sobel_output(int32_t magnitude) {
  if (magnitude <= TEMP_DIFF_SOBEL_THRESHOLD) {
    return 0;
  }
#if TEMP_DIFF_OUTPUT_BINARY
  return 255;
#else
  return (magnitude > 255) ? 255 : magnitude;
#endif
}

/* Software-only teammate comparison path.
 *
 * This performs temporal difference before Sobel, so static background edges
 * are suppressed and only moving edges remain.  It is useful as an alternative
 * software reference, but it is intentionally not used as the final hardware
 * architecture: moving this into camera.v would require previous-frame memory
 * reads plus updated-frame writes from the camera accelerator.  The final
 * hardware Sobel path stays purely streaming and write-only, then the CPU uses
 * motion4_ci() on the resulting edge frames.
 */
static void run_temporal_difference_sobel_sw(volatile unsigned int *vga,
                                             int32_t width,
                                             int32_t height,
                                             uint32_t pixels,
                                             uint32_t cpu_khz) {
  profile_accum_t profile;
  uint32_t start;
  uint32_t frames;

  profile.capture_cycles = 0;
  profile.diff_cycles = 0;
  profile.sobel_cycles = 0;
  profile.total_cycles = 0;

  printf("\nTemporal-difference software Sobel for %d frames...\n", TEMP_DIFF_FRAMES);
  printf("This is a software comparison path, not the final hardware architecture.\n");

  camera_set_output_mode(0);
  clear_u8(tempdiff_edge, pixels, 0);
  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) &tempdiff_edge[0]);

  takeSingleImageBlocking((uint32_t) &rgb565[0]);
  rgb565_to_grayscale_sw(rgb565, grayscale_prev, width, height);

  for (frames = 0; frames < TEMP_DIFF_FRAMES; frames++) {
    uint32_t frame_capture_cycles;
    uint32_t frame_diff_cycles = 0;
    uint32_t frame_sobel_cycles = 0;

    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
    frame_capture_cycles = cycle_counter_ci() - start;

    for (int32_t x = 0; x < width; x++) {
      tempdiff_edge[x] = 0;
      tempdiff_edge[(height - 1) * width + x] = 0;
    }

    for (int32_t y = 0; y < height; y++) {
      uint32_t diff_start = cycle_counter_ci();
      uint8_t *line = diff_line[y % 3];

      for (int32_t x = 0; x < width; x++) {
        uint32_t index = y * width + x;
        uint8_t current_gray = rgb565_pixel_to_grayscale_sw(rgb565[index]);
        uint8_t previous_gray = grayscale_prev[index];
        line[x] = abs_diff_u8(current_gray, previous_gray);
        grayscale_prev[index] = current_gray;
      }
      frame_diff_cycles += cycle_counter_ci() - diff_start;

      if (y >= 2) {
        uint32_t sobel_start = cycle_counter_ci();
        int32_t center_y = y - 1;
        uint8_t *top = diff_line[(y - 2) % 3];
        uint8_t *mid = diff_line[(y - 1) % 3];
        uint8_t *bot = diff_line[y % 3];
        uint32_t out_base = center_y * width;

        tempdiff_edge[out_base] = 0;
        for (int32_t x = 1; x < width - 1; x++) {
          int32_t p00 = top[x - 1];
          int32_t p01 = top[x];
          int32_t p02 = top[x + 1];
          int32_t p10 = mid[x - 1];
          int32_t p12 = mid[x + 1];
          int32_t p20 = bot[x - 1];
          int32_t p21 = bot[x];
          int32_t p22 = bot[x + 1];
          int32_t gx = -p00 + p02 - (p10 << 1) + (p12 << 1) - p20 + p22;
          int32_t gy = -p00 - (p01 << 1) - p02 + p20 + (p21 << 1) + p22;
          int32_t mag = ((gx < 0) ? -gx : gx) + ((gy < 0) ? -gy : gy);
          tempdiff_edge[out_base + x] = tempdiff_sobel_output(mag);
        }
        tempdiff_edge[out_base + width - 1] = 0;
        frame_sobel_cycles += cycle_counter_ci() - sobel_start;
      }
    }

    profile.capture_cycles += frame_capture_cycles;
    profile.diff_cycles += frame_diff_cycles;
    profile.sobel_cycles += frame_sobel_cycles;
    profile.total_cycles += frame_capture_cycles + frame_diff_cycles + frame_sobel_cycles;
  }

  uint32_t avg_capture = average_cycles(profile.capture_cycles, frames);
  uint32_t avg_diff = average_cycles(profile.diff_cycles, frames);
  uint32_t avg_sobel = average_cycles(profile.sobel_cycles, frames);
  uint32_t avg_processing = avg_diff + avg_sobel;
  uint32_t avg_total = avg_capture + avg_processing;

  print_image_stats("temporal-difference Sobel", tempdiff_edge, width, height);
  print_cycles("tempdiff average capture", avg_capture, cpu_khz);
  print_cycles("tempdiff average grayscale diff/update", avg_diff, cpu_khz);
  print_cycles("tempdiff average Sobel-on-diff", avg_sobel, cpu_khz);
  print_cycles("tempdiff average processing after capture", avg_processing, cpu_khz);
  print_cycles("tempdiff average total with capture", avg_total, cpu_khz);
  print_fps_from_average("tempdiff software approximate throughput", avg_total, cpu_khz);
}

static void print_tile_map_summary(const char *label,
                                   volatile uint8_t *tile_map,
                                   int32_t width,
                                   int32_t height) {
  uint32_t columns = width / MOTION_TILE_SIZE;
  uint32_t rows = height / MOTION_TILE_SIZE;

  printf("%s active tile summary (%dx%d):\n",
         label,
         MOTION_SUMMARY_COLUMNS,
         MOTION_SUMMARY_ROWS);

  for (uint32_t sy = 0; sy < MOTION_SUMMARY_ROWS; sy++) {
    uint32_t tile_y0 = (sy * rows) / MOTION_SUMMARY_ROWS;
    uint32_t tile_y1 = ((sy + 1) * rows) / MOTION_SUMMARY_ROWS;

    if (tile_y1 <= tile_y0) {
      tile_y1 = tile_y0 + 1;
    }

    for (uint32_t sx = 0; sx < MOTION_SUMMARY_COLUMNS; sx++) {
      uint32_t tile_x0 = (sx * columns) / MOTION_SUMMARY_COLUMNS;
      uint32_t tile_x1 = ((sx + 1) * columns) / MOTION_SUMMARY_COLUMNS;
      uint32_t active = 0;

      if (tile_x1 <= tile_x0) {
        tile_x1 = tile_x0 + 1;
      }

      for (uint32_t ty = tile_y0; ty < tile_y1 && active == 0; ty++) {
        for (uint32_t tx = tile_x0; tx < tile_x1; tx++) {
          if (tile_map[ty * columns + tx] != 0) {
            active = 1;
            break;
          }
        }
      }

      putchar(active ? 'X' : '.');
      if (sx + 1 < MOTION_SUMMARY_COLUMNS) {
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

/* -------------------------------------------------------------------------- */
/* Debug test 1: raw RGB565 camera capture.                                   */
/* Expected result: same live color camera image as the raw camera test.       */
/* This tests camera + VGA without grayscale/Sobel.                           */
/* -------------------------------------------------------------------------- */
static void test_raw_camera_rgb565(volatile unsigned int *vga) {
  printf("\nTEST 1: raw RGB565 camera capture for %d frames...\n", CAMERA_DEBUG_FRAMES);

  camera_set_output_mode(0);
  vga[2] = swap_u32(1); /* RGB565 framebuffer */
  vga[3] = swap_u32((uint32_t) &rgb565[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
  }

  printf("Raw RGB565 debug frames: %d\n", CAMERA_DEBUG_FRAMES);
}

/* -------------------------------------------------------------------------- */
/* Debug test 2: camera grayscale streaming mode.                              */
/* Expected result: live grayscale camera image.                               */
/* If raw RGB565 works but this fails, the camera output-mode 1 path is broken.*/
/* -------------------------------------------------------------------------- */
static void test_camera_grayscale_mode(volatile unsigned int *vga,
                                       int32_t width,
                                       int32_t height) {
  printf("\nTEST 2: camera 8-bit grayscale mode for %d frames...\n", CAMERA_DEBUG_FRAMES);

  camera_set_output_mode(1);
  vga[2] = swap_u32(2); /* 8-bit grayscale framebuffer */
  vga[3] = swap_u32((uint32_t) &grayscale[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    takeSingleImageBlocking((uint32_t) &grayscale[0]);
  }

  print_image_stats("camera grayscale", grayscale, width, height);
  printf("Camera grayscale debug frames: %d\n", CAMERA_DEBUG_FRAMES);
}

/* -------------------------------------------------------------------------- */
/* Debug test 4: camera streaming Sobel mode without motion comparison.        */
/* Expected result: live binary Sobel edge image.                              */
/* If this is black, try lowering SOBEL_THRESHOLD_HW or fix camera mode 2.     */
/* -------------------------------------------------------------------------- */
static void test_camera_sobel_mode_only(volatile unsigned int *vga,
                                        int32_t width,
                                        int32_t height,
                                        uint32_t cpu_khz) {
  uint32_t start;
  uint32_t total_capture_cycles = 0;

  printf("\nTEST 4: camera streaming Sobel mode only for %d frames...\n", CAMERA_DEBUG_FRAMES);

  clear_u8(edge_a, IMG_W_MAX * IMG_H_MAX, 0);

  camera_set_sobel_threshold(SOBEL_THRESHOLD_HW);
  camera_set_output_mode(2);

  vga[2] = swap_u32(2); /* 8-bit Sobel edge framebuffer */
  vga[3] = swap_u32((uint32_t) &edge_a[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &edge_a[0]);
    total_capture_cycles += cycle_counter_ci() - start;
  }

  print_image_stats("hw streaming sobel", edge_a, width, height);
  print_cycles("hardware Sobel average capture",
               total_capture_cycles / CAMERA_DEBUG_FRAMES,
               cpu_khz);
  printf("Camera streaming Sobel-only debug frames: %d\n", CAMERA_DEBUG_FRAMES);
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
  uint32_t use_a_as_current = 0;
  uint32_t start;

  printf("\nTEST 5: motion4 on real streaming Sobel frames for %d frames...\n",
         CAMERA_DEBUG_FRAMES);

  camera_set_sobel_threshold(SOBEL_THRESHOLD_HW);
  camera_set_output_mode(2);

  vga[2] = swap_u32(2);
  vga[3] = swap_u32((uint32_t) &edge_a[0]);

  clear_u8(edge_a, pixels, 0);
  clear_u8(edge_b, pixels, 0);
  takeSingleImageBlocking((uint32_t) &edge_a[0]);

  for (uint32_t frames = 0; frames < CAMERA_DEBUG_FRAMES; frames++) {
    volatile uint8_t *current = use_a_as_current ? edge_a : edge_b;
    volatile uint8_t *previous = use_a_as_current ? edge_b : edge_a;

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
    vga[3] = swap_u32((uint32_t) current);

    if (((frames + 1) % MOTION_PRINT_EVERY) == 0) {
      printf("debug frame %d: changed samples=%d active tiles=%d motion=%d\n",
             frames + 1,
             motion.changed_samples,
             motion.active_tiles,
             motion.motion_detected);
    }
    use_a_as_current ^= 1;
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
}

static void run_debug_tests(volatile unsigned int *vga,
                            int32_t width,
                            int32_t height,
                            uint32_t pixels,
                            uint32_t cpu_khz) {
  printf("\n================ DEBUG TESTS START ================\n");
  printf("All debug tests use real camera data and fixed frame counts.\n");

  test_raw_camera_rgb565(vga);

  test_camera_grayscale_mode(vga, width, height);

#if RUN_TEMP_DIFF_SW_TEST
  printf("\nTEST 3: temporal-difference software Sobel comparison.\n");
  run_temporal_difference_sobel_sw(vga,
                                   width,
                                   height,
                                   pixels,
                                   cpu_khz);
#else
  printf("\nTEST 3: temporal-difference software Sobel skipped. Set RUN_TEMP_DIFF_SW_TEST=1 to enable.\n");
#endif

  test_camera_sobel_mode_only(vga, width, height, cpu_khz);

  test_camera_motion4_real_frames(vga,
                                  pixels,
                                  width,
                                  height,
                                  cpu_khz);

  printf("\n================ DEBUG TESTS END ==================\n");
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

  uint32_t cpu_info = cpu_info_ci();
  uint32_t cpu_khz = cpu_khz_from_info(cpu_info);
  printf("CPU info   : 0x%08x\n", cpu_info);
  printf("CPU (kHz)  : %d\n", cpu_khz);

  int32_t width = camParams.nrOfPixelsPerLine;
  int32_t height = camParams.nrOfLinesPerImage;
  uint32_t pixels = width * height;
  printf("Image size : %d x %d (%d pixels)\n", width, height, pixels);

#if RUN_DEBUG_TESTS
  run_debug_tests(vga, width, height, pixels, cpu_khz);
#endif

  /* ---------------------------------------------------------------------- */
  /* Software-only baseline: RGB565 capture + C grayscale + C Sobel.        */
  /* ---------------------------------------------------------------------- */
#if RUN_SOFTWARE_BASELINE
  printf("\nSoftware-only Sobel baseline for %d frames...\n", SW_BASELINE_FRAMES);
  camera_set_output_mode(0);
  show_stable_blank_view(vga, edge_b, pixels);

  uint32_t sw_frames = 0;
  profile_accum_t sw_profile;
  sw_profile.capture_cycles = 0;
  sw_profile.diff_cycles = 0;
  sw_profile.sobel_cycles = 0;
  sw_profile.total_cycles = 0;

  for (sw_frames = 0; sw_frames < SW_BASELINE_FRAMES; sw_frames++) {
    uint32_t start;
    uint32_t capture_cycles;
    uint32_t grayscale_cycles;
    uint32_t sobel_cycles;

    start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) &rgb565[0]);
    capture_cycles = cycle_counter_ci() - start;

    start = cycle_counter_ci();
    rgb565_to_grayscale_sw(rgb565, grayscale, width, height);
    grayscale_cycles = cycle_counter_ci() - start;

    clear_u8(sobel_sw, pixels, 0);

    start = cycle_counter_ci();
    edgeDetection(grayscale, sobel_sw, width, height, SOBEL_THRESHOLD_SW);
    sobel_cycles = cycle_counter_ci() - start;

    sw_profile.capture_cycles += capture_cycles;
    sw_profile.diff_cycles += grayscale_cycles;
    sw_profile.sobel_cycles += sobel_cycles;
    sw_profile.total_cycles += capture_cycles + grayscale_cycles + sobel_cycles;

    if (SHOW_SOFTWARE_BASELINE_VIEW) {
      vga[2] = swap_u32(2);
      vga[3] = swap_u32((uint32_t) &sobel_sw[0]);
    }
  }

  printf("Software Sobel frames completed: %d\n", sw_frames);
  printf("This is the non-real-time reference path required by the project.\n");
  uint32_t sw_avg_capture = average_cycles(sw_profile.capture_cycles, sw_frames);
  uint32_t sw_avg_gray = average_cycles(sw_profile.diff_cycles, sw_frames);
  uint32_t sw_avg_sobel = average_cycles(sw_profile.sobel_cycles, sw_frames);
  uint32_t sw_avg_processing = sw_avg_gray + sw_avg_sobel;
  uint32_t sw_avg_total = sw_avg_capture + sw_avg_processing;
  print_cycles("software baseline average capture", sw_avg_capture, cpu_khz);
  print_cycles("software baseline average grayscale", sw_avg_gray, cpu_khz);
  print_cycles("software baseline average Sobel", sw_avg_sobel, cpu_khz);
  print_cycles("software baseline average processing after capture", sw_avg_processing, cpu_khz);
  print_cycles("software baseline average total with capture", sw_avg_total, cpu_khz);
  print_fps_from_average("software baseline approximate throughput", sw_avg_total, cpu_khz);
#else
  printf("\nSoftware-only Sobel baseline skipped. Set RUN_SOFTWARE_BASELINE=1 to enable.\n");
#endif

  /* ---------------------------------------------------------------------- */
  /* Accelerated path: streaming camera Sobel + CPU motion custom instr.    */
  /* ---------------------------------------------------------------------- */
  printf("\nStreaming Sobel + motion4 custom instruction for %d frames...\n", HW_STREAM_FRAMES);
  camera_set_sobel_threshold(SOBEL_THRESHOLD_HW);
  camera_set_output_mode(2);

  vga[2] = swap_u32(2); /* 8-bit Sobel edge framebuffer */
  vga[3] = swap_u32((uint32_t) &edge_a[0]);

  clear_u8(edge_a, pixels, 0);
  clear_u8(edge_b, pixels, 0);
  clear_u8(accumulated_tile_map,
           MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS,
           0);

  /* Prime the previous frame. */
  takeSingleImageBlocking((uint32_t) &edge_a[0]);

  uint32_t hw_frames = 0;
  motion_stats_t motion;
  uint32_t last_changed_samples = 0;
  uint32_t last_active_tiles = 0;
  uint32_t last_motion_detected = 0;
  uint32_t total_changed_samples = 0;
  uint32_t total_active_tiles = 0;
  uint32_t total_hw_capture_cycles = 0;
  uint32_t total_hw_compare_cycles = 0;
  uint32_t use_a_as_current = 0;

  for (hw_frames = 0; hw_frames < HW_STREAM_FRAMES; hw_frames++) {
    volatile uint8_t *current = use_a_as_current ? edge_a : edge_b;
    volatile uint8_t *previous = use_a_as_current ? edge_b : edge_a;

    uint32_t start = cycle_counter_ci();
    takeSingleImageBlocking((uint32_t) current);
    total_hw_capture_cycles += cycle_counter_ci() - start;

    start = cycle_counter_ci();
    motion = motion16_tiles_frame(current, previous, width, height);
    total_hw_compare_cycles += cycle_counter_ci() - start;

    last_changed_samples = motion.changed_samples;
    last_active_tiles = motion.active_tiles;
    last_motion_detected = motion.motion_detected;
    total_changed_samples += last_changed_samples;
    total_active_tiles += last_active_tiles;
    for (uint32_t i = 0; i < MOTION_MAX_TILE_COLUMNS * MOTION_MAX_TILE_ROWS; i++) {
      accumulated_tile_map[i] |= active_tile_map[i];
    }
    vga[3] = swap_u32((uint32_t) current);

    if (((hw_frames + 1) % MOTION_PRINT_EVERY) == 0) {
      printf("frame %d: changed samples=%d active tiles=%d motion=%d\n",
             hw_frames + 1,
             motion.changed_samples,
             motion.active_tiles,
             motion.motion_detected);
    }
    use_a_as_current ^= 1;
  }

  printf("Streaming Sobel frames completed: %d\n", hw_frames);
  printf("Last tile motion: samples=%d active tiles=%d motion=%d\n",
         last_changed_samples,
         last_active_tiles,
         last_motion_detected);
  printf("Average tile motion: samples=%d active tiles=%d/frame\n",
         (hw_frames == 0) ? 0 : total_changed_samples / hw_frames,
         (hw_frames == 0) ? 0 : total_active_tiles / hw_frames);
  uint32_t hw_avg_capture = average_cycles(total_hw_capture_cycles, hw_frames);
  uint32_t hw_avg_compare = average_cycles(total_hw_compare_cycles, hw_frames);
  uint32_t hw_avg_total = hw_avg_capture + hw_avg_compare;
  print_cycles("final hardware average capture", hw_avg_capture, cpu_khz);
  print_cycles("final motion4 tile average compare", hw_avg_compare, cpu_khz);
  print_cycles("final hardware+motion average total", hw_avg_total, cpu_khz);
  print_fps_from_average("final hardware+motion approximate throughput", hw_avg_total, cpu_khz);
  print_tile_map_summary("Accumulated run", accumulated_tile_map, width, height);

  while (1) {
    /* Keep the final Sobel/motion image on screen. */
  }
}
