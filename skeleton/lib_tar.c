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
    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }
    int nheader = 0;


    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){
        if(header.magic[0] == '\0'){
            fclose(fd);
            return nheader;
        }
        else if (strncmp(header.magic,TMAGIC,TMAGLEN) != 0){
            fclose(fd);
            perror("magic value not valid");
            return -1;
        }
        else if (strncmp(header.version, TVERSION, TVERSLEN) != 0) {
            perror("version not valid");
            return -2;
        }
        else if (check_sum(header) == 0){
            fclose(fd);
            perror("checksum not valid");
            return -3;
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
 * This function calculates the checksum of the given TAR header and compares it
 * with the checksum value stored in the header. According to the TAR specification,
 * the checksum is calculated by summing all bytes of the header, treating the 
 * checksum field (bytes 148–155) as spaces (' ') during the calculation.
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
    if (sum == checksum){
        return 1;
    }
    return 0;
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
    
    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }

    if (check_archive(tar_fd) != 0){
        perror("check archive failed");
        fclose(fd);
        return -1;
    }

    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){
        if (header.name[0] == '\0') {
            fclose(fd);
            return 0;
        }

        if (header.prefix[0] != '\0') {
            snprintf(full_path, sizeof(full_path), "%s/%s", header.prefix, header.name);
        }
        else {
            strncpy(full_path, header.name, sizeof(full_path));
        }

        full_path[sizeof(full_path) - 1] = '\0';

        if (strcmp(full_path, path) == 0) {
            fclose(fd);
            return nheader;
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

    FILE* fd = fdopen(tar_fd,"rb");
    if (fd == NULL) {
        perror("fdopen failed");
        return -1;
    }
    int nheader = 0;

    if (exists(tar_fd,path) <=0){
        perror("not exist");
        fclose(fd);
        return 0;
    }


    while(fread(&header, sizeof(tar_header_t),1,fd) == 1){
        if (typeflag == REGTYPE && ((header.typeflag == REGTYPE) || (header.typeflag == AREGTYPE))){
            fclose(fd);
            return 1;
        }
        else if (header.typeflag == typeflag){
            fclose(fd);
            return 1;
        }
        

        nheader++;

        // Se déplacer vers le prochain en-tête
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