#include "executor.h"
#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

// Language configuration argument arrays
static const char *sh_args[]         = {"$NAME", "-euc", "$CODE", "--"};
static const char *awk_args[]        = {"awk", "$CODE"};
static const char *node_args[]       = {"node", "-e", "$CODE"};
static const char *python_args[]     = {"python", "-c", "$CODE"};
static const char *ruby_args[]       = {"ruby", "-e", "$CODE"};
static const char *php_args[]        = {"php", "-r", "$CODE"};
static const char *cmd_args[]        = {"cmd.exe", "/c", "$CODE"};
static const char *powershell_args[] = {"powershell.exe", "-c", "$CODE"};

// Language configuration mappings
static const struct language_config language_configs[] = {
    {"sh", sh_args, 4},
    {"bash", sh_args, 4},
    {"zsh", sh_args, 4},
    {"fish", sh_args, 4},
    {"dash", sh_args, 4},
    {"ksh", sh_args, 4},
    {"ash", sh_args, 4},
    {"shell", sh_args, 4},
    {"awk", awk_args, 2},
    {"js", node_args, 3},
    {"javascript", node_args, 3},
    {"py", python_args, 3},
    {"python", python_args, 3},
    {"rb", ruby_args, 3},
    {"ruby", ruby_args, 3},
    {"php", php_args, 3},
    {"cmd", cmd_args, 3},
    {"batch", cmd_args, 3},
    {"powershell", powershell_args, 3}};

const struct language_config *get_language_config(const char *lang) {
    const struct language_config *config = NULL;
    // Find language configuration
    for (size_t i = 0; i < sizeof(language_configs) / sizeof(language_configs[0]); i++) {
        if (strcasecmp(language_configs[i].name, lang) == 0) {
            config = &language_configs[i];
            break;
        }
    }
    return config;
}

// Execute code blocks for a given node
int execute_node(MD_NODE *node, char **args, int num_args) {
    int exit_code;
    info("Executing node: %s\n", node->text);

    info("Setting up environment variables\n");
    // First collect all nodes from root to target in a stack
    info("Env stack size: %d\n", node->level);
    MD_NODE *stack[node->level];
    int      stack_size = 0;
    MD_NODE *current    = node;
    while (current) {
        stack[stack_size++] = current;
        current             = current->parent;
    }

    // Now set environment variables from root to leaf (reverse order of stack)
    for (int i = stack_size - 1; i >= 0; i--) {
        ENV_ENTRY *env = stack[i]->env_entry;
        while (env) {
            if (env->value) {
                setenv(env->key, env->value, 1);
                info("Setenv %s=%s\n", env->key, env->value);
            } else {
                unsetenv(env->key);
                info("Unsetenv %s\n", env->key);
            }

            env = env->next;
        }
    }

    CODE_BLOCK *block = node->code_block;
    while (block) {
        if (block->info && block->content) {
            const char                   *lang   = block->info;
            const struct language_config *config = get_language_config(lang);

            if (config) {
                info("Executing code block: \n```%s\n%s```\n", block->info, block->content);
                info("Using language config: %s\n", config->name);

                // Fork and execute
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork failed");
                    return 0;
                }

                if (pid == 0) {
                    // Child process
                    // Calculate number of arguments needed
                    int total_args = config->prefix_args_count; // Prefix arguments
                    if (num_args > 0) total_args += num_args;   // User arguments

                    // Allocate argument array
                    char **exec_args = safe_malloc(total_args + 1);
                    if (!exec_args) {
                        _exit(1);
                    }

                    // Fill argument array with prefix args first
                    int arg_idx = 0;
                    for (size_t i = 0; i < config->prefix_args_count; i++) {
                        if (strcmp(config->prefix_args[i], "$CODE") == 0) {
                            exec_args[arg_idx++] = block->content;
                        } else if (strcmp(config->prefix_args[i], "$NAME") == 0) {
                            exec_args[arg_idx++] = (char *)config->name;
                        } else {
                            exec_args[arg_idx++] = (char *)config->prefix_args[i];
                        }
                    }

                    // Add user arguments
                    for (int i = 0; i < num_args; i++) {
                        exec_args[arg_idx++] = args[i];
                    }

                    exec_args[arg_idx] = NULL;

                    execvp(exec_args[0], exec_args);
                    perror("execvp failed");
                    free(exec_args);
                    _exit(1);
                } else {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);

                    exit_code = WEXITSTATUS(status);
                    if (!WIFEXITED(status) || exit_code != 0) {
                        info("Command failed with status %d\n", exit_code);
                    } else {
                        info("Command completed successfully %d\n", exit_code);
                    }
                }
            } else {
                error("Unsupported language: %s\n", lang);
                return 1;
            }
        }
        if (exit_code) {
            break;
        }
        block = block->next;
    }
    return exit_code;
}