#ifndef PROF_H_
#define PROF_H_

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <unistd.h>
#endif

#ifndef PROF_H_INCLUDE_
#define PROF_H_INCLUDE_

#define SIZE 10000000
#define MAX_PROFILE_ENTRIES 1024

#define DEFER(begin, end) \
    for(int _defer_ = ((begin), 0); !_defer_; _defer_ = 1, (end))

#define CONCAT(a, b) a##b
#define CONCAT_AUX(a,b) CONCAT(a,b)

typedef struct {
    const char* label;
    double elapsed_ms;
    uint64_t hit_count; // how many times the same block was profiled
} prof_entry;

typedef struct {
    prof_entry entries[MAX_PROFILE_ENTRIES];
    int count;
} prof_storage;

typedef struct {
    uint64_t start_time;
    int entry_index;
} prof_zone;

// Global storage for profile results
extern prof_storage g_prof_storage;

/*
    Get the number of OS ticks-per-second
*/
static uint64_t prof_get_timer_freq(void);

/*
    Get the time elapsed since the last reset in nanoseconds
*/
static uint64_t prof_get_time(void);

void prof_block_start(prof_zone* zone, const char* name, int counter_id);
void prof_block_end(prof_zone* zone);

// Functions to manage the global profile storage
void prof_init(void);
void prof_reset(void);
void prof_print_results(void);
void prof_sort_results(void);

#define PROFILE(name) \
    prof_zone CONCAT_AUX(_prof_, __LINE__); \
    DEFER(prof_block_start(&CONCAT_AUX(_prof_, __LINE__), name, __COUNTER__), prof_block_end(&CONCAT_AUX(_prof_, __LINE__)))

#endif

#ifdef PROF_IMPLEMENTATION

// Global storage definition
prof_storage g_prof_storage = {0};

static uint64_t prof_get_timer_freq(void) 
{
    #ifdef _WIN32
        LARGE_INTEGER Freq;
        QueryPerformanceFrequency(&Freq);
        return Freq.QuadPart;
    #else
        return 1000000000ULL; // nanosecond resolution (clock_gettime)
    #endif
}

static uint64_t prof_get_time(void) 
{
    #ifdef _WIN32
        LARGE_INTEGER Value;
        QueryPerformanceCounter(&Value);
        // We know the time of a single tick in seconds we just convert it to nanoseconds
        // and multiply by the number of elapsed ticks
        uint64_t elapsed_ns = Value.QuadPart * 1000000000ULL;
        elapsed_ns /= prof_get_timer_freq();
        return elapsed_ns;
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        
        uint64_t result = prof_get_timer_freq() * (uint64_t)ts.tv_sec + (uint64_t)ts.tv_nsec;
        return result;
    #endif
}

void prof_block_start(prof_zone* zone, const char* name, int counter_id) 
{
    zone->start_time = prof_get_time();
    
    // Use counter_id as direct index (each PROFILE call gets unique counter)
    if (counter_id < MAX_PROFILE_ENTRIES) 
    {
        zone->entry_index = counter_id;
        
        // Initialize entry if first time
        if (g_prof_storage.entries[counter_id].hit_count == 0) 
        {
            g_prof_storage.entries[counter_id].label = name;
            g_prof_storage.entries[counter_id].elapsed_ms = 0.0;
            g_prof_storage.entries[counter_id].hit_count = 0;
            
            // Update count to track highest used index
            if (counter_id >= g_prof_storage.count) {
                g_prof_storage.count = counter_id + 1;
            }
        }
    } 
    else 
    {
        zone->entry_index = -1; // Invalid index
    }
}

void prof_block_end(prof_zone* zone) 
{
    uint64_t end_time = prof_get_time();
    double elapsed_ns = (double)(end_time - zone->start_time);
    double elapsed_ms = elapsed_ns / 1000000.0;
    
    // Update the corresponding entry
    if (zone->entry_index >= 0 && zone->entry_index < MAX_PROFILE_ENTRIES) {
        g_prof_storage.entries[zone->entry_index].elapsed_ms += elapsed_ms;
        g_prof_storage.entries[zone->entry_index].hit_count++;
    }
}

void prof_init(void) 
{
    memset(&g_prof_storage, 0, sizeof(g_prof_storage));
}

void prof_reset(void) 
{
    memset(&g_prof_storage, 0, sizeof(g_prof_storage));
    g_prof_storage.count = 0;
}

// Comparison function for sorting (descending order by time)
static int prof_compare(const void* a, const void* b) 
{
    const prof_entry* entry_a = (const prof_entry*)a;
    const prof_entry* entry_b = (const prof_entry*)b;
    
    // Skip empty entries
    if (entry_a->hit_count == 0 && entry_b->hit_count == 0) return 0;
    if (entry_a->hit_count == 0) return 1;
    if (entry_b->hit_count == 0) return -1;
    
    if (entry_a->elapsed_ms > entry_b->elapsed_ms) return -1;
    if (entry_a->elapsed_ms < entry_b->elapsed_ms) return 1;
    return 0;
}

void prof_sort_results(void) 
{
    qsort(g_prof_storage.entries, g_prof_storage.count, sizeof(prof_entry), prof_compare);
}

void prof_print_results(void) 
{
    printf("\n=== Profile Results ===\n");
    for (int i = 0; i < g_prof_storage.count; i++) {
        if (g_prof_storage.entries[i].hit_count > 0) {
            printf("[PROFILE] %s[%llu]: %.6f ms (total)\n", 
                   g_prof_storage.entries[i].label,
                   (unsigned long long)g_prof_storage.entries[i].hit_count,
                   g_prof_storage.entries[i].elapsed_ms);
        }
    }
    printf("=======================\n");
}
#endif

#endif