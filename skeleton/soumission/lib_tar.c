#include "lib_tar.h"



/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd) {
    tar_header_t header;
    int valid_arch;
    int nheader = 0;

    while (read(tar_fd, (void*)&header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        valid_arch = valid_archive(header,nheader);
        if (valid_arch != 0){
            return valid_arch;
        }
        nheader++;

        if (lseek(tar_fd, aligned_size(header), SEEK_CUR) == -1) {
            perror("fseek failed");
            return -3;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET)  == -1) {
        perror("fseek failed");
        return -3;
    }
    return nheader;
}

/**
 * Validates a tar archive header.
 *
 * Checks the header's `magic`, `version`, and checksum for validity. 
 * Returns the header index if `magic` is empty, signaling the end of the archive.
 *
 * @param header The tar header to validate.
 * @param nheader The current header index in the archive.
 * 
 * @return `nheader` if `magic` is empty, 0 if valid, 
 *         -1 for invalid `magic`, -2 for invalid `version`, -3 for invalid checksum.
 */
int valid_archive(tar_header_t header,int nheader){
    if(header.magic[0] == '\0'){
        return nheader;
    }
    else if (strncmp(header.magic,TMAGIC,TMAGLEN) != 0){
        perror("magic value not valid");
        return -1;
    }
    else if (strncmp(header.version, TVERSION, TVERSLEN) != 0) {
        perror("version not valid");
        return -2;
    }
    else if (check_sum(header) == 0){
        perror("checksum not valid");
        return -3;
    }
    return 0;
}

/**
 * Computes the aligned size of a file in a tar archive.
 *
 * @param header A tar header structure containing metadata about the file, including its size in octal format.
 *
 * @return The aligned size of the file, in bytes.
 */
long aligned_size(tar_header_t header){
    unsigned int size = strtol(header.size, NULL, 8);
    return (size % 512 == 0) ? size : size + (512 - (size % 512));
}


/**
 * Validates the checksum of a TAR archive header.
 *
 * @param header A tar_header_t structure representing a single header of the TAR archive.
 *
 * @return 1 if the calculated checksum matches the stored checksum, indicating the header is valid.
 *         0 if the calculated checksum does not match the stored checksum, indicating the header is invalid.
 *
 */
int check_sum(tar_header_t header){
    unsigned char *header_bytes = (unsigned char *)&header;
    unsigned int checksum = strtol(header.chksum, NULL, 8);
    unsigned int sum = 0;

    for (int i = 0; i < sizeof(tar_header_t); i++){
        if (i >= 148 && i <= 155){
            sum += ' ';
        }
        else{
            sum += header_bytes[i];
        }
    }
    return (sum == checksum);
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path) {
    tar_header_t header;

    while (read(tar_fd, (void*)&header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {

        if (header.name[0] == '\0') {
            return 0;
        }
        else if (strcmp(header.name, path) == 0) {
            return 1;
        }

        if (lseek(tar_fd, aligned_size(header), SEEK_CUR)  == -1) {
            perror("fseek failed");
            return -3;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET)  == -1) {
        perror("fseek failed");
        return -3;
    }
    
    return 0;
}

/**
 * Checks whether an entry in the archive matches the specified typeflag.
 *
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @param typeflag The typeflag to check against.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not the flag,
 *         any other value otherwise.
 */
int check_flag(int tar_fd, char *path, char typeflag){
    tar_header_t header;

    while (read(tar_fd, (void*)&header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (header.name[0] == '\0') {
            return 0;
        }
        if (strcmp(header.name, path) == 0){
            if (typeflag == REGTYPE && ((header.typeflag == REGTYPE) || (header.typeflag == AREGTYPE))){
                return 1;
            }
            if (header.typeflag == typeflag){
                
                return 1;
            }
            return 0;
        }
        

        if (lseek(tar_fd, aligned_size(header), SEEK_CUR) == -1) {
            perror("fseek failed");
            return -3;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET)  == -1) {
        perror("fseek failed");
        return -3;
    }
    return 0;

}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    return check_flag(tar_fd,path,DIRTYPE);
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    return check_flag(tar_fd,path,REGTYPE);
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    return check_flag(tar_fd,path,SYMTYPE);
}


/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller sets it to the number of entries in `entries`.
 *                   The callee sets it to the number of entries listed.
 *
 * @return 0 if no directory at the given path exists in the archive,
 *         a positive number for the number of entries listed,
 *         or a negative value for an error.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    tar_header_t header;
    size_t path_len = strlen(path);
    size_t count = 0;

    if (path == NULL || entries == NULL || no_entries == NULL) {
        fprintf(stderr, "Error: invalid arguments to list()\n");
        return -1;
    }

    if (!is_dir(tar_fd, path) && !is_symlink(tar_fd, path)) {
        return 0;
    }
    if (is_symlink(tar_fd,path)){
        char* new_path = get_symlink(tar_fd,path);
        if (new_path == NULL){
            return 0;
        }
        return list(tar_fd, new_path, entries, no_entries);
    }
    if (lseek(tar_fd, 0, SEEK_SET) == -1) {
        perror("lseek failed");
        return -1;
    }

    while (read(tar_fd, &header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (header.name[0] == '\0') {
            break;
        }

        if (strncmp(header.name, path, path_len) == 0) {

            const char *relative_path = header.name + path_len;

            if (strchr(relative_path, '/') == NULL ||
                strchr(relative_path, '/') == relative_path + strlen(relative_path) - 1) {

                if (count < *no_entries && strcmp(path, header.name) != 0) {
                    strncpy(entries[count], header.name, 100);
                    entries[count][99] = '\0';
                    count++;
                }
            }
        }

        off_t align_offset = aligned_size(header);
        if (lseek(tar_fd, align_offset, SEEK_CUR) == -1) {
            perror("lseek failed");
            return -3;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET) == -1) {
        perror("lseek failed");
        return -4;
    }

    *no_entries = count;
    printf("no entries in list %ld\n",*no_entries);

    return count;
}

//TODO handle 
char* get_symlink(int tar_fd, char *path){
    tar_header_t header;


    while (read(tar_fd, (void*)&header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (header.name[0] == '\0') {
            return NULL;
        }
        if (strcmp(header.name, path) == 0) {
            
            if (header.typeflag == SYMTYPE){
                if (lseek(tar_fd, 0, SEEK_SET) == -1) {
                    perror("lseek failed");
                    return NULL;
                }
                if (is_symlink(tar_fd, header.linkname)){
                    return get_symlink(tar_fd, header.linkname);
                }
                
            }
            return strdup(strcat(path,"/"));
        }

        if (lseek(tar_fd, aligned_size(header), SEEK_CUR) == -1) {
            perror("fseek failed");
            return NULL;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET)  == -1) {
        perror("fseek failed");
        return NULL;
    }
    return NULL;
}

/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    tar_header_t header;
    

    while (read(tar_fd, (void*)&header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (header.name[0] == '\0') {
            break;
        }

        if (strcmp(header.name, path) == 0) {
            
            if (header.typeflag == SYMTYPE){
                char *new_path = header.linkname;
                if (lseek(tar_fd, 0, SEEK_SET) == -1) {
                    perror("lseek failed");
                    return -3;
                }
                return read_file(tar_fd, new_path, offset, dest, len);
            }
            else if (header.typeflag != REGTYPE && header.typeflag != AREGTYPE) {
                return -1;
            }
            
            size_t file_size = strtol(header.size, NULL, 8);

            if (offset >= file_size){
                return -2;
            }

            if (lseek(tar_fd, offset, SEEK_CUR) == -1) {
                perror("fseek failed");
                return -3;
            }

            size_t data_len = file_size - offset;
            if (*len < data_len) {
                data_len = *len;
            }
            ssize_t bytes_read = read(tar_fd, dest, data_len);
            if (bytes_read == -1) {
                perror("read failed");
                return -3;
            }
            *len = bytes_read;
            return file_size - offset - bytes_read;
        }

        if (lseek(tar_fd, aligned_size(header), SEEK_CUR) == -1) {
            perror("fseek failed");
            return -3;
        }
    }
    if (lseek(tar_fd, 0, SEEK_SET)  == -1) {
        perror("fseek failed");
        return -3;
    }
    return -1;
}

