#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>

#define SHARED_FILE_PATH "/multi_shell_shm"
#define BUF_SIZE 4096
#define HISTORY_SIZE 4096
#define MAX_NAME_LEN 64
#define MAX_SHELLS 10 // Maksimum terminal sayısı

#define errExit(msg)        \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

struct shmbuf
{
    sem_t *sem; // Semaphore işaretçisi
    size_t cnt;
    int fd;
    char history[HISTORY_SIZE];                     // Genel geçmiş (normal mesajlar için)
    char private_history[MAX_SHELLS][HISTORY_SIZE]; // Her terminal için özel mesajlar
};

#endif /* SHARED_H */