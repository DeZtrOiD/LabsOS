
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

const int ARRAY_SIZE = 1024 * 1024 * 16;

enum ErrorCodes {
    WRONG_ARGS = 1,
    WRONG_THREADS,

};

typedef enum Direction {
    INCREASING,
    DECREASING,
    ALLOC_ERR
} Direction;

typedef struct thread_data {
    int *arr;
    int start;
    int count;
    Direction dir;
    int spawn;
} thread_data;

void cmp_swap(int*, int, int, Direction);
void bitsort_merge(int*, int, int, Direction);
void* bitsort(void*);
int greatest_power_of_two_in_num(int);
void exit_err(int, char *, int);


int main(int argc, char** argv) {
    
    if (argc != 2) {
        char mes[] = "Invalid number of arguments.\n";
        exit_err(WRONG_ARGS, mes, sizeof(mes));
    }
    int* arr = (int*)malloc(ARRAY_SIZE * sizeof(int));
    if (arr == NULL) {
        char mes[] = "Memory allocation error";
        exit_err(ALLOC_ERR, mes, sizeof(mes));
    }
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        arr[i] = rand() % 100000;
    }
    int threads_num = atoi(argv[1]);
    if (threads_num < 1) {
        char mes[] = "Invalid number of threads.\n";
        exit_err(WRONG_THREADS, mes, sizeof(mes));
    }
    int p_thr = 0;
    p_thr = greatest_power_of_two_in_num(threads_num);
    if (threads_num & (threads_num - 1) != 0) {
        threads_num = 1 << p_thr;
    }
    thread_data d = {arr, 0, ARRAY_SIZE, INCREASING, p_thr};
    bitsort(&d);
    free(arr);
}

void exit_err(int code, char* mes, int mes_size) {
    write(STDERR_FILENO, mes, mes_size);
    exit(code);
}

void* bitsort(void* args) {
    thread_data* data = (thread_data*)args;
    if (data->count > 1) {
        if (data->spawn > 0) {
            int middle = data->count / 2;
            pthread_t thread_1, thread_2;
            thread_data left = {
                data->arr,
                data->start,
                middle,
                INCREASING,
                data->spawn - 1,
            };
            if (pthread_create(&thread_1, NULL, bitsort, (void *)&left)){
                char mes[] = "Failed to create thread.\n";
                write(STDERR_FILENO, mes, sizeof(mes));
                bitsort((void*)&left);

                thread_data right = {
                    data->arr,
                    data->start + middle,
                    middle,
                    DECREASING,
                    data->spawn - 1,
                };
                bitsort((void *)&right);
            }
            else {
                thread_data right = {
                    data->arr,
                    data->start + middle,
                    middle,
                    DECREASING,
                    data->spawn - 1,
                };
                bitsort((void *)&right);
                pthread_join(thread_1, NULL);
            }
        }
        else {
            int middle = data->count / 2;
            thread_data left = {data->arr, data->start, middle, INCREASING, data->spawn};
            bitsort((void*)&left);
            thread_data right = {data->arr, data->start + middle, middle, DECREASING, data->spawn};
            bitsort((void*)&right);
        }
        bitsort_merge(data->arr, data->start, data->count, data->dir);
    }
}

void bitsort_merge(int* arr, int start, int count, Direction dir) {
    if (count > 1) {
        int middle = count / 2;
        for (int i = start; i < start + middle; ++i) {
            cmp_swap(arr, i, i + middle, dir);
        }
        bitsort_merge(arr, start, middle, dir);
        bitsort_merge(arr, start + middle, middle, dir);
    }
}

void cmp_swap(int *arr, int left, int right, Direction dir) {
    if ((arr[left] > arr[right] && dir == INCREASING) ||
    (arr[left] < arr[right] && dir == DECREASING)) {
        int tmp = arr[left];
        arr[left] = arr[right];
        arr[right] = tmp;
    }
}
int greatest_power_of_two_in_num(int n) {
    int p = 0;
    while (n > 1) {
        p += 1;
        n /= 2;
    }
    return p;
}
