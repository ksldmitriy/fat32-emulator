#include <stdio.h>
#include <unistd.h>

#include "fat32.h"
#include "shell.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Invalid agruments count\n Usage: fat32 FILE\n");
        return 0;
    }

    const char *filepath = argv[1];

    if (access(filepath, F_OK) != 0) {
        int ret = create_fat32_file(filepath);
        if (ret) {
            fprintf(stderr, "failed to create a fat32 file\n");
            return -1;
        }
    }

    return lauch_shell(filepath);
}
