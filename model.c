#include "shared.h"
#include "model.h"
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct
{
    pid_t pid;           
    char command[256];  
    int status;         // 0: çalışıyor, 1: bitti
} ProcessInfo;

static ProcessInfo process_table[100]; // Süreç tablosu
static int process_count = 0;          // Süreç sayısı

// Paylaşılan belleği başlatır
struct shmbuf *buf_init()
{
    // Paylaşılan bellek dosyasını aç/oluştur
    struct shmbuf *shmp;
    int fd = shm_open(SHARED_FILE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd < 0)
        errExit("could not open shared file");

    // Bellek boyutunu ayarla (Linux için)
#ifdef __linux__
    if (ftruncate(fd, sizeof(struct shmbuf)) == -1)
        errExit("ftruncate error");
#endif

    // Belleği programa bağla
    shmp = mmap(NULL, sizeof(struct shmbuf), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED)
        errExit("mmap error");

    // Semafor oluştur (aynı anda tek süreç yazsın)
    shmp->fd = fd;
    sem_t *semaphore = sem_open("/my_semaphore", O_CREAT, 0644, 1);
    if (semaphore == SEM_FAILED)
        errExit("sem_open failed");
    shmp->sem = semaphore;

    // Belleği sıfırla
    shmp->cnt = 0;
    memset(shmp->history, 0, HISTORY_SIZE);

    close(fd);
    return shmp;
}

// Komutu çalıştırıp çıktıyı geçmişe kaydeder
void execute_command_and_store_output(char *history, size_t *cnt, const char *input, const char *shell_name)
{
    // Boru oluştur (çıktıyı yakalamak için)
    int pipefd[2];
    if (pipe(pipefd) == -1)
        errExit("pipe error");

    pid_t pid = fork();
    if (pid == 0) 
    {
        // Çıktıyı boruya yönlendir
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Komutu parçala ve çalıştır
        char *args[256];
        char input_copy[256];
        strncpy(input_copy, input, sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';
        int i = 0;
        args[i] = strtok(input_copy, " ");
        while (args[i] != NULL)
            args[++i] = strtok(NULL, " ");
        execvp(args[0], args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) 
    {
        // Çıktıyı borudan oku
        close(pipefd[1]);
        char output[BUF_SIZE] = "";
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], output + strlen(output), BUF_SIZE - strlen(output) - 1)) > 0)
            output[strlen(output) + bytes_read] = '\0';
        close(pipefd[0]);

        waitpid(pid, NULL, 0); 

        
        char formatted[BUF_SIZE + MAX_NAME_LEN];
        snprintf(formatted, sizeof(formatted), "[%s ➜ %s]\n%s\n", shell_name, input, output);
        if (*cnt + strlen(formatted) < HISTORY_SIZE - 1)
            strcat(history, formatted);
        else
            strcpy(history, formatted);
        *cnt = strlen(history);

    
        process_table[process_count].pid = pid;
        strncpy(process_table[process_count].command, input, 256);
        process_table[process_count].status = 1;
        process_count++;
    }
    else
        errExit("fork error");
}

// Özel mesaj gönderir
void model_send_private_message(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num, int target_shell)
{
    sem_wait(shmp->sem); // Belleğe erişimi kilitle

    // Mesajı formatla
    char formatted[BUF_SIZE];
    snprintf(formatted, sizeof(formatted), "[%d|%s ➡️ %d]: %s\n", shell_num, shell_name, target_shell, msg);

    // Mesajı hedef terminalin özel geçmişine ekle
    int target_idx = target_shell - 1;
    if (target_idx >= 0 && target_idx < MAX_SHELLS)
    {
        if (strlen(shmp->private_history[target_idx]) + strlen(formatted) < HISTORY_SIZE - 1)
            strcat(shmp->private_history[target_idx], formatted);
        else
            strcpy(shmp->private_history[target_idx], formatted);
    }

    // Gönderenin geçmişine de ekle
    int sender_idx = shell_num - 1;
    if (sender_idx >= 0 && sender_idx < MAX_SHELLS && sender_idx != target_idx)
    {
        if (strlen(shmp->private_history[sender_idx]) + strlen(formatted) < HISTORY_SIZE - 1)
            strcat(shmp->private_history[sender_idx], formatted);
        else
            strcpy(shmp->private_history[sender_idx], formatted);
    }

    sem_post(shmp->sem); // Kilidi aç
}

// Genel mesaj gönderir
void model_send_message(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num)
{
    sem_wait(shmp->sem); // Belleği kilitle

    // Mesajı formatla ve geçmişe ekle
    char formatted[BUF_SIZE];
    snprintf(formatted, sizeof(formatted), "[%d|%s 🗨️]: %s\n", shell_num, shell_name, msg);
    if (shmp->cnt + strlen(formatted) < HISTORY_SIZE - 1)
        strcat(shmp->history, formatted);
    else
        strcpy(shmp->history, formatted);

    shmp->cnt = strlen(shmp->history);
    sem_post(shmp->sem); // Kilidi aç
}

// Mesajları okur
void model_read_messages(struct shmbuf *shmp, char *buffer, int shell_num)
{
    sem_wait(shmp->sem); // Belleği kilitle

    // Genel ve özel mesajları birleştir
    strncpy(buffer, shmp->history, HISTORY_SIZE - 1);
    int target_idx = shell_num - 1;
    if (target_idx >= 0 && target_idx < MAX_SHELLS)
        strncat(buffer, shmp->private_history[target_idx], HISTORY_SIZE - strlen(buffer) - 1);
    buffer[HISTORY_SIZE - 1] = '\0';

    sem_post(shmp->sem); // Kilidi aç
}

// Mesajı fork ile gönderir
void model_execute_with_fork(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num)
{
    pid_t pid = fork();
    if (pid == 0) 
    {
        model_send_message(shmp, msg, shell_name, shell_num);
        exit(EXIT_SUCCESS);
    }
    else if (pid > 0) 
        wait(NULL); 
    else
        errExit("fork error");
}

// Dosya içeriğini gönderir
void model_send_file(struct shmbuf *shmp, const char *filename, const char *shell_name, int shell_num)
{
    sem_wait(shmp->sem); // Belleği kilitle

    // Dosyayı oku ve formatla
    char formatted[BUF_SIZE];
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        snprintf(formatted, sizeof(formatted), "[%d|%s 📁]: Failed to open file '%s'\n", shell_num, shell_name, filename);
    else
    {
        char file_content[BUF_SIZE - 256];
        ssize_t bytes_read = read(fd, file_content, sizeof(file_content) - 1);
        if (bytes_read < 0)
            snprintf(formatted, sizeof(formatted), "[%d|%s 📁]: Error reading file '%s'\n", shell_num, shell_name, filename);
        else
        {
            file_content[bytes_read] = '\0';
            snprintf(formatted, sizeof(formatted), "[%d|%s 📁]: File '%s' contents:\n%s\n", shell_num, shell_name, filename, file_content);
        }
        close(fd);
    }

    // Geçmişe ekle
    if (shmp->cnt + strlen(formatted) < HISTORY_SIZE - 1)
        strcat(shmp->history, formatted);
    else
        strcpy(shmp->history, formatted);

    shmp->cnt = strlen(shmp->history);
    sem_post(shmp->sem); // Kilidi aç
}