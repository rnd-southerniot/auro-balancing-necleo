/**
 * @file microros_allocators.c
 * @brief Override malloc/free/calloc/realloc with FreeRTOS heap.
 *
 * micro-ROS uses the default rcl allocator which calls malloc/free.
 * On bare-metal STM32 with newlib-nano, these map to _sbrk which
 * conflicts with FreeRTOS heap_4. Override them globally so all
 * malloc calls go through pvPortMalloc.
 */

#include "FreeRTOS.h"
#include <stddef.h>
#include <string.h>

void *malloc(size_t size)
{
    return pvPortMalloc(size);
}

void free(void *ptr)
{
    vPortFree(ptr);
}

void *calloc(size_t num, size_t size)
{
    size_t total = num * size;
    void *ptr = pvPortMalloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (size == 0) {
        vPortFree(ptr);
        return NULL;
    }
    void *new_ptr = pvPortMalloc(size);
    if (new_ptr != NULL && ptr != NULL) {
        /* heap_4 doesn't track allocation size — copy 'size' bytes.
         * Safe for growing allocations (micro-ROS pattern). */
        memcpy(new_ptr, ptr, size);
        vPortFree(ptr);
    }
    return new_ptr;
}
