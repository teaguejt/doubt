#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define USAGE(x) \
    (printf("%s %s\n", argv[0], x));

#define BLOCK_SIZE ((int)4096)

const uint16_t VERSION_MINOR = 1;
const uint16_t VERSION_MAJOR = 0;

const char *magic = "DoUbTjTt";

struct data_block {
    char d[BLOCK_SIZE];
};

struct ptr_block {
    uint64_t p[BLOCK_SIZE / sizeof(uint64_t)];
};

struct dnode {
    char name[32];
    uint16_t stripe;
    uint64_t size;
    uint64_t dbs[8];        // Direct data blocks
    uint64_t pbs[16];       // Pointer blocks
    uint64_t bbs[2];        // Double pointer blocks
};

struct dcont {
    uint64_t doubt_magic;   // Magic number
    uint64_t num_dnodes;    // Number of dnodes in container
    uint16_t v_min;         // Minor version
    uint16_t m_maj;         // Major version
    uint32_t bm_offset;     // Bitmask offset
    uint32_t dn_offset;     // dnode offset
    uint32_t dt_offset;     // Data offset
};

void usage(char **argv) {
    printf("usage:\n");
    USAGE("c container_file size - create container with size");
    USAGE("d container_file - securely delete container");
    USAGE("a container_file file_name - add file to container");
    USAGE("g container_file file_number - get file from doubt");
    USAGE("b <container_file> - diagnostic and system information");
    USAGE("h - display this printout\n");
}

void diagnostics(char *container) {
    printf("DOUBT DIAGNOSTICS\n");
    printf("=================\n");
    printf("size of dcont:  %d\n", (int)sizeof(struct dcont));
    printf("size of dnode:  %d\n", (int)sizeof(struct dnode));
    printf("size of dblock: %d\n", (int)sizeof(struct data_block));
    printf("size of pblock: %d\n", (int)sizeof(struct ptr_block));

    if(container) {
    }
}

int create_container(char *name, int size) {
    unsigned long tsize;
    unsigned long dsize;
    unsigned long i;
    int fd;
    int perm;
    uint8_t zero = 0;
    uint32_t mask_offset;
    uint32_t node_offset;
    uint32_t data_offset;
    uint64_t num_nodes = size;
    int size_dnodes = size * sizeof(struct dnode);
    int size_mask   = size / 8;
    char *buf;

    /* Calculate sizes for the container */
    dsize = size;
    dsize *= BLOCK_SIZE;

    if(size % 8 != 0) {
        printf("doubt: size must be a multiple of eight because reasons.\n");
        return -1;
    }

    tsize = size_dnodes + size_mask + sizeof(struct dcont) + dsize;

    mask_offset = sizeof(struct dcont);
    node_offset = mask_offset + size_mask;
    data_offset = node_offset + size_dnodes;
    
    /* Informational output */
    printf("doubt: creating container %s with %d data blocks:\n", name, size);
    printf("       %d bytes for the container\n", (int)sizeof(struct dcont));
    printf("       %d bytes for the dnodes\n", size_dnodes);
    printf("       %d bytes for the bitmask\n", size_mask);
    printf("       %lu bytes for data\n", dsize);
    printf("       %lu bytes total\n", tsize);
    printf("       %" PRIu32 " (0x%" PRIx32 ") mask offset\n", mask_offset,
            mask_offset);
    printf("       %" PRIu32 " (0x%" PRIx32 ") node offset\n", node_offset,
            node_offset);
    printf("       %" PRIu32 " (0x%" PRIx32 ") data offset\n", data_offset,
            data_offset);

    /* Create the container, starting with the "dcont" struct */
    perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    fd = open(name, O_RDWR | O_CREAT, perm);
    if(fd < 0) {
        printf("doubt: error creating container (%d)\n", errno);
        return -2;
    }

    for(i = 0; i < 8; i++) {
        if(write(fd, &magic[i], 1) != 1) {
            printf("doubt: write error while writing magic number\n");
            return -3;
        }
    }

    if(write(fd, &num_nodes, 8) != 8) {
        printf("doubt: write error while writing node amount\n");
        return -4;
    }

    if(write(fd, &VERSION_MINOR, 2) != 2) {
        printf("doubt: write error while writing version\n");
        return -5;
    }

    if(write(fd, &VERSION_MAJOR, 2) != 2) {
        printf("doubt: write error while writing version\n");
        return -6;
    }

    if(write(fd, &mask_offset, 4) != 4) {
        printf("doubt: write error while writing mask offset\n");
        return -7;
    }

    if(write(fd, &node_offset, 4) != 4) {
        printf("doubt: write error while writing node offset\n");
        return -8;
    }

    if(write(fd, &data_offset, 4) != 4) {
        printf("doubt: write error while writing data offset\n");
        return -9;
    }

    /* Now write the bitmask, dnodes, and blocks -- all zeros */
    /* There's GOT to be a better way to do this... */
    buf = malloc(sizeof(char) * size_mask);
    if(!buf) {
        printf("doubt: malloc error.\n");
        return -13;
    }
    for(i = 0; i < size_mask; i++) {
        buf[i] = 0;
    }
    if(write(fd, &buf[0], size_mask) != size_mask) {
        printf("doubt: write error while writing bitmask\n");
        return -10;
    }

    free(buf);
    buf = malloc(sizeof(char) * size_dnodes);
    if(!buf) {
        printf("doubt: malloc error.\n");
        return -14;
    }
    for(i = 0; i < size_dnodes; i++) {
        buf[i] = 0;
    }
    if(write(fd, &buf[0], size_dnodes) != size_dnodes) {
        printf("doubt: write error while writing dnodes\n");
        return -11;
    }

    free(buf);
    buf = malloc(sizeof(char) * BLOCK_SIZE);
    for(i = 0; i < BLOCK_SIZE; i++) {
        buf[i] = 0;
    }
    for(i = 0; i < size; i++) {
        if(write(fd, &buf[0], BLOCK_SIZE) != BLOCK_SIZE) {
            printf("doubt: write error while writing data blocks\n");
            return -12;
        }
    }
    free(buf);
    close(fd);

    return 0;
}

int main(int argc, char **argv) {
    int size;

    if(argc < 2 || argc > 4 || argv[1][0] == 'h') {
        usage(argv);
        return 0;
    }

    if(argv[1][0] == 'b') {
        if(argc == 3) {
            diagnostics(argv[2]);
        }
        else if(argc == 2) {
            diagnostics(NULL);
        }
        else {
            printf("doubt: too many arguments for flag 'b'\n");
            usage(argv);
        }
    }

    if(argv[1][0] == 'c') {
        size = atoi(argv[3]);
        if(create_container(argv[2], size)) {
            printf("doubt: error creating container.\n");
            return -1;
        }

        printf("doubt: container created. Please verify its attributes.\n");
    }

    return 0;
}
