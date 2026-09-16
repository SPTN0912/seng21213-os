#include "pmm.h"

/*
 * Stage 3 — Physical Memory Manager
 *
 * The bootloader stores the BIOS E820 memory map at:
 *
 *   0x8000 -> number of E820 entries
 *   0x8004 -> first E820 entry
 *
 * Each E820 entry is 24 bytes:
 *   +0  : base address   (uint64_t)
 *   +8  : length         (uint64_t)
 *   +16 : type           (uint32_t)
 *   +20 : ACPI attributes(uint32_t)
 */

#define E820_COUNT_ADDR  0x8000
#define E820_ENTRIES_ADDR 0x8004

#define MAX_MEMORY_MB 32
#define MAX_FRAMES ((MAX_MEMORY_MB * 1024 * 1024) / PAGE_SIZE)
#define BITMAP_SIZE (MAX_FRAMES / 8)

#define E820_USABLE 1

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} e820_entry_t;

static uint8_t frame_bitmap[BITMAP_SIZE];

static uint32_t total_memory = 0;
static uint32_t used_memory = 0;
static uint32_t total_frames = 0;

/* Set a frame as used/reserved. */
static void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
}

/* Set a frame as free/usable. */
static void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
}

/* Check whether a frame is currently used/reserved. */
static bool bitmap_test(uint32_t frame)
{
    return (frame_bitmap[frame / 8] &
            (uint8_t)(1 << (frame % 8))) != 0;
}

void pmm_init(void)
{
    volatile uint32_t *entry_count =
        (volatile uint32_t *)E820_COUNT_ADDR;

    volatile e820_entry_t *entries =
        (volatile e820_entry_t *)E820_ENTRIES_ADDR;

    uint32_t count = *entry_count;

    /*
     * Start with every frame reserved.
     * Only E820 type-1 frames will be made available.
     */
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        frame_bitmap[i] = 0xFF;
    }

    total_frames = MAX_FRAMES;
    total_memory = 0;
    used_memory = 0;

    /*
     * Parse the E820 memory map.
     *
     * Type 1 means usable RAM.
     */
    for (uint32_t i = 0; i < count; i++) {

        uint64_t base64 = entries[i].base;
        uint64_t length64 = entries[i].length;

        if (entries[i].type != E820_USABLE) {
            continue;
        }

        /*
         * Our bitmap supports the first 32 MB.
         * Ignore usable memory completely above that limit.
         */
        if (base64 >= (uint64_t)MAX_MEMORY_MB * 1024 * 1024) {
            continue;
        }

        uint32_t base = (uint32_t)base64;
        uint32_t length = (uint32_t)length64;

        uint32_t end = base + length;

        if (end > MAX_MEMORY_MB * 1024 * 1024) {
            end = MAX_MEMORY_MB * 1024 * 1024;
        }

        /*
         * Convert addresses to frame numbers.
         * Round the beginning up and the ending down so that
         * only complete 4 KB frames are released.
         */
        uint32_t first_frame =
            (base + PAGE_SIZE - 1) / PAGE_SIZE;

        uint32_t last_frame =
            end / PAGE_SIZE;

        if (last_frame > total_frames) {
            last_frame = total_frames;
        }

        for (uint32_t frame = first_frame;
             frame < last_frame;
             frame++) {

            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                total_memory += PAGE_SIZE;
            }
        }
    }

    /*
     * Reserve physical frame 0.
     * Address 0 is not returned by pmm_alloc_frame().
     */
    if (!bitmap_test(0)) {
        bitmap_set(0);

        if (total_memory >= PAGE_SIZE) {
            total_memory -= PAGE_SIZE;
        }
    }

    /*
     * At this point all usable E820 frames are free except
     * frame 0. Therefore used memory starts at zero.
     */
    used_memory = 0;
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t frame = 0; frame < total_frames; frame++) {

        if (!bitmap_test(frame)) {

            bitmap_set(frame);
            used_memory += PAGE_SIZE;

            return frame * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t paddr)
{
    uint32_t frame = paddr / PAGE_SIZE;

    if (frame >= total_frames) {
        return;
    }

    /*
     * Never free physical frame 0.
     */
    if (frame == 0) {
        return;
    }

    if (bitmap_test(frame)) {
        bitmap_clear(frame);

        if (used_memory >= PAGE_SIZE) {
            used_memory -= PAGE_SIZE;
        }
    }
}

uint32_t pmm_get_total_memory(void)
{
    return total_memory;
}

uint32_t pmm_get_used_memory(void)
{
    return used_memory;
}

uint32_t pmm_get_free_memory(void)
{
    return total_memory - used_memory;
}
