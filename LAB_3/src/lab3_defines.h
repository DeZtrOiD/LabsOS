#ifndef _LAB3_DEFINES
    #define _LAB3_DEFINES

    #include <semaphore.h>

    static const int STR_BUF_SIZE = 500;
    static const int SHM_SIZE = STR_BUF_SIZE * sizeof(char);
    const char SHM_NAME[] = "/shared_memory";
    const char SEM_WRITE[] = "/sem_write_";
    const char SEM_READ[] = "/sem_read_";

    enum ERR_FLG
    {
        READ_ERR = 1,
        WRITE_ERR,
        WRONG_INSTRUCTION_ERR,
        SHM_ERROR,
        SEM_ERROR,

        WRONG_FILE_NAME_ERR,
        OPEN_FILE_ERR,
        CREATE_PROCESS_ERR,
        READ_PATH_ERR,
        CONVERT_ERR,
        CAT_ERR,
        EXEC_ERR,
        CHILD_ERROR = 1000,
    };

    enum MEM_SIGNALS
    {
        STOP_MES,
        READY_TO_READ,
        ERROR_MES_FROM_CHILD,
        ERROR_MES_FROM_PARENT,
        READY_TO_WRITE,
    };

    struct shm_handler
    {
        int shm_fd;
        char *shm_ptr;
        sem_t *sem_wr;
        sem_t *sem_rd;
    };

#endif
