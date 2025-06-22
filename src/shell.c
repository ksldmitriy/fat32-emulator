#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat32.h"

static char **split_input(char *input, int *words) {
    if (!input)
        return NULL;

    int capacity = 10;
    int count = 0;
    char **result = malloc(capacity * sizeof(char *));
    if (!result)
        return NULL;

    char *token = strtok(input, " \t\r\n");
    while (token) {
        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(result, capacity * sizeof(char *));
            if (!tmp) {
                for (int i = 0; i < count; i++)
                    free(result[i]);
                free(result);
                return NULL;
            }
            result = tmp;
        }

        result[count] = strdup(token);
        if (!result[count]) {
            for (int i = 0; i < count; i++)
                free(result[i]);
            free(result);
            return NULL;
        }

        count++;
        token = strtok(NULL, " \t\r\n");
    }

    *words = count;

    result = realloc(result, (count + 1) * sizeof(char *));
    result[count] = NULL;

    return result;
}

int lauch_shell(const char *filepath) {
    char *cwd = malloc(2);
    strcpy(cwd, "/");

    char *line = NULL;
    size_t len = 0;

    while (1) {
        printf("%s>", cwd);
        fflush(stdout);

        ssize_t read = getline(&line, &len, stdin);
        if (read == -1) {
            fprintf(stderr, "failed to read input\n");
            break;
        }

        int word_count = 0;
        char **words = split_input(line, &word_count);

        if (word_count == 0) {
            // skip next checks
        } else if (strcmp(words[0], "format") == 0) {
            if (word_count != 1) {
                printf("invalid amount of arguments\nusage: format\n");
            } else {
                remove(filepath);
                create_fat32_file(filepath);
                cwd = realloc(cwd, 2);
                strcpy(cwd, "/");
            }
        } else if (strcmp(words[0], "ls") == 0) {
            if (word_count != 1 && word_count != 2) {
                printf("invalid amount of arguments\nusage: ls [path]\n");
            } else {
                char *path = word_count == 1 ? cwd : words[1];
                fat32_ls(filepath, path);
            }
        } else if (strcmp(words[0], "cd") == 0) {
            if (word_count != 2) {
                printf("invalid amount of arguments\nusage: cd <path>\n");
            } else if (fat32_is_directory(filepath, words[1])) {
                char *tmp = words[1];
                words[1] = cwd;
                cwd = tmp;
            } else {
                printf("no such directory\n");
            }
        } else if (strcmp(words[0], "mkdir") == 0) {
            if (word_count != 2) {
                printf("invalid amount of arguments\nusage: mkdir <path>\n");
            } else if (fat32_exists(filepath, words[1])) {
                printf("%s already exists\n", words[1]);
            } else {
                fat32_mkdir(filepath, words[1]);
            }
        } else if (strcmp(words[0], "touch") == 0) {
            if (word_count != 2) {
                printf("invalid amount of arguments\nusage: touch <path>\n");
            } else if (fat32_exists(filepath, words[1])) {
                printf("%s already exists\n", words[1]);
            } else {
                fat32_touch(filepath, words[1]);
            }
        } else {
            printf("no such command\n");
        }

        for (int i = 0; i < word_count; i++) {
            free(words[i]);
        }
        free(words);
    }

    free(cwd);
    free(line);
    return 0;
}
