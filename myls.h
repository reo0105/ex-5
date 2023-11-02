#ifndef MYLS
    #define MYLS
#include <sys/stat.h>

#define STR_MAX 128
#define NUM 100
#define S_IRWUSR S_IRUSR | S_IWUSR
#define S_IRXUSR S_IRUSR | S_IXUSR
#define S_IWXUSR S_IWUSR | S_IXUSR
#define S_IRWGRP S_IRGRP | S_IWGRP
#define S_IRXGRP S_IRGRP | S_IXGRP
#define S_IWXGRP S_IWGRP | S_IXGRP
#define S_IRWOTH S_IROTH | S_IWOTH
#define S_IRXOTH S_IROTH | S_IXOTH
#define S_IWXOTH S_IWOTH | S_IXOTH


void free_list(char **, int);
void sort(char **, int, int);
int create_stat(struct stat, char *, int *);
int myls(char *, char **, int *);


#endif