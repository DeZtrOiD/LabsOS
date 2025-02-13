#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "lab3_defines.h"


typedef enum PARSER_FLAGS {
    CARRY_DIVIDED = 0b1,
    CARRY_DIVISOR = 0b10,
    SPACE_FOUND   = 0b100,
    ENDLINE_FOUND = 0b1000,
    CUR_VAL_DIGIT = 0b10000,
    CUR_VAL_SPACE = 0b100000,
    CUR_VAL_ENDL  = 0b1000000,
    CUR_VAL_SIGN  = 0b10000000,
    SIGN_FOUND    = 0b100000000,
    CUR_VAL_ZERO  = 0b1000000000,
    ZERO_FOUND    = 0b10000000000,
} PARSER_FLAGS;

void exitChilde(int, int, int, const char *, int, struct shm_handler*);
void stopChilde(float, int, int, struct shm_handler*);

int main(int argc,char **argv) {


        int fd = atoi(argv[1]);
        char strBuf[STR_BUF_SIZE];
        float divided = 0;
        int divisor = 0;
        int parser_flag = CARRY_DIVIDED | ENDLINE_FOUND;

        struct shm_handler shm_obj;
        shm_obj.shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_obj.shm_fd == -1) {
            close(fd);
            const char mes[] = "Error shm_open.\n";
            exit(SHM_ERROR);
        }

        shm_obj.shm_ptr = (char*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_obj.shm_fd, 0);
        if (shm_obj.shm_ptr == MAP_FAILED) {
            close(fd);
            const char mes[] = "Error mmap.\n";
            exit(SHM_ERROR);
        }

        shm_obj.sem_wr = sem_open(SEM_WRITE, O_CREAT, 0666, 0);
        shm_obj.sem_rd = sem_open(SEM_READ, O_CREAT, 0666, 0);
        if (shm_obj.sem_wr == SEM_FAILED || shm_obj.sem_rd == SEM_FAILED) {
            munmap(shm_obj.shm_ptr, SHM_SIZE);
            close(fd);
            const char mes[] = "Error sem_open.\n";
            exit(SEM_ERROR);
        }


        while (1) {

            int number = 0;
            char sig[1] = {(char)READY_TO_READ};
            strBuf[0] = (char)READY_TO_READ;
            strBuf[1] = '\0';

            sem_wait(shm_obj.sem_rd);
            memcpy(shm_obj.shm_ptr, strBuf, STR_BUF_SIZE);

            sem_post(shm_obj.sem_wr);
            sem_wait(shm_obj.sem_rd);
            memcpy(strBuf, shm_obj.shm_ptr, STR_BUF_SIZE);
            number = strlen(strBuf);
            if (number < 1) {
                const char mes[] = "Error: failed to read from shm\n";
                exitChilde(READ_ERR, fd, ERROR_MES_FROM_CHILD, mes, sizeof(mes), &shm_obj);
            }
            char* ptr = strBuf;
            for (; ptr < strBuf + number; ++ptr) {
                if (*ptr == '0' && (parser_flag & (ENDLINE_FOUND | SPACE_FOUND | SIGN_FOUND | ZERO_FOUND)))
                    parser_flag |= CUR_VAL_ZERO;
                else if (isdigit(*ptr)) parser_flag |= CUR_VAL_DIGIT;
                else if (*ptr == ' ') parser_flag |= CUR_VAL_SPACE;
                else if (*ptr == '\n') parser_flag |= CUR_VAL_ENDL;
                else if (*ptr == '-' || *ptr == '+') parser_flag |= CUR_VAL_SIGN;
                
                switch (parser_flag) {
                    
                    case CARRY_DIVIDED | CUR_VAL_DIGIT:
                    case CARRY_DIVIDED | CUR_VAL_DIGIT | ENDLINE_FOUND:
                        divided = divided * 10 + *ptr - '0';
                        parser_flag = CARRY_DIVIDED;
                        break;
                    case CARRY_DIVIDED | CUR_VAL_DIGIT | SIGN_FOUND:
                        divided = divided * (*ptr - '0');
                        parser_flag = CARRY_DIVIDED;
                        break;


                    case CARRY_DIVIDED | CUR_VAL_SPACE:
                    case CARRY_DIVIDED | CUR_VAL_SPACE | ZERO_FOUND:
                        parser_flag = CARRY_DIVISOR | SPACE_FOUND;
                        break;
                    

                    case CARRY_DIVIDED | CUR_VAL_ENDL:
                    case CARRY_DIVIDED | CUR_VAL_ENDL | ZERO_FOUND:
                        char result[14];
                        int len = sprintf(result, "%g\n", divided);
                        if (write(fd, result, len * sizeof(char)) == -1) {
                            const char mes[] = "Error: failed to write to file\n";
                            exitChilde(WRITE_ERR, fd, ERROR_MES_FROM_CHILD, mes, sizeof(mes), &shm_obj);
                        }
                        divided = 0;
                        parser_flag = CARRY_DIVIDED | ENDLINE_FOUND;
                        break;


                    case CARRY_DIVIDED | CUR_VAL_SIGN | ENDLINE_FOUND:
                        if (*ptr == '-') divided = -1;
                        else divided = 1;
                        parser_flag = CARRY_DIVIDED | SIGN_FOUND;
                        break;


                    case CARRY_DIVIDED | CUR_VAL_ZERO | ENDLINE_FOUND:
                    case CARRY_DIVIDED | CUR_VAL_ZERO | SIGN_FOUND:
                        divided = 0;
                        parser_flag = CARRY_DIVIDED | ZERO_FOUND;
                        break;



                    case CARRY_DIVISOR | CUR_VAL_DIGIT:
                    case CARRY_DIVISOR | CUR_VAL_DIGIT | SPACE_FOUND:
                        divisor = divisor * 10 + *ptr - '0';
                        parser_flag = CARRY_DIVISOR;
                        break;
                    case CARRY_DIVISOR | CUR_VAL_DIGIT | SIGN_FOUND:
                        divisor = divisor * (*ptr - '0');
                        parser_flag = CARRY_DIVISOR;
                        break;


                    case CARRY_DIVISOR | CUR_VAL_SPACE:
                    case CARRY_DIVISOR | CUR_VAL_SPACE | ZERO_FOUND:
                        if (divisor == 0) {
                            stopChilde(divided, fd, STOP_MES, &shm_obj);
                            divided = 0;
                        }
                        else {
                            divided /= divisor;
                            divisor = 0;
                            parser_flag = CARRY_DIVISOR | SPACE_FOUND;
                        }
                        break;


                    case CARRY_DIVISOR | CUR_VAL_ENDL:
                    case CARRY_DIVISOR | CUR_VAL_ENDL | ZERO_FOUND:
                        if (divisor == 0) {
                            stopChilde(divided, fd, STOP_MES, &shm_obj);
                            divided = 0;
                        }
                        else {
                            divided /= divisor;
                            divisor = 0;
                            char result[14];
                            int len = sprintf(result, "%g\n", divided);
                            if (write(fd, result, sizeof(char) * len) == -1) {
                                const char mes[] = "Error: failed to write to file\n";
                                exitChilde(WRITE_ERR, fd, ERROR_MES_FROM_CHILD, mes, sizeof(mes), &shm_obj);
                            }
                            divided = 0;
                            parser_flag = CARRY_DIVIDED | ENDLINE_FOUND;
                        }
                        break;


                    case CARRY_DIVISOR | CUR_VAL_SIGN | SPACE_FOUND:
                        if (*ptr == '-') divisor = -1;
                        else divisor = 1;
                        parser_flag = CARRY_DIVISOR | SIGN_FOUND;
                        break;


                    case CARRY_DIVISOR | CUR_VAL_ZERO | SPACE_FOUND:
                    case CARRY_DIVISOR | CUR_VAL_ZERO | SIGN_FOUND:
                        divisor = 0;
                        parser_flag = CARRY_DIVISOR | ZERO_FOUND;
                        break;
                        
                    default:
                    /*  
                        ERRORS:
                        case CARRY_DIVIDED | CUR_VAL_SPACE | ENDLINE_FOUND
                        case CARRY_DIVIDED | CUR_VAL_ENDL | ENDLINE_FOUND
                        case CARRY_DIVISOR | CUR_VAL_SPACE | SPACE_FOUND
                        case CARRY_DIVISOR | CUR_VAL_ENDL | SPACE_FOUND
                        case CARRY_DIVIDED | CUR_VAL_SIGN ...
                        IMPOSSIBLE:
                        case CARRY_DIVIDED | CUR_VAL_DIGIT | SPACE_FOUND : SPACE_FOUND -> CARRY_DIVISOR
                        case CARRY_DIVIDED | CUR_VAL_SPACE | SPACE_FOUND
                        case CARRY_DIVIDED | CUR_VAL_ENDL | SPACE_FOUND
                        case CARRY_DIVISOR | CUR_VAL_DIGIT | ENDLINE_FOUND : ENDLINE_FOUND -> CARRY_DIVIDED
                        case CARRY_DIVISOR | CUR_VAL_SPACE | ENDLINE_FOUND
                        case CARRY_DIVISOR | CUR_VAL_SPACE | ENDLINE_FOUND
                        case CARRY_DIVIDED | CUR_VAL_SIGN | SPACE_FOUND ...
                    */
                        const char mes[] = "Error: wrong inscturction\n";
                        exitChilde(WRONG_INSTRUCTION_ERR, fd, ERROR_MES_FROM_CHILD, mes, sizeof(mes), &shm_obj);
                        break;
                }
            }
        }
}

