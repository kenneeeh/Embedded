#include <assert.h>
#include <perf.h>

#define SPR_CPU_INFO 9

static uint32_t cpu_freq = 0;

void perf_init() {
    cpu_freq = perf_cpu_freq();
}

uint32_t perf_cpu_freq() {
    uint32_t cpu_freq, cpu_info;

    do {
        cpu_info = SPR_READ(SPR_CPU_INFO);
    } while ((cpu_info & 0xFFFFFF) == 0);

    // This register gives the CPU-frequency in kHz; hence we have a resolution error of +/- 1 ms
    cpu_freq = (cpu_info >> 28) & 0xF;
    cpu_freq = cpu_freq * 10 + ((cpu_info >> 24) & 0xF);
    cpu_freq = cpu_freq * 10 + ((cpu_info >> 20) & 0xF);
    cpu_freq = cpu_freq * 10 + ((cpu_info >> 16) & 0xF);
    cpu_freq = cpu_freq * 10 + ((cpu_info >> 12) & 0xF);
    cpu_freq = cpu_freq * 10 + ((cpu_info >> 8) & 0xF);

    return cpu_freq;
}

perf_time_t perf_cycles_to_time(perf_cycles_t cycles) {
    die_if_not(cpu_freq != 0, "cpu frequency is not initialized, call perf_init() first!");

    perf_time_t result = { .h = 0, .m = 0, .s = 0, .ms = 0 };

    uint64_t runtime = 0, ms = 0, s = 0, m = 0, h = 0;

    runtime = cycles / cpu_freq;

    ms = runtime % 1000;
    s = runtime / 1000;

    if (s > 60) {
        m = s / 60;
        s %= 60;
    }
    if (m > 60) {
        h = m / 60;
        m %= 60;
    }

    return (perf_time_t) { .h = h, .m = m, .s = s, .ms = ms };
}

#include <stdio.h>

void perf_print_cycles(unsigned counter_id, const char *desc) {
    desc = desc ? desc : "no desc";
    printf("%-32s [%02d] : %lld cycles\n", desc, counter_id, perf_read_counter(counter_id));
}

void perf_print_time(unsigned counter_id, const char* desc) {
    desc = desc ? desc : "no desc";
    perf_time_t t = perf_cycles_to_time(perf_read_counter(counter_id));
    printf("%-32s [%02d] : %02u:%02u:%02u.%03u\n", desc, counter_id, t.h, t.m, t.s, t.ms);
}
