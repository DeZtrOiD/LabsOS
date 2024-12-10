#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h> // sprintf() for cast float to char*


int strLen(char*);
int getFirstNumAndShift(float, float*, int*);
char* floatToStr(float);
void exitParent(int, int, int);
void exitChilde(int, int, int, int, char);
void stopChilde(float, int, int ,int, char);

const char STOP_MES = '1';
const char ERROR_MES = '2';
const char READY_TO_READ = '3';
const int FileNameBufSize = 15;
const int StrBufSize = 50;

typedef enum {
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
} FLAGSS;

typedef enum {
    READ_ERR = 1,
    WRONG_FILE_NAME_ERR,
    OPEN_FILE_ERR,
    CREATE_PIPE_ERR,
    CREATE_PROCESS_ERR,
    WRITE_ERR,
    WRONG_INSTRUCTION_ERR,
    CHILDE_ERROR,
} ERR_FLG;

int main() {
    char buff[FileNameBufSize];
    buff[FileNameBufSize - 1] = '\n';
    int nameFileLen = 0;
    if ((nameFileLen = read(STDIN_FILENO, buff, (int)sizeof(buff))) == -1) exit(READ_ERR);
    else if (buff[nameFileLen - 1] != '\n') exit(WRONG_FILE_NAME_ERR);
    buff[nameFileLen - 1] = '\0';
    int fd = open(buff, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXO);
    if (fd == -1) exit(OPEN_FILE_ERR);

    int pToChilde[2];
    if (pipe(pToChilde)) {
        close(fd);
        exit(CREATE_PIPE_ERR);
    }
    int pToParent[2];
    if (pipe(pToParent)) {
        close(fd);
        close(pToChilde[1]);
        close(pToChilde[0]);
        exit(CREATE_PIPE_ERR);
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(fd);
        close(pToChilde[1]);
        close(pToChilde[0]);
        close(pToParent[1]);
        close(pToParent[0]);
        exit(CREATE_PROCESS_ERR);
    }
    else if (pid == 0) {
        close(pToChilde[1]);
        close(pToParent[0]);
        char strBuf[StrBufSize];
        float divided = 0;
        int divisor = 0;
        int parser_flag = CARRY_DIVIDED | ENDLINE_FOUND;
        int end_cycle_flag = 0;
        while (!end_cycle_flag) {
            int number = 0;
            if (write(pToParent[1], &READY_TO_READ, sizeof(READY_TO_READ)) == -1) {
                exitChilde(WRITE_ERR, pToChilde[0], pToParent[1], fd, ERROR_MES);
            }
            if ((number = read(pToChilde[0], strBuf, (int)sizeof(strBuf))) == -1) {
                exitChilde(READ_ERR, pToChilde[0], pToParent[1], fd, ERROR_MES);}
            else if (number == 0) {
                if (parser_flag & SPACE_FOUND) {} // endl sont check
            }
            number /= sizeof(char);
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
                        char* result = floatToStr(divided);
                        int len = strLen(result);
                        if (write(fd, result, len * sizeof(char)) == -1) {
                            free(result);
                            exitChilde(WRITE_ERR, pToChilde[0], pToParent[1], fd, ERROR_MES);
                        }
                        free(result);
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
                            stopChilde(divided, pToChilde[0], pToParent[1], fd, STOP_MES);
                            divided = 0;
                            end_cycle_flag = 1;
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
                            stopChilde(divided, pToChilde[0], pToParent[1], fd, STOP_MES);
                            divided = 0;
                            end_cycle_flag = 1;
                        }
                        else {
                            divided /= divisor;
                            divisor = 0;
                            char* result = floatToStr(divided);
                            int len = strLen(result);
                            if (write(fd, result, sizeof(char) * len) == -1) {
                                free(result);
                                exitChilde(WRITE_ERR, pToChilde[0], pToParent[1], fd, ERROR_MES);
                            }
                            divided = 0;
                            free(result);
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
                        exitChilde(WRONG_INSTRUCTION_ERR, pToChilde[0], pToParent[1], fd, ERROR_MES);

                        break;
                }
                if (end_cycle_flag) break;
            }
        }
    }
    else {
        close(pToChilde[0]);
        close(pToParent[1]);
        close(fd);
        int end_cycle_flag = 0;
        char strBuf[StrBufSize];
        char sigHand[sizeof(STOP_MES) / sizeof(char)] = {'\0'};
        while (!end_cycle_flag) {
            if (read(pToParent[0], sigHand, sizeof(sigHand)) == -1) {
                exitParent(READ_ERR, pToChilde[1], pToParent[0]);
            }
            else if (sigHand[0] == READY_TO_READ) {
                int number = read(STDIN_FILENO, strBuf, sizeof(strBuf));
                if (number == -1) {
                    exitParent(READ_ERR, pToChilde[1], pToParent[0]);
                }
                else if (write(pToChilde[1], strBuf, number) == -1) {
                    exitParent(WRITE_ERR, pToChilde[1], pToParent[0]);
                }
            }
            else if (sigHand[0] == STOP_MES) {
                close(pToChilde[1]);
                close(pToParent[0]);
                end_cycle_flag = 1;
            }
            else exitParent(CHILDE_ERROR, pToChilde[1], pToParent[0]);
        }
        wait(NULL);
    }
    return 0;
}

void exitParent(int code, int pToChilde, int pToParent) {
    close(pToChilde);
    close(pToParent);
    wait(NULL);
    exit(code);
}

void exitChilde(int code, int pToChilde, int pToParent, int fd, char message) {
    write(pToParent, &message, sizeof(message));
    close(pToChilde);
    close(pToParent);
    close(fd);
    exit(code);
}

void stopChilde(float divided, int pToChilde, int pToParent, int fd, char message) {
    close(pToChilde);
    char* result = floatToStr(divided);
    int len = strLen(result);
    if (write(fd, result, sizeof(char) * len) == -1) {
        free(result);
        exitChilde(WRITE_ERR, pToChilde, pToParent, fd, ERROR_MES);
    }
    free(result);
    if (write(pToParent, &STOP_MES, sizeof(STOP_MES)) == -1)
        exitChilde(WRITE_ERR, pToChilde, pToParent, fd, ERROR_MES);
    close(pToParent);
    close(fd);
}

char* floatToStr(float num){
    char* str = (char*)calloc(15, sizeof(char));
    sprintf(str, "%g\n", num);
    return str;
}

int strLen(char* str){
    char* ptr = str;
    for (;*ptr != '\0'; ++ptr);
    return ptr - str;
}
