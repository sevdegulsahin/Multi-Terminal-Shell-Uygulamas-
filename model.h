#ifndef MODEL_H
#define MODEL_H

#include "shared.h"

struct shmbuf *buf_init();
void execute_command_and_store_output(char *history, size_t *cnt, const char *input, const char *shell_name);
void model_send_message(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num);
void model_read_messages(struct shmbuf *shmp, char *buffer, int shell_num); 
void model_execute_with_fork(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num);
void model_send_file(struct shmbuf *shmp, const char *filename, const char *shell_name, int shell_num);
void model_send_private_message(struct shmbuf *shmp, const char *msg, const char *shell_name, int shell_num, int target_shell);

#endif /* MODEL_H */