#include "shared.h"
#include "model.h"
#include "controller.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

// pipe komutlarını ayırmak için
char **split_commands(const char *input, int *cmd_count)
{
    char *input_copy = strdup(input);
    char **commands = malloc(256 * sizeof(char *));
    int count = 0;

    char *token = strtok(input_copy, "|");
    while (token != NULL && count < 256)
    {
        while (*token == ' ')
            token++; // Baştaki boşlukları temizle
        commands[count] = strdup(token);
        count++;
        token = strtok(NULL, "|");
    }

    *cmd_count = count;
    free(input_copy);
    return commands;
}
// pipe komutlarını bağlayıp çalıştırır 
void execute_piped_commands(char *history, size_t *cnt, const char *input, const char *shell_name)
{
    int cmd_count;
    char **commands = split_commands(input, &cmd_count);
    if (cmd_count == 0)
    {
        free(commands);
        return;
    }

    int pipes[cmd_count - 1][2]; // Komutlar arası borular
    int output_pipe[2];          // Son komutun çıktısını yakalamak için
    pid_t *pids = malloc(cmd_count * sizeof(pid_t));

    // Boruları oluştur
    for (int i = 0; i < cmd_count - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
            errExit("pipe error");
    }
    if (pipe(output_pipe) == -1)
        errExit("output pipe error");

   
    for (int i = 0; i < cmd_count; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        { 
            if (i > 0)
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // Son komut değilse, bir sonraki boruya yaz
            if (i < cmd_count - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            else
            {
               
                dup2(output_pipe[1], STDOUT_FILENO);
            }

            // Kullanılmayan boru uçlarını kapat
            for (int j = 0; j < cmd_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(output_pipe[0]);
            if (i < cmd_count - 1)
                close(output_pipe[1]);

            // Komutu ayrıştır ve çalıştır
            char *args[256];
            int k = 0;
            args[k] = strtok(commands[i], " ");
            while (args[k] != NULL)
            {
                args[++k] = strtok(NULL, " ");
            }
            execvp(args[0], args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    }

    // Ana süreç: Kullanılmayan boru uçlarını kapat
    for (int i = 0; i < cmd_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(output_pipe[1]);

    // Son komutun çıktısını oku
    char output[BUF_SIZE] = "";
    ssize_t bytes_read;
    size_t total_bytes = 0;

    while ((bytes_read = read(output_pipe[0], output + total_bytes, BUF_SIZE - total_bytes - 1)) > 0)
    {
        total_bytes += bytes_read;
        output[total_bytes] = '\0';
    }
    close(output_pipe[0]);


    for (int i = 0; i < cmd_count; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    // Çıktıyı history tamponuna kaydet
    char formatted[BUF_SIZE + MAX_NAME_LEN];
    int written = snprintf(formatted, sizeof(formatted), "[%s ➜ %s]\n%s", shell_name, input, output);
    if (written < 0 || written >= sizeof(formatted))
    {
        formatted[sizeof(formatted) - 1] = '\0';
    }

    // History tamponuna ekle
    if (*cnt + strlen(formatted) < HISTORY_SIZE - 1)
    {
        strcat(history, formatted);
    }
    else
    {
        strcpy(history, formatted);
    }
    *cnt = strlen(history);

    // Belleği temizle
    for (int i = 0; i < cmd_count; i++)
    {
        free(commands[i]);
    }
    free(commands);
    free(pids);
}

// handle_input fonksiyonunda boru kontrolü
void handle_input(struct shmbuf *shmp, char *history, size_t *cnt, const char *input, const char *shell_name)
{
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    // Boru kontrolü
    if (strchr(input_copy, '|'))
    {
        execute_piped_commands(history, cnt, input_copy, shell_name);
        return;
    }

    //  komut işleme mantığı 
    if (strncmp(input, "@msg ", 5) == 0)
    {
        int shell_num = atoi(shell_name + 9);
        model_execute_with_fork(shmp, input + 5, shell_name, shell_num);
    }
    else if (strncmp(input, "@msgto ", 7) == 0)
    {
        int shell_num = atoi(shell_name + 9);
        const char *rest = input + 7;
        int target_shell = atoi(rest);
        while (*rest && *rest != ' ')
            rest++;
        while (*rest == ' ')
            rest++;
        if (target_shell > 0)
            model_send_private_message(shmp, rest, shell_name, shell_num, target_shell);
        else
            snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s] Hata: Geçersiz terminal numarası\n", shell_name);
    }
    else if (strncmp(input, "@file ", 6) == 0)
    {
        int shell_num = atoi(shell_name + 9);
        const char *filename = input + 6;
        while (*filename == ' ')
            filename++;
        model_send_file(shmp, filename, shell_name, shell_num);
    }
    else if (strncmp(input, "cd ", 3) == 0 || strcmp(input, "cd") == 0)
    {
        // Komutu history'ye ekle
        char formatted[BUF_SIZE + MAX_NAME_LEN];
        int written = snprintf(formatted, sizeof(formatted), "[%s ➜ %s]\n", shell_name, input);
        if (written < 0 || written >= sizeof(formatted))
        {
            formatted[sizeof(formatted) - 1] = '\0';
        }
        if (*cnt + strlen(formatted) < HISTORY_SIZE - 1)
        {
            strcat(history, formatted);
        }
        else
        {
            strcpy(history, formatted);
        }
        *cnt = strlen(history);
    
        char *path = NULL;
        if (strlen(input) > 3)
        {
            path = strdup(input + 3);
            if (!path)
            {
                snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s] Hata: Bellek tahsisi başarısız\n", shell_name);
                *cnt = strlen(history);
                return;
            }
            while (*path == ' ')
                path++;
        }
        if (!path || strlen(path) == 0)
        {
            path = getenv("HOME");
            if (!path)
            {
                snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s] Hata: HOME ortam değişkeni tanımlı değil\n", shell_name);
                *cnt = strlen(history);
                return;
            }
            path = strdup(path);
            if (!path)
            {
                snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s] Hata: Bellek tahsisi başarısız\n", shell_name);
                *cnt = strlen(history);
                return;
            }
        }
        if (chdir(path) == -1)
        {
            snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s] Hata: Dizin değiştirilemedi: %s\n", shell_name, strerror(errno));
        }
        *cnt = strlen(history);
        if (path && input + 3 != path)
            free(path);
        fflush(stdout); 
    }
    else
    {
        char *redirect = strstr(input_copy, ">");
        if (redirect)
        {
            *redirect = '\0';
            char *command = input_copy;
            char *filename = redirect + 1;
            while (*filename == ' ')
                filename++;

            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1)
            {
                perror("open failed");
                return;
            }

            pid_t pid = fork();
            if (pid == 0)
            {
                dup2(fd, STDOUT_FILENO);
                close(fd);
                char *args[256];
                int i = 0;
                args[i] = strtok(command, " ");
                while (args[i] != NULL)
                {
                    args[++i] = strtok(NULL, " ");
                }
                execvp(args[0], args);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                wait(NULL);
                close(fd);
                snprintf(history + *cnt, HISTORY_SIZE - *cnt, "[%s ➜ %s]\nOutput redirected to %s\n", shell_name, command, filename);
                *cnt = strlen(history);
            }
            else
            {
                errExit("fork error");
            }
        }
        else
        {
            execute_command_and_store_output(history, cnt, input, shell_name);
        }
    }
}