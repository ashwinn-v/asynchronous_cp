#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <liburing.h>

#define FILE_PATH "example.txt"
#define BUFFER_SIZE 1024
#define ASYNC_WRITE_BLOCK_SIZE 256 // Renamed to avoid name clash

int main() {
    struct io_uring ring;
    int fd, ret;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    char buffer[BUFFER_SIZE];
    off_t offset = 0;

    // Fill the buffer with some data to write
    memset(buffer, 'A', BUFFER_SIZE);

    // Open the file for writing
    fd = open(FILE_PATH, O_WRONLY | O_CREAT, 0644);
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

    // Split the write request into smaller parts
    for (int i = 0; i < BUFFER_SIZE; i += ASYNC_WRITE_BLOCK_SIZE) {
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            fprintf(stderr, "Could not get SQE.\n");
            io_uring_queue_exit(&ring);
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Write asynchronously
        io_uring_prep_write(sqe, fd, buffer + i, ASYNC_WRITE_BLOCK_SIZE, offset);
        offset += ASYNC_WRITE_BLOCK_SIZE;
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
    for (int i = 0; i < BUFFER_SIZE; i += ASYNC_WRITE_BLOCK_SIZE) {
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "io_uring_wait_cqe error.\n");
            break;
        }

        if (cqe->res < 0) {
            fprintf(stderr, "Async write failed.\n");
            break;
        }

        // Mark this request as processed
        io_uring_cqe_seen(&ring, cqe);
    }

    // Cleanup
    io_uring_queue_exit(&ring);
    close(fd);

    printf("Data written asynchronously.\n");
    return 0;
}
