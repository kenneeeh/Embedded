#include <cache.h>
#include <stdio.h>

const char* cache_size[] = {
    "1k", "2k", "4k", "8k"
};

const char* cache_assoc[] = {
    "dm", "2-way", "4-way"
};

const char* cache_policy[] = {
    "fifo", "plru", "lru"
};

void cache_printinfo(uint32_t value) {
    printf("Cache info for value = 0x%08x: ", value);
    unsigned int res;

    res = (value >> 19) & 1;
    if (res == 0) {
        printf("%s\n", "disabled");
        return;
    }

    res = (value >> 30) & 3;
    printf("size = %s, ", cache_size[res]);

    res = (value) & 3;
    printf("assoc = %s, ", cache_assoc[res]);

    res = (value >> 16) & 3;
    printf("policy = %s\n", cache_policy[res]);
}
