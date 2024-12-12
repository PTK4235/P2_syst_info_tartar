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

    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }


    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){
        valid_arch = valid_archive(header,nheader);
        if (valid_arch != 0){
            fclose(fd);
            return valid_arch;
        }
        nheader++;

        if (fseek(fd, aligned_size(header), SEEK_CUR) != 0) {
            perror("fseek failed");
            fclose(fd);
            return -3;
        }
    }
    fclose(fd);
    return 0;
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
    char full_path[256];
    int nheader = 0;
    int path_finder;
    
    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }

    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){

        path_finder = is_path(header, path, full_path,sizeof(full_path), nheader);
        if (path_finder != -1){
            fclose(fd);
            return path_finder;
        }
        nheader++;

        if (fseek(fd, aligned_size(header), SEEK_CUR) != 0) {
            perror("fseek failed");
            fclose(fd);
            return -3;
        }
    }
    fclose(fd);
    return 0;
}

/**
 * Checks if a given path matches the full path of the current tar header entry.
 *
 * @param header A tar_header_t structure representing a single header entry in the tar archive.
 * @param path The path to compare against the constructed full path.
 * @param full_path A buffer to store the constructed full path of the header entry.
 * @param nheader The index of the current header entry.
 *
 * @return The index of the header entry (`nheader`) if the paths match,
 *         0 if the `name` field of the header is empty,
 *         -1 if the paths do not match.
 */

int is_path(tar_header_t header,char *path, char *full_path, int size_full_path,int nheader){
    if (header.name[0] == '\0') {
        return 0;
    }

    if (header.prefix[0] != '\0') {
        snprintf(full_path, size_full_path, "%s/%s", header.prefix, header.name);
    }
    else {
        strncpy(full_path, header.name, size_full_path);
    }

    full_path[sizeof(full_path) - 1] = '\0';

    if (strcmp(full_path, path) == 0) {
        
        return nheader;
    }
    return -1;
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
    char full_path[256];
    int path_finder;

    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }
    int nheader = 0;




    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){
        path_finder = is_path(header,path,full_path,sizeof(full_path),nheader);
        if (path_finder == 0){
            fclose(fd);
            return 0;
        }
        else if (path_finder < 0){
            if (typeflag == REGTYPE && ((header.typeflag == REGTYPE) || (header.typeflag == AREGTYPE))){
                fclose(fd);
                return 1;
            }
            else if (header.typeflag == typeflag){
                fclose(fd);
                return 1;
            }
        }
        

        nheader++;

        if (fseek(fd, aligned_size(header), SEEK_CUR) != 0) {
            perror("fseek failed");
            fclose(fd);
            return -3;
        }
    }
    fclose(fd);
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
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    return 0;
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
    return 0;
}