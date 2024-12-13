#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h> 


static const int FILE_NAME_BUF_SIZE = 15;
static const int STR_BUF_SIZE = 50;
static char const CHILDE_PROGRAM_NAME[] = "childe";
static const int PATH_SIZE = 1024;


enum PIPE_SIGNALS {
    STOP_MES,
    READY_TO_READ,
    ERROR_MES,
};

enum ERR_FLG {
    // for parent.c and childe.c
    READ_ERR = 1,
    WRITE_ERR,
    WRONG_INSTRUCTION_ERR,
    // unic for parent.c
    WRONG_FILE_NAME_ERR,
    OPEN_FILE_ERR,
    CREATE_PIPE_ERR,
    CREATE_PROCESS_ERR,
    READ_PATH_ERR,
    CONVERT_ERR,
    CAT_ERR,
    EXEC_ERR,
    DUP2_ERR,
    CHILDE_ERROR,
};


void exitParent(int, char* const, int);


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

    
    pid_t pid = fork();
    if (pid == -1) {
        close(fd);
        close(pToChilde[1]);
        close(pToChilde[0]);
        close(pToParent[1]);
        close(pToParent[0]);
        const char mes[] = "Error: failed to create process\n";
        exitParent(CREATE_PROCESS_ERR, mes, sizeof(mes));
    }
    else if (pid == 0) {
        close(pToChilde[1]);
        close(pToParent[0]);

        if (dup2(pToChilde[0], STDIN_FILENO) == -1){
            const char mes[] = "Error: failed to dup\n";
            exit(DUP2_ERR);
        }
        if (dup2(pToParent[1], STDOUT_FILENO) == -1){
            const char mes[] = "Error: failed to dup\n";
            exit(DUP2_ERR);
        }

        close(pToChilde[0]);
        close(pToParent[1]);
        
        char path[PATH_SIZE];
        if (snprintf(path, sizeof(path) - sizeof(char), "%s/%s", progpath, CHILDE_PROGRAM_NAME) < 0){
            const char mes[] = "Error: failed to create full name of childe process\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            exit(CAT_ERR);
        }
        char fd_str[FILE_NAME_BUF_SIZE];
        if (snprintf(fd_str, sizeof(fd_str), "%d", fd) < 0) {
            const char mes[] = "Error: failed to convert int FD to str FD\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            exit(CONVERT_ERR);
        }

        char* const argv[] = {CHILDE_PROGRAM_NAME, fd_str, NULL};

        int status = execv(path, argv);

        if (status == -1) {
            char mes[] = "Error: failed to exec into new exectuable image\n";
            write(STDERR_FILENO, mes, sizeof(mes));
            exit(EXEC_ERR);
        }
    }
    // parent
    else {
        close(pToChilde[0]);
        close(pToParent[1]);
        close(fd);


        char strBuf[STR_BUF_SIZE];
        char sigHand[1] = {(char)ERROR_MES};
        while (1) {
            if (read(pToParent[0], sigHand, sizeof(sigHand)) == -1) {
                close(pToChilde[1]);
                close(pToParent[0]);
                wait(NULL);
                const char mes[] = "Error: failed to read from pipe\n";
                exitParent(READ_ERR, mes, sizeof(mes));
            }
            else if (sigHand[0] == (char)READY_TO_READ) {
                int number = read(STDIN_FILENO, strBuf, sizeof(strBuf));
                if (number == -1) {
                    close(pToChilde[1]);
                    close(pToParent[0]);
                    wait(NULL);
                    const char mes[] = "Error: failed to read from STDIN\n";
                    exitParent(READ_ERR, mes, sizeof(mes));
                }
                else if (write(pToChilde[1], strBuf, number) == -1) {
                    close(pToChilde[1]);
                    close(pToParent[0]);
                    wait(NULL);
                    const char mes[] = "Error: failed to write to pipe\n";
                    exitParent(WRITE_ERR, mes, sizeof(mes));
                }
            }
            else if (sigHand[0] == STOP_MES) {
                close(pToChilde[1]);
                close(pToParent[0]);
                wait(NULL);
                exit(EXIT_SUCCESS);
            }
            else {
                close(pToChilde[1]);
                close(pToParent[0]);
                const char mes[] = "Error: error in child process\n";
                int status = 0;
                wait(&status);
                exitParent(CHILDE_ERROR + status, mes, sizeof(mes));
            }
        }
    }
}

void exitParent(int code, char* const message, int mes_size) {
    write(STDERR_FILENO, message, mes_size);
    exit(code);
}
