#include "dl.h"

#define MIN_BLOCK_SIZE 32

typedef struct Block
{
    size_t size;
    struct Block *next;
    bool is_free;
} Block;

typedef struct Allocator
{
    Block *free_list;
    void *memory_start;
    size_t total_size;
} Allocator;

EXPORT Allocator *allocator_create(void *memory, size_t size)
{
    if (!memory || size < sizeof(Allocator) + sizeof(Block) + MIN_BLOCK_SIZE)
    {
        return NULL;
    }

    Allocator *allocator = (Allocator *)memory;
    allocator->memory_start = (char *)memory + sizeof(Allocator);
    allocator->total_size = size - sizeof(Allocator);
    allocator->free_list = (Block *)allocator->memory_start;

    allocator->free_list->size = allocator->total_size - sizeof(Block);
    allocator->free_list->next = NULL;
    allocator->free_list->is_free = true;

    return allocator;
}

EXPORT void allocator_destroy(Allocator *allocator)
{
    if (allocator)
    {
        memset(allocator, 0, allocator->total_size + sizeof(Allocator));
    }
}

EXPORT void *allocator_alloc(Allocator *allocator, size_t size)
{
    if (!allocator || size < 1)
    {
        return NULL;
    }

    size = (size / MIN_BLOCK_SIZE
        + (size % MIN_BLOCK_SIZE) ? 1 : 0)
        * MIN_BLOCK_SIZE;

    Block *current = allocator->free_list;

    while (current)
    {
        if (current->is_free && current->size >= size) {
            size_t remain_size = current->size - size;
            if (remain_size >= sizeof(Block) + MIN_BLOCK_SIZE) {
                Block *new_block = (Block *)((char *)current + 2 * sizeof(Block) + size);
                new_block->size = remain_size - sizeof(Block);
                new_block->is_free = true;
                new_block->next = current->next;

                current->next = new_block;
                current->size = size;
            }
            current->is_free = false;
            return (void *)((char *)current + sizeof(Block));
        }
        else {
            current = current->next;
        }
    }
    
    return NULL;
}

EXPORT void allocator_free(Allocator *allocator, void *ptr_to_memory)
{
    if (!allocator || !ptr_to_memory) // bound
    {
        return;
    }

    Block *current = (Block *)((char *)ptr_to_memory - sizeof(Block));
    if (!current)
        return;

    current->is_free = true;

    allocator->free_list;
    while (current && current->next)
    {
        if (current->is_free && current->next->is_free)
        {
            current->size += current->next->size + sizeof(Block);
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}
