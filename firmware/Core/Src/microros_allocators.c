/**
 * @file microros_allocators.c
 * @brief FreeRTOS heap allocators for micro-ROS.
 *
 * Replaces default malloc/free with pvPortMalloc/vPortFree so
 * micro-ROS uses the FreeRTOS heap (heap_4, 50KB configured).
 */

#include "FreeRTOS.h"
#include <stddef.h>
#include <string.h>

void *microros_allocate(size_t size, void *state)
{
    (void)state;
    return pvPortMalloc(size);
}

void microros_deallocate(void *pointer, void *state)
{
    (void)state;
    vPortFree(pointer);
}

void *microros_reallocate(void *pointer, size_t size, void *state)
{
    (void)state;
    void *new_ptr = pvPortMalloc(size);
    if (new_ptr != NULL && pointer != NULL) {
        /* Cannot know old allocation size — copy 'size' bytes.
         * This is safe for micro-ROS usage where realloc grows. */
        memcpy(new_ptr, pointer, size);
        vPortFree(pointer);
    }
    return new_ptr;
}

void *microros_zero_allocate(size_t num, size_t size, void *state)
{
    (void)state;
    size_t total = num * size;
    void *ptr = pvPortMalloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}
