#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

// Returns 0 on success, -1 on error
int c_fsync_by_name(const char *filename) {
    // Open in write-only/append mode so we don't destroy existing contents
    int fd = open(filename, O_WRONLY | O_APPEND);
    if (fd == -1) {
      printf("opening file named %s\n", filename);
        perror("Fortran interface open failed");
        return -1;
    }

    // Force OS kernel cache to flush to physical disk
    int result = fsync(fd);
    if (result == -1) {
        perror("Fortran interface fsync failed");
    }

    close(fd);
    return result;
}

