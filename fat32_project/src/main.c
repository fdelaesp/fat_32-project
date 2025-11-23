#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fat32.h"
#include "../include/commands.h"

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 10

/* Parse command line input */
int parse_input(char *input, char **args) {
    int argc = 0;
    int in_quotes = 0;
    char *start = input;

    while (*input) {
        if (*input == '"') {
            if (!in_quotes) {
                in_quotes = 1;
                start = input + 1;
            } else {
                *input = '\0';
                args[argc++] = start;
                in_quotes = 0;
                start = input + 1;
            }
        } else if (*input == ' ' && !in_quotes) {
            if (input > start) {
                *input = '\0';
                args[argc++] = start;
            }
            start = input + 1;
        }
        input++;
    }

    if (input > start && !in_quotes) {
        args[argc++] = start;
    }

    return argc;
}

/* Main shell loop */
void shell_loop(FileSystem *fs) {
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARGS];

    while (1) {
        /* Print prompt */
        printf("[%s]%s/>", fs->image_name, fs->current_path);
        fflush(stdout);

        /* Read input */
        if (!fgets(input, MAX_INPUT_SIZE, stdin)) {
            break;
        }

        /* Remove newline */
        input[strcspn(input, "\n")] = '\0';

        /* Skip empty lines */
        if (strlen(input) == 0) {
            continue;
        }

        /* Parse input */
        int argc = parse_input(input, args);
        if (argc == 0) {
            continue;
        }

        /* Process commands */
        if (strcmp(args[0], "exit") == 0) {
            break;
        } else if (strcmp(args[0], "info") == 0) {
            if (argc != 1) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_info(fs);
            }
        } else if (strcmp(args[0], "ls") == 0) {
            if (argc != 1) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_ls(fs);
            }
        } else if (strcmp(args[0], "cd") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_cd(fs, args[1]);
            }
        } else if (strcmp(args[0], "mkdir") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_mkdir(fs, args[1]);
            }
        } else if (strcmp(args[0], "creat") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_creat(fs, args[1]);
            }
        } else if (strcmp(args[0], "open") == 0) {
            if (argc != 3) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_open(fs, args[1], args[2]);
            }
        } else if (strcmp(args[0], "close") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_close(fs, args[1]);
            }
        } else if (strcmp(args[0], "lsof") == 0) {
            if (argc != 1) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_lsof(fs);
            }
        } else if (strcmp(args[0], "lseek") == 0) {
            if (argc != 3) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                uint32_t offset = atoi(args[2]);
                cmd_lseek(fs, args[1], offset);
            }
        } else if (strcmp(args[0], "read") == 0) {
            if (argc != 3) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                uint32_t size = atoi(args[2]);
                cmd_read(fs, args[1], size);
            }
        } else if (strcmp(args[0], "write") == 0) {
            if (argc != 3) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_write(fs, args[1], args[2]);
            }
        } else if (strcmp(args[0], "mv") == 0) {
            if (argc != 3) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_mv(fs, args[1], args[2]);
            }
        } else if (strcmp(args[0], "rm") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_rm(fs, args[1]);
            }
        } else if (strcmp(args[0], "rmdir") == 0) {
            if (argc != 2) {
                printf("Error: Incorrect number of arguments\n");
            } else {
                cmd_rmdir(fs, args[1]);
            }
        } else {
            printf("Error: Unknown command\n");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FAT32 image file>\n", argv[0]);
        return 1;
    }

    FileSystem fs;
    if (mount_image(&fs, argv[1]) < 0) {
        fprintf(stderr, "Error: Cannot open image file\n");
        return 1;
    }

    shell_loop(&fs);

    close_image(&fs);
    return 0;
}
