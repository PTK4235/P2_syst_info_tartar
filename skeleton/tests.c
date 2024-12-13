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

    int fd = open(argv[1] , O_RDONLY);
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

    // int ret = check_archive(fd);
    // printf("check_archive returned %d\n", ret);

    // const char *path_to_check = "testar/doss/test.c";
    // int ret = exists(fd,(char*)path_to_check);
    // printf("check_path returned %d\n", ret);

    // char *dir_to_check = "doss/";
    // int ret = is_dir(fd, dir_to_check);
    // printf("is_dir returned %d\n", ret);

    size_t no_entries = 10;
    char *entries[10];

    for (size_t i = 0; i < 10; i++) {
        entries[i] = malloc(100 * sizeof(char)); 
        if (entries[i] == NULL) {
            perror("Failed to allocate memory for entry");
            return 1;
        }
    }

    int result = list(fd, "testar/doss/", entries, &no_entries);
    if (result != 0) {
        printf("No directory found at given path in the archive.\n");
    } else {
        printf("Entries listed:\n");
        for (size_t i = 0; i < no_entries; i++) {
            printf("%ld : %s\n", i,entries[i]);
        }
    }
    printf("n entries : %ld \n",no_entries);

    for (size_t i = 0; i < 10; i++) {
        free(entries[i]);
    }


    close(fd);
    return 0;
}