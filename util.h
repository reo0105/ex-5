#ifndef UTIL
    #define UTIL


#define mem_alloc(ptr, type, size, name)                                                      \
do {                                                                                          \
    if ((ptr = (type *)malloc(sizeof(type) * (size))) == NULL) {                              \
        fprintf(stderr, "%s: Cannot allocate %ldbyte memory\n", name, sizeof(type) * size);   \
    }                                                                                         \
} while(0)


#define file_open(fd, fname, mode, permission)        \
do {                                                  \
    if ((fd = open(fname, mode, permission)) < 0) {   \
        perror("file_open()->open()");                \
    }                                                 \
} while(0)                                            \


#endif