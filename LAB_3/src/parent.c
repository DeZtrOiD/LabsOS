#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include "lab3_defines.h"

static const int FILE_NAME_BUF_SIZE = 15;
static char CHILDE_PROGRAM_NAME[] = "childe"; // child
static const int PATH_SIZE = 1024;

void exitParent(int, const char* const, int);

int main() {

    char progpath[PATH_SIZE];
    ssize_t len = readlink("/proc/self/exe", progpath, sizeof(progpath) / sizeof(char) - sizeof(char));
    if (len == -1) {
        const char mes[] = "Error: failed to read full program path\n";
        exitParent(READ_PATH_ERR, mes, sizeof(mes));
    }
    while (progpath[len] != '/')
        --len;
    progpath[len] = '\0';


    char buff[FILE_NAME_BUF_SIZE];
    int nameFileLen = 0;
    if ((nameFileLen = read(STDIN_FILENO, buff, sizeof(buff))) == -1) {
        const char mes[] = "Error: failed to read from STDIN\n";
        exitParent(READ_ERR, mes, sizeof(mes));
    }
    else if (buff[nameFileLen - 1] != '\n') {
        const char mes[] = "Error: file name too long\n";
        exitParent(WRONG_FILE_NAME_ERR, mes, sizeof(mes));
    }
    buff[nameFileLen - 1] = '\0';


    int fd = open(buff, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXO);
    if (fd == -1) {
        const char mes[] = "Error: failed to open file\n";
        exitParent(OPEN_FILE_ERR, mes, sizeof(mes));
    }
    /*
        int pToChilde[2];
        if (pipe(pToChilde)) {
            close(fd);
            const char mes[] = "Error: failed to create pipe\n";
            exitParent(CREATE_PIPE_ERR, mes, sizeof(mes));
        }
        int pToParent[2];
        if (pipe(pToParent)) {
            close(fd);
            close(pToChilde[1]);
            close(pToChilde[0]);
            const char mes[] = "Error: failed to create pipe\n";
            exitParent(CREATE_PIPE_ERR, mes, sizeof(mes));
        }
    */

    struct shm_handler shm_obj;
    shm_obj.shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_obj.shm_fd == -1){
        close(fd);
        const char mes[] = "Error shm_open.\n";
        exitParent(SHM_ERROR, mes, sizeof(mes));
    }

    if (ftruncate(shm_obj.shm_fd, BUFSIZ) == -1){
        shm_unlink(SHM_NAME);
        close(fd);
        const char mes[] = "Error ftruncate.\n";
        exitParent(SHM_ERROR, mes, sizeof(mes));
    }

    shm_obj.shm_ptr = (char*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_obj.shm_fd, 0);
    if (shm_obj.shm_ptr == MAP_FAILED) {
        shm_unlink(SHM_NAME);
        close(fd);
        const char mes[] = "Error mmap.\n";
        exitParent(SHM_ERROR, mes, sizeof(mes));
    }

    shm_obj.sem_wr = sem_open(SEM_WRITE, O_CREAT, 0666, 0);
    if (shm_obj.sem_wr == SEM_FAILED)
    {
        munmap(shm_obj.shm_ptr, SHM_SIZE);
        shm_unlink(SHM_NAME);
        close(fd);
        const char mes[] = "Error sem_open.\n";
        exitParent(SEM_ERROR, mes, sizeof(mes));
    }
    shm_obj.sem_rd = sem_open(SEM_READ, O_CREAT, 0666, 0);
    if (shm_obj.sem_rd == SEM_FAILED) {
        munmap(shm_obj.shm_ptr, SHM_SIZE);
        shm_unlink(SHM_NAME);
        sem_close(shm_obj.sem_wr);
        sem_unlink(SEM_WRITE);
        close(fd);
        const char mes[] = "Error sem_open.\n";
        exitParent(SEM_ERROR, mes, sizeof(mes));
    }

    pid_t pid = fork();
    if (pid == -1) {
        sem_close(shm_obj.sem_rd);
        sem_close(shm_obj.sem_wr);
        sem_unlink(SEM_WRITE);
        sem_unlink(SEM_READ);
        close(fd);
        munmap(shm_obj.shm_ptr, SHM_SIZE);
        shm_unlink(SHM_NAME);
        const char mes[] = "Error: failed to create process\n";
        exitParent(CREATE_PROCESS_ERR, mes, sizeof(mes));
    }
    else if (pid == 0) {
        
        char path[PATH_SIZE];
        if (snprintf(path, sizeof(path) - sizeof(char), "%s/%s", progpath, CHILDE_PROGRAM_NAME) < 0){
            const char mes[] = "Error: failed to create full name of childe process\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            sem_post(shm_obj.sem_wr);
            exit(CAT_ERR);
        }
        char fd_str[FILE_NAME_BUF_SIZE];
        if (snprintf(fd_str, sizeof(fd_str), "%d", fd) < 0) {
            const char mes[] = "Error: failed to convert int FD to str FD\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            sem_post(shm_obj.sem_wr);
            exit(CONVERT_ERR);
        }

        char* const argv[] = {CHILDE_PROGRAM_NAME, fd_str, NULL};

        int status = execv(path, argv);

        if (status == -1) {
            char mes[] = "Error: failed to exec into new exectuable image\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            sem_post(shm_obj.sem_wr);
            exit(EXEC_ERR);
        }
    }
    // parent
    else {
        close(fd);
        char strBuf[STR_BUF_SIZE];
        strBuf[0] = (char)READY_TO_WRITE;
        strBuf[1] = '\0';
        memcpy(shm_obj.shm_ptr, strBuf, STR_BUF_SIZE);

        while (1) {
            sem_post(shm_obj.sem_rd);
            sem_wait(shm_obj.sem_wr);
            memcpy(strBuf, shm_obj.shm_ptr, STR_BUF_SIZE - 1);
            if (strBuf[0] == (char)READY_TO_READ) {
                int number = read(STDIN_FILENO, strBuf, (STR_BUF_SIZE - 1) * sizeof(char));
                strBuf[STR_BUF_SIZE - 1] = '\0';
                if (number == -1) {
                    strBuf[0] = (char)ERROR_MES_FROM_PARENT;
                    strBuf[1] = '\0';
                    memcpy(shm_obj.shm_ptr, strBuf, STR_BUF_SIZE);
                    sem_post(shm_obj.sem_rd);
                    wait(NULL);
                    sem_close(shm_obj.sem_rd);
                    sem_close(shm_obj.sem_wr);
                    sem_unlink(SEM_WRITE);
                    sem_unlink(SEM_READ);
                    munmap(shm_obj.shm_ptr, SHM_SIZE);
                    shm_unlink(SHM_NAME);
                    const char mes[] = "Error: failed to read from STDIN\n";
                    exitParent(READ_ERR, mes, sizeof(mes));
                }
                strBuf[number] = '\0';
                memcpy(shm_obj.shm_ptr, strBuf, STR_BUF_SIZE);
                sem_post(shm_obj.sem_rd);
            }
            else if (strBuf[0] == (char)STOP_MES) {
                wait(NULL);
                sem_close(shm_obj.sem_rd);
                sem_close(shm_obj.sem_wr);
                sem_unlink(SEM_WRITE);
                sem_unlink(SEM_READ);
                munmap(shm_obj.shm_ptr, SHM_SIZE);
                shm_unlink(SHM_NAME);
                exit(EXIT_SUCCESS);
            }
            else {
                const char mes[] = "Error: error in child process\n";
                int status = 0;
                sem_post(shm_obj.sem_rd);
                wait(&status);
                sem_close(shm_obj.sem_rd);
                sem_close(shm_obj.sem_wr);
                sem_unlink(SEM_WRITE);
                sem_unlink(SEM_READ);
                munmap(shm_obj.shm_ptr, SHM_SIZE);
                shm_unlink(SHM_NAME);
                exitParent(CHILD_ERROR + status, mes, sizeof(mes));
            }
        }
    }
}

void exitParent(int code, const char* const message, int mes_size) {
    write(STDERR_FILENO, message, mes_size);
    exit(code);
}
