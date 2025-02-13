#include "dl.h"
#include <math.h>

#define MIN_BLOCK_SIZE 32
#define MAX_BLOCK_INDEX 24

typedef struct Block
{
    size_t size;
    struct Block *next;

} Block;

typedef struct Allocator
{
    void *memory;
    size_t total_size;
    Block *free_lists[MAX_BLOCK_INDEX];
} Allocator;

size_t round_to_power_of_two(size_t size)
{
    size_t power = MIN_BLOCK_SIZE;
    while (power < size)
    {
        power <<= 1;
    }
    return power;
}

int get_power_of_two(size_t size)
{
    return (int)(log2(size));
}

EXPORT Allocator *allocator_create(void *memory, const size_t size)
{
    if (!memory || size < MIN_BLOCK_SIZE + sizeof(Allocator) + sizeof(Block))
    {
        return NULL;
    }

    Allocator *allocator = (Allocator*)memory; //(Allocator *)mmap(
        //NULL, sizeof(Allocator), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    size_t total_size = round_to_power_of_two(size - sizeof(Allocator));
    size_t max_size = 1 << (MAX_BLOCK_INDEX - 1);

    if (total_size > max_size)
    {
        return NULL;
    }

    allocator->memory = memory - sizeof(Allocator);
    allocator->total_size = total_size;

    for (size_t i = 0; i < MAX_BLOCK_INDEX; i++)
    {
        allocator->free_lists[i] = NULL;
    }

    Block *initial_block = (Block *)allocator->memory;
    initial_block->size = total_size;
    initial_block->next = NULL;

    int index = get_power_of_two(total_size) - get_power_of_two(MIN_BLOCK_SIZE);
    allocator->free_lists[index] = initial_block;

    return allocator;
}

EXPORT void allocator_destroy(Allocator *allocator)
{
    if (!allocator)
        return;
    memset(allocator, 0, allocator->total_size + sizeof(Allocator));
}

EXPORT void *allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (!allocator || size == 0)
        return NULL;

    size_t block_size = round_to_power_of_two(size + sizeof(Block));
    size_t max_size = 1 << (MAX_BLOCK_INDEX - 1);

    if (block_size > max_size)
    {
        return NULL;
    }

    int index = get_power_of_two(block_size) - get_power_of_two(MIN_BLOCK_SIZE);
    if (index >= MAX_BLOCK_INDEX)
    {
        return NULL;
    }
    else if (index < 0)
    {
        index = 1;
    }

    while (index < MAX_BLOCK_INDEX && !allocator->free_lists[index])
    {
        index++;
    }

    if (index >= MAX_BLOCK_INDEX)
    {
        return NULL;
    }

    Block *block = allocator->free_lists[index];
    allocator->free_lists[index] = block->next;

    while (block->size > block_size)
    {
        size_t new_size = block->size >> 1;
        Block *buddy = (Block *)((char *)block + new_size);
        buddy->size = new_size;
        buddy->next =
            allocator->free_lists[get_power_of_two(new_size) - get_power_of_two(MIN_BLOCK_SIZE)];
        allocator->free_lists[get_power_of_two(new_size) - get_power_of_two(MIN_BLOCK_SIZE)] =
            buddy;
        block->size = new_size;
    }

    return (void *)((char *)block + sizeof(Block));
}

EXPORT void allocator_free(Allocator *const allocator, void *const memory)
{
    if (!allocator || !memory)
        return;

    Block *block = (Block *)((char *)memory - sizeof(Block));

    size_t block_size = block->size;
    int index = get_power_of_two(block_size) - get_power_of_two(MIN_BLOCK_SIZE);
    if (index >= MAX_BLOCK_INDEX)
        return;

    block->next = allocator->free_lists[index];
    allocator->free_lists[index] = block;
}