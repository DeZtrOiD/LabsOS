#include "dl.h"
#include <sys/mman.h>

#define MEMORY_POOL_SIZE 1024 * 1024

static Allocator *allocator_create_stub(void *const memory, const size_t size)
{
    void *mapped_memory = mmap(memory, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapped_memory == MAP_FAILED)
    {
        const char err_msg[] = "allocator_create: mmap failed\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
        return NULL;
    }
    return (Allocator *)mapped_memory;
}

static void allocator_destroy_stub(Allocator *const allocator)
{
    if (allocator && munmap(allocator, MEMORY_POOL_SIZE) == -1)
    {
        const char err_msg[] = "allocator_destroy: munmap failed\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
    }

}

static void *allocator_alloc_stub(Allocator *const allocator, const size_t size)
{
    void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_memory == MAP_FAILED)
    {
        const char err_msg[] = "allocator_alloc: mmap failed\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
        return NULL;
    }
    return mapped_memory;
}

static void allocator_free_stub(Allocator *const allocator, void *const memory)
{
    if (memory && munmap(memory, sizeof(memory)) == -1)
    {
        const char err_msg[] = "allocator_free: munmap failed\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
    }
}

static allocator_create_f *allocator_create;
static allocator_destroy_f *allocator_destroy;
static allocator_alloc_f *allocator_alloc;
static allocator_free_f *allocator_free;

int main(int argc, char **argv)
{
    void *library = NULL;

    if (argc == 2) {
        library = dlopen(argv[1], RTLD_LOCAL | RTLD_NOW);

        if (!library)
        {
            const char msg[] = "Failed to load library. Using stub.\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            allocator_create = allocator_create_stub;
            allocator_destroy = allocator_destroy_stub;
            allocator_alloc = allocator_alloc_stub;
            allocator_free = allocator_free_stub;
        }
        else
        {
            allocator_create = dlsym(library, "allocator_create");
            allocator_destroy = dlsym(library, "allocator_destroy");
            allocator_alloc = dlsym(library, "allocator_alloc");
            allocator_free = dlsym(library, "allocator_free");

            if (!allocator_create)
            {
                const char msg[] = "Failed to load allocator_create. Using stub.\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                allocator_create = allocator_create_stub;
            }
            if (!allocator_destroy)
            {
                const char msg[] = "Failed to load allocator_destroy. Using stub.\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                allocator_destroy = allocator_destroy_stub;
            }
            if (!allocator_alloc)
            {
                const char msg[] = "Failed to load allocator_alloc. Using stub.\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                allocator_alloc = allocator_alloc_stub;
            }
            if (!allocator_free)
            {
                const char msg[] = "Failed to load allocator_free. Using stub.\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                allocator_free = allocator_free_stub;
            }
        }
    }
    else
    {
        const char msg[] = "Incorrect use. Using stub.\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        allocator_create = allocator_create_stub;
        allocator_destroy = allocator_destroy_stub;
        allocator_alloc = allocator_alloc_stub;
        allocator_free = allocator_free_stub;
    }

    // Teсты библиотеки
    size_t size = MEMORY_POOL_SIZE;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
    {
        dlclose(library);
        char message[] = "mmap failed\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return EXIT_FAILURE;
    }

    Allocator *allocator = allocator_create(addr, MEMORY_POOL_SIZE);

    if (!allocator)
    {
        const char msg[] = "Failed to initialize allocator\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        munmap(addr, size);
        dlclose(library);

        return EXIT_FAILURE;
    }

    int *int_block = (int *)allocator_alloc(allocator, sizeof(int));
    if (int_block)
    {
        *int_block = 42;
        const char msg[] = "Allocated int_block with value 42\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }
    else
    {
        const char msg[] = "Failed to allocate memory for int_block\n";
        write(STDERR_FILENO, msg, sizeof(msg));
    }

    float *float_block = (float *)allocator_alloc(allocator, sizeof(float));
    if (float_block)
    {
        *float_block = 2.718f;
        const char msg[] = "Allocated float_block with value 2.718\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }
    else
    {
        const char msg[] = "Failed to allocate memory for float_block\n";
        write(STDERR_FILENO, msg, sizeof(msg));
    }

    if (int_block)
    {
        allocator_free(allocator, int_block);
        const char msg[] = "Freed int_block\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }

    if (float_block)
    {
        allocator_free(allocator, float_block);
        const char msg[] = "Freed float_block\n";
        write(STDOUT_FILENO, msg, sizeof(msg));
    }

    allocator_destroy(allocator);
    const char msg[] = "Allocator destroyed\n";
    write(STDOUT_FILENO, msg, sizeof(msg));

    if (library)
        dlclose(library);
    munmap(addr, size);

    return EXIT_SUCCESS;
}