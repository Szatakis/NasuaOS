#ifndef COMMANDS_H
#define COMMANDS_H

void execute_command(const char *cmd);
void print_info(const char* msg);
void print_warn(const char* msg);
void print_error(const char* msg);
void fetch();

#endif // COMMANDS_H