#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "shared.h"

void execute_command_and_store_output(char *history, size_t *cnt, const char *input, const char *shell_name);
void handle_input(struct shmbuf *shmp, char *history, size_t *cnt, const char *input, const char *shell_name);

#endif /* CONTROLLER_H */