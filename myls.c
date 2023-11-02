#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

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


/* ファイル名でソート */
void sort(char **data, int left, int right)
{
    int i, j;
    char tmp[STR_MAX], pivot[STR_MAX];
    if (left >= right) {
        return;
    }
    strcpy(pivot, data[left]);
    i = left, j = right;
    while (1) {
        while ((strcmp(data[i], pivot) < 0)) {
            i++;
        }
        while ((strcmp(data[j], pivot) > 0)) {
            j--;
        }
        if (i >= j) {
            break;
        }
        strcpy(tmp, data[i]);
        strcpy(data[i], data[j]);
        strcpy(data[j], tmp);
        i++; j--;
    }
    sort(data, left, i - 1);
    sort(data, j + 1, right);
}


/* ファイルの状態を出力 */
int create_stat(struct stat buf, char *data, int *err)
{
    int flag = 0;
    struct tm time;
    char hostname[NAME_MAX], stat[11], filename[STR_MAX];
    int len;

    strcpy(filename, data);

    switch (buf.st_mode & S_IFMT) {
        case S_IFBLK:  strcpy(stat, "b"); break;
        case S_IFCHR:  strcpy(stat, "c"); break;
        case S_IFDIR:  strcpy(stat, "d"); break;
        case S_IFIFO:  strcpy(stat, "p"); break;
        case S_IFLNK:  strcpy(stat, "l"); break;
        case S_IFREG:  strcpy(stat, "-"); break;
        case S_IFSOCK: strcpy(stat, "s"); break;
        default:       strcpy(stat, "?"); break;
    }
    flag = 1;
    switch (buf.st_mode & S_IRWXU) {
        case S_IRWXU:  strcat(stat, "rwx"); flag = 0; break;
        case S_IRWUSR: strcat(stat, "rw-"); flag = 0; break;
        case S_IRXUSR: strcat(stat, "r-x"); flag = 0; break;
        case S_IWXUSR: strcat(stat, "-wx"); flag = 0; break;
        case S_IRUSR:  strcat(stat, "r--"); flag = 0; break;
        case S_IWUSR:  strcat(stat, "-w-"); flag = 0; break;
        case S_IXUSR:  strcat(stat, "--x"); flag = 0; break;
    }
    if (flag) {
        strcat(stat, "---");
    }
    flag = 1;
    switch (buf.st_mode & S_IRWXG) {
        case S_IRWXG:  strcat(stat, "rwx"); flag = 0; break;
        case S_IRWGRP: strcat(stat, "rw-"); flag = 0; break;
        case S_IRXGRP: strcat(stat, "r-x"); flag = 0; break;
        case S_IWXGRP: strcat(stat, "-wx"); flag = 0; break;
        case S_IRGRP:  strcat(stat, "r--"); flag = 0; break;
        case S_IWGRP:  strcat(stat, "-w-"); flag = 0; break;
        case S_IXGRP:  strcat(stat, "--x"); flag = 0; break;
    }
    if (flag) {
        strcat(stat, "---");
    }
    flag = 1;
    switch (buf.st_mode & S_IRWXO) {
        case S_IRWXO:  strcat(stat, "rwx"); flag = 0; break;
        case S_IRWOTH: strcat(stat, "rw-"); flag = 0; break;
        case S_IRXOTH: strcat(stat, "r-x"); flag = 0; break;
        case S_IWXOTH: strcat(stat, "-wx"); flag = 0; break;
        case S_IROTH:  strcat(stat, "r--"); flag = 0; break;
        case S_IWOTH:  strcat(stat, "-w-"); flag = 0; break;
        case S_IXOTH:  strcat(stat, "--x"); flag = 0; break;
    }
    if (flag) {
        strcat(stat, "---");
    }
    time = *localtime(&buf.st_mtime);

    if (gethostname(hostname, NAME_MAX) == -1) {
        perror("myls.c->create_stat()->gethostname()");
        *err = errno;
        return -1;
    }

    //printf("%s\n", stat);

    len = snprintf(NULL, 0, "%s %3ld %s %9ld %2d/%2d %2d:%02d %s ", stat, buf.st_nlink, hostname, buf.st_size, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, filename);

    snprintf(data, len, "%s %3ld %s %9ld %2d/%2d %2d:%02d %s ", stat, buf.st_nlink, hostname, buf.st_size, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, filename);

    return 0;
}

