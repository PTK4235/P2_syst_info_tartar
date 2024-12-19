#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len) {
    for (int i = 0; i < len;) {
        printf("%04x:  ", (int) i);

        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++) {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

    size_t no_entries = 10;
    char *entries[10];

    for (size_t i = 0; i < 10; i++) {
        entries[i] = malloc(100 * sizeof(char));
        if (entries[i] == NULL) {
            perror("Failed to allocate memory for entry");
            // Libérer la mémoire déjà allouée
            for (size_t j = 0; j < i; j++) {
                free(entries[j]);
            }
            close(fd);
            return 1;
        }
    }

    int result = list(fd, "testar/sym", entries, &no_entries);
    if (result < 0) {
        printf("Error occurred during list operation.\n");
    } else if (result == 0) {
        printf("No directory found at given path in the archive.\n");
    } else {
        printf("Entries listed:\n");
        for (size_t i = 0; i < no_entries; i++) {
            printf("%zu : %s\n", i, entries[i]);
        }
    }
    printf("Number of entries: %zu\n", no_entries);

    for (size_t i = 0; i < 10; i++) {
        free(entries[i]);
    }

    close(fd);
    return 0;
}