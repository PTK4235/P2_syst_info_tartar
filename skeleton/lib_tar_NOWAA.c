#include "lib_tar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/**
 * Ce code est l'implémentation des fonctions de manipulation des archives TAR
 * 
 * Voici l'architecture globale:
 *  -Fonction d'aide: Ce sont les fonction auxilliaires pour le calcul du checksum
 *   la validation des headers.
 * 
 *  -Les fonction principales sont:
 *      
 *      -"check_archive" Vérifie la validité d'une archive TAR
 *      -"exists" Vérifie si un entrée DANS l'archive existe
 *      -"is_dir" && "is_file" && is_symlink vérifient si l'entrée est
 *          -répertoire
 *          -fichier
 *          -lien
 *      -"list" Liste les entrées du répertoire de l'archive 
 *      -"read_file" Lit un fichier en entrée
 */



// HELPER FUNCTIONS

// Vérifie si header du fichier tar est => fin d'archive
// On retourne bool si il est nul
static inline int is_null_header(const tar_header_t *header) {
    return header && memcmp(header, &(tar_header_t){0}, sizeof(tar_header_t)) == 0;
}

// Mesure la taille réelle d'un fichier de l'archive avec le padding
static size_t total_entry(const tar_header_t *header) {
    // Parse size from octal string, handling potential leading spaces
    size_t file_size = strtol(header->size, NULL, 8);
    return file_size + ((512 - (file_size % 512)) % 512);
}

// Calcul le checksum 
static unsigned int check_checksum(const tar_header_t *header) {
    unsigned int checksum = 0;
    const unsigned char *header_bytes = (const unsigned char *)header;

    for (size_t i = 0; i < sizeof(tar_header_t); i++) {
        if (i >= offsetof(tar_header_t, chksum) && i < offsetof(tar_header_t, chksum) + sizeof(header->chksum)) {
            checksum += ' ';
        } else {
            checksum += header_bytes[i];
        }
    }

    return checksum;
}

// On valide le checksum
static int is_valid_checksum(const tar_header_t *header) {
    unsigned int stored_checksum = strtol(header->chksum, NULL, 8);
    unsigned int calculated_checksum = check_checksum(header);
    
    return stored_checksum == calculated_checksum;
}

//Validation du magic et de la version
static int validate_magic_and_version(const tar_header_t *header) {
    if (strncmp(header->magic, TMAGIC, TMAGLEN) != 0) return 0;
    
    if (strncmp(header->version, TVERSION, TVERSLEN) != 0) return 0;
    
    return 1;
}

// Fonction auxilière importante !
// Il recherche dans l'archive avec la condition en argument
static int archive_search(int tar_fd, const char *path, int (*condition)(const tar_header_t*, const char*)) {
    lseek(tar_fd, 0, SEEK_SET);
    tar_header_t header;

    while (read(tar_fd, &header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (is_null_header(&header)) break;

        if (!validate_magic_and_version(&header) || !is_valid_checksum(&header)) break;

        if (strncmp(header.name, path, strlen(path)) == 0) {
            if (condition(&header, path)) return 1;
            return 0;
        }
        
        if (strncmp(header.version, TVERSION, TVERSLEN) != 0) {
            return -2;
        }


        lseek(tar_fd, total_entry(&header), SEEK_CUR);
    }

    return 0;
}

// Conditions pour différents types d'entrées
static int is_directory_cond(const tar_header_t *header, const char *path) {
    return header->typeflag == DIRTYPE;
}

static int is_file_cond(const tar_header_t *header, const char *path) {
    return header->typeflag == REGTYPE || header->typeflag == AREGTYPE;
}

static int is_symlink_cond(const tar_header_t *header, const char *path) {
    return header->typeflag == SYMTYPE;
}

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
    lseek(tar_fd, 0, SEEK_SET);
    tar_header_t header;
    int valid_headers = 0;

    while (read(tar_fd, &header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (is_null_header(&header)) break;

        if (!validate_magic_and_version(&header)) return -1;
        

        if (!is_valid_checksum(&header)) return -3;

        lseek(tar_fd, total_entry(&header), SEEK_CUR);
        valid_headers++;
    }

    return valid_headers;
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
    lseek(tar_fd, 0, SEEK_SET);
    tar_header_t header;

    while (read(tar_fd, &header, sizeof(tar_header_t)) == sizeof(tar_header_t)) {
        if (is_null_header(&header)) break;

        // Validate header
        if (!validate_magic_and_version(&header) || !is_valid_checksum(&header)) 
            break;

        if (strncmp(header.name, path, sizeof(header.name)) == 0) 
            return 1;

        // Move to next header
        lseek(tar_fd, total_entry(&header), SEEK_CUR);
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
    return archive_search(tar_fd, path, is_directory_cond);
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
    return archive_search(tar_fd, path, is_file_cond);
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
    return archive_search(tar_fd, path, is_symlink_cond);
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