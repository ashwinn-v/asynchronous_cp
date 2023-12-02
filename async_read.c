#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <liburing.h>

#define FILE_PATH "example.txt"
#define BUFFER_SIZE 1024
#define BLOCK_SIZE 256

int main() {
    struct io_uring ring;
    int fd, ret;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    char buffer[BUFFER_SIZE];
    off_t offset = 0;

    // Open the file for reading
    fd = open(FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Initialize io_uring
    if (io_uring_queue_init(32, &ring, 0) < 0) {
        perror("io_uring_queue_init failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Split the read request into smaller parts
    for (int i = 0; i < BUFFER_SIZE; i += BLOCK_SIZE) {
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            fprintf(stderr, "Could not get SQE.\n");
            io_uring_queue_exit(&ring);
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Read asynchronously
        io_uring_prep_read(sqe, fd, buffer + i, BLOCK_SIZE, offset);
        offset += BLOCK_SIZE;
        sqe->user_data = i;

        // Submit the request
        ret = io_uring_submit(&ring);
        if (ret < 0) {
            fprintf(stderr, "io_uring_submit error.\n");
            io_uring_queue_exit(&ring);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for completions
    for (int i = 0; i < BUFFER_SIZE; i += BLOCK_SIZE) {
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "io_uring_wait_cqe error.\n");
            break;
        }

        if (cqe->res < 0) {
            fprintf(stderr, "Async read failed.\n");
            break;
        }

        // Mark this request as processed
        io_uring_cqe_seen(&ring, cqe);
    }

    // Cleanup
    io_uring_queue_exit(&ring);
    close(fd);

    printf("Read data: %.*s\n", BUFFER_SIZE, buffer);
    return 0;
}