int myls(char *path, char **data, int *err)
{
    DIR *dp;
    int i, count = 1, total = 0, len;
    char cur_path[STR_MAX];
    struct dirent *p;
    struct stat buf[NUM];

    memset(buf, 0, sizeof(struct stat));

    /* pathで指定されたものの属性を取得 */
    if (stat(path, &buf[0]) == -1) {
        perror("myls.c->myls()->stat()");
        *err = errno;
        return -1;
    }

    mem_alloc(data[0], char, STR_MAX, "list[0]");
    if (data[0] == NULL) {
        return -1;
    }

    //printf("mode = %d, %d\n", buf[0].st_mode& S_IFMT, S_IFDIR);

    /* pathがディレクトリでない場合 */
    if((buf[0].st_mode & S_IFMT) != S_IFDIR) {
        //printf("A\n");
        strcpy(data[0], path);
        create_stat(buf[0], data[0], err);
        return count;
    }

    /* 現在のディレクトリを保持 */
    if (getcwd(cur_path, PATH_MAX) == NULL) {
        perror("myls.c->myls()->getcwd()");
        *err = errno;
        return -1;
    }

    /* ディレクトリを変更 */
    if (chdir(path) == -1) {
        perror("myls.c->myls()->chidir()");
        *err = errno;
        return -1;
    }

    /* ディレクトリを開く */
    if ((dp = opendir(".")) == NULL) {
        perror("myls.c->myls()->opendir()");
        *err = errno;
        return -1;
    }

    // if ((data[0] = (char *)malloc(sizeof(STR_MAX))) == NULL) {
    //     fprintf(stderr, "Cannot allocate memory\n");
    //     closedir(dp);
    //     exit(EXIT_FAILURE);
    // }

    //printf("myls path = %s\n", path);
   
    /* ディレクトリストリームdpからディレクトリ項目の読み込み */
    errno = 0;
    while ((p = readdir(dp)) != NULL) {
        //printf("%s ", p->d_name);
        /* 隠しファイルでないならばDATAに追加する */
        if (p->d_name[0] != '.') {
            mem_alloc(data[count], char, STR_MAX, "list");
            if (data[count] == NULL) {
                closedir(dp);
                free_list(data, count);
                exit(EXIT_FAILURE);
            }
            //printf("count = %d\n", count);
            strcpy(data[count], p->d_name);
            count++;
        }
    }

    /* 読み込みエラー */
    if (errno != 0) {
        perror("myls.c->myls()->readdir()");
        closedir(dp);
        free_list(data, count);
        *err = errno;
        return -1;
    }

    // printf("\nbefore sort\n");
    // for (i = 0; i < count; i++) {
    //     printf("%d: %s\n", i, data[i]);
    // }

    /* ファイルの名前でソート */
    strcpy(data[0], "\0");
    sort(data, 0, count - 1);


    //  printf("\nafter sort\n");
    // for (i = 0; i < count; i++) {
    //     printf("%d: %s\n", i, data[i]);
    // }

    for (i = 1; i < count; i++) {
        /* 合計値の出力のためファイルの属性を取得する */
        //printf("%d: %s\n", i, data[i]);

        if (stat(data[i], &buf[i]) == -1) {
            perror("myls.c->myls()->stat()");
            closedir(dp);
            free_list(data, count);
            //printf("return\n");
            *err = errno;
            return -1;
        }
        //stat(data[i], &buf);
        total += buf[i].st_blocks;
    }
    //printf("合計 %d\n", total / 2);
    len = snprintf(NULL, 0, "total: %d ", total/2);
    snprintf(data[0], len, "total: %d ", total/2);
    //printf("%s\n", data[0]);
    //printf("ok\n");

    for (i = 1; i < count; i++) {
        /* ファイルの属性を取得する */
        // if (stat(data[i], &buf) == -1) {
        //     perror("stat");
        //     exit(EXIT_FAILURE);
        // }
        //stat(data[i], &buf);
        if (create_stat(buf[i], data[i], err) == -1) {
            closedir(dp);
            free_list(data, count);
            return -1;
        }
        //printf("%s\n", data[i]);
    }

    //printf("%d", errno);

    // if (errno != 0) {
    //     perror("readdir");
    //     exit(1);
    // }

    /* もとのディレクトリの戻る */
    if (chdir(cur_path) == -1) {
        //check_err = errno;
        perror("myls.c->myls()->chidir()");
        *err = errno;
        closedir(dp);
        free_list(data, count);
        return -1;
    }

    /* ディレクトリを閉じる */
    if (closedir(dp) != 0) {
        perror("myls.c->myls()->closedir()");
        free_list(data, count);
        *err = errno;
        return -1;
    }

    return count;
}