void exitChilde(int code, int fd, int sig, const char *const mes, int mes_size, struct shm_handler *shm_obj) {
    char buf[2] = {(char)sig, '\0'};
    // shm_obj.sem_wr
    sem_wait(shm_obj->sem_rd);
    memcpy(shm_obj->shm_ptr, buf, sizeof(buf));
    write(STDERR_FILENO, mes, mes_size);
    close(fd);
    sem_post(shm_obj->sem_wr);
    sem_close(shm_obj->sem_rd);
    sem_close(shm_obj->sem_wr);
    exit(code);
}

void stopChilde(float divided, int fd, int mes_to_parent, struct shm_handler *shm_obj) {
    char res[14];
    int len = sprintf(res, "%g\n", divided);
    if (write(fd, res, sizeof(char) * len) == -1) {
        const char mes[] = "Error: failed to write to file\n";
        exitChilde(WRITE_ERR, fd, ERROR_MES_FROM_CHILD, mes, sizeof(mes), shm_obj);
    }
    res[0] = (char)mes_to_parent;
    res[1] = '\0';
    sem_wait(shm_obj->sem_rd);
    memcpy(shm_obj->shm_ptr, res, sizeof(res));
    close(fd);
    sem_post(shm_obj->sem_wr);
    sem_close(shm_obj->sem_rd);
    sem_close(shm_obj->sem_wr);
    exit(EXIT_SUCCESS);
}
