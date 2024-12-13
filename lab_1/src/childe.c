#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>


const int STR_BUF_SIZE = 50;


enum PIPE_SIGNALS {
    STOP_MES,
    READY_TO_READ,
    ERROR_MES,
};

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

typedef enum ERR_FLG {
    READ_ERR = 1,
    WRITE_ERR,
    WRONG_INSTRUCTION_ERR,
} ERR_FLG;


void exitChilde(int, int, int, char*, int);
void stopChilde(float, int, int);


int main(int argc,char **argv) {

        int fd = atoi(argv[1]);
        char strBuf[STR_BUF_SIZE];
        float divided = 0;
        int divisor = 0;
        int parser_flag = CARRY_DIVIDED | ENDLINE_FOUND;
        while (1) {
            int number = 0;
            char sig[1] = {(char)READY_TO_READ};
            if (write(STDOUT_FILENO, sig, sizeof(sig)) == -1) {
                const char mes[] = "Error: failed to write to pipe\n";
                exitChilde(WRITE_ERR, fd, ERROR_MES, mes, sizeof(mes));
            }
            else if ((number = read(STDIN_FILENO, strBuf, (int)sizeof(strBuf))) == -1) {
                const char mes[] = "Error: failed to read from pipe\n";
                exitChilde(READ_ERR, fd, ERROR_MES, mes, sizeof(mes));
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
                        char* result[14];
                        int len = sprintf(result, "%g\n", divided);
                        if (write(fd, result, len * sizeof(char)) == -1) {
                            const char mes[] = "Error: failed to write to file\n";
                            exitChilde(WRITE_ERR, fd, ERROR_MES, mes, sizeof(mes));
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
                            stopChilde(divided, fd, STOP_MES);
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
                            stopChilde(divided, fd, STOP_MES);
                            divided = 0;
                        }
                        else {
                            divided /= divisor;
                            divisor = 0;
                            char* result[14];
                            int len = sprintf(result, "%g\n", divided);
                            if (write(fd, result, sizeof(char) * len) == -1) {
                                const char mes[] = "Error: failed to write to file\n";
                                exitChilde(WRITE_ERR, fd, ERROR_MES, mes, sizeof(mes));
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
                        exitChilde(WRONG_INSTRUCTION_ERR, fd, ERROR_MES, mes, sizeof(mes));
                        break;
                }
            }
        }
}


void exitChilde(int code, int fd, int sig, char* const mes, int mes_size) {
    char buf[1] = {(char)sig};
    write(STDOUT_FILENO, buf, sizeof(buf));
    write(STDERR_FILENO, mes, mes_size);
    close(fd);
    exit(code);
}

void stopChilde(float divided, int fd, int mes_to_parent) {
    char res[14];
    int len = sprintf(res, "%g\n", divided);
    if (write(fd, res, sizeof(char) * len) == -1) {
        const char mes[] = "Error: failed to write to file\n";
        exitChilde(WRITE_ERR, fd, ERROR_MES, mes, sizeof(mes));
    }
    res[0] = (char)mes_to_parent;
    if (write(STDOUT_FILENO, res, sizeof(char)) == -1) {
        const char mes[] = "Error: failed to write to pipe\n";
        exitChilde(WRITE_ERR, fd, ERROR_MES, mes, sizeof(mes));
    }
    close(fd);
    exit(EXIT_SUCCESS);
}
