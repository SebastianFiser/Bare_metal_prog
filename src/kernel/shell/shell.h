#pragma once

struct fs_node;

void shell_prompt(void);
void shell_execute_command(const char *cmd);
struct fs_node *shell_get_cwd(void);
