#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "myftp.h"
#include "myftpc.h"
#include "myls.h"
#include "util.h"


// 送信・受信ミス系はUNDEEFで全部EXITする
// クライアントはUNDEFを受け取ったら通信をやめる
void quit_proc(int argc, char **argv, int s);
void pwd_proc(int argc, char **argv, int s);
void cd_proc(int argc, char **argv, int s);
void dir_proc(int argc, char **argv, int s);
void lpwd_proc(int argc, char **argv, int s);
void lcd_proc(int argc, char **argv, int s);
void ldir_proc(int argc, char **argv, int s);
void get_proc(int argc, char **argv, int s);
void put_proc(int argc, char **argv, int s);
void help_proc(int argc, char **argv, int s);
void close_socket(int);

struct proctable ptab[] = {
    {"quit", quit_proc},
    {"pwd",  pwd_proc},
    {"cd",   cd_proc},
    {"dir",  dir_proc},
    {"lpwd", lpwd_proc},
    {"lcd",  lcd_proc},
    {"ldir", ldir_proc},
    {"get",  get_proc},
    {"put",  put_proc},
    {"help", help_proc},
    {NULL,   NULL}
};


/* ホスト名からipアドレスの取得とサーバとの接続 */
void get_ip_and_set_socket(char *hostname, int *s)
{
    struct addrinfo hints, *res;
    int err;
    struct sockaddr_in *a;
    char port[PORT_LEN];

    snprintf(port, PORT_LEN, "%d", SERVER_PORT);

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    if ((err = getaddrinfo(hostname, port, &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((*s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("myftpc.c->get_ip_and_set_socket()->socket()");
        exit(EXIT_FAILURE);
    }

    a = (struct sockaddr_in *)res->ai_addr;

    printf("%s\n", inet_ntoa(a->sin_addr));

    if (connect(*s, res->ai_addr, res->ai_addrlen) < 0) {
        perror("myftpc.c->get_ip_and_set_socket()->connect()");
        exit(EXIT_FAILURE);
    }
    
    return;
}


/* 引数をavにコピー */
void getargs(int *ac, char *av[], char *p)  
{
    int count = 0;
    
    /* argvにpをコピーしなきゃだめ */
    *ac = 0;

    while (1) {
        while (isblank(*p)) {
            p++;
        }
        
        if (*p == '\n') {
            return;
        }

        while (*p && !isblank(*p) && *p != '\n') {
            av[*ac][count++] = *p;
            p++;
        }

    
        av[(*ac)++][count] = '\0';

        if (*p == '\n') {
            return;
        }

        count = 0;
        
    }
}


/* argvの開放 */
void free_argv(char **argv)
{
    int i;

    for (i = 0; i < ARG_NUM; i++) {
        free(argv[i]);
    }

    return;
}


/* エラーの理由を文字列で返す */
void err_reason(int type, int code, char *err)
{
    switch(type) {
        case HEADER_TYPE_CMD_ERR:
            if (code == HEADER_CODE_CMD_SYNTAX_ERR) {
                strcpy(err, "err: Invalid syntax");
                break;
            } else if (code == HEADER_CODE_CMD_UNDEF_ERR) {
                strcpy(err, "err: Undefined command");
                break;
            } else if (code == HEADER_CODE_CMD_PROTOCOL_ERR) {
                strcpy(err, "err: Protocol err");
                break;
            }
            break;
        case HEADER_TYPE_FILE_ERR:
            if (code == HEADER_CODE_NO_FILE_ERR) {
                strcpy(err, "err: There is not such a file(or directory)");
                break;
            } else if (code == HEADER_CODE_NO_PERMISSION_ERR) {
                strcpy(err, "err: There are not permission");
                break;
            }
            break;
        case HEADER_TYPE_UNKWN_ERR:
            if (code == HEADER_CODE_UNDED_ERR) {
                strcpy(err, "err: Undefined err happened at server");
                break;
            }
    }

    return;
}

/* quitコマンド(myftpcの終了) */
void quit_proc(int argc, char **argv, int s)
{
    struct myftph *header;

    if ((header = (struct myftph *)malloc(sizeof(struct myftph))) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        free_argv(argv);
        close_socket(s);
        exit(EXIT_FAILURE);
    }

    if (argc != 1) {
        fprintf(stderr, "Argument num must be 1\n");
    }

    // memset(header, 0, sizeof(struct myftph));
    // header->type = HEADER_TYPE_QUIT;

    send_reply(s, NULL, HEADER_TYPE_QUIT, 0, 0, NULL);

    recive_header(s, header);

    if (header->type != HEADER_TYPE_CMD_OK) {
        fprintf(stderr, "Recived header type field is invalid\n");

    } else if (header->code != HEADER_CODE_CMD_OK) {
        fprintf(stderr, "Recived header code field is invalid\n");

    } else if (header->length != 0) {
        fprintf(stderr, "Recived header length field is invalid\n");

    } else {
        close_socket(s);
        printf("Quit done\n");
        free_argv(argv);
        close_socket(s);
        free(header);
        exit(EXIT_SUCCESS);
    }

    free(header);

    return;
}


/* pwdコマンド(サーバでのカレントディレクトリの表示) */
void pwd_proc(int argc, char **argv, int s)
{
    struct myftph *header;
    char data[DATASIZE];

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        free_argv(argv);
        close(s);
        exit(EXIT_FAILURE);
    }

    if (argc != 1) {
        fprintf(stderr, "Argument num must be 1\n");
    }

    send_reply(s, NULL, HEADER_TYPE_PWD, 0, 0, NULL);

    recive_header(s, header);

    if (header->type != HEADER_TYPE_CMD_OK) {
        fprintf(stderr, "Recived header type field is invalid\n");

    } else if (header->code != HEADER_CODE_CMD_OK) {
        fprintf(stderr, "Recived header code field is invalid\n");

    } else if (header->length == 0) {
        fprintf(stderr, "Recived header length field is invalid\n");

    } else {
        recv_data(s, header, ntohs(header->length), data);
        printf("%s\n", data);
    }

    free(header);

    return;
}

/* cdコマンド(サーバでのカレントディレクトリの移動) */
void cd_proc(int argc, char **argv, int s)
{
    struct myftph *header;
    char *str_err;

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        free_argv(argv);
        close(s);
        exit(EXIT_FAILURE);
    }

    if (argc == 1) {
        fprintf(stderr, "Please input a path\n");

    } else if (argc != 2) {
        fprintf(stderr, "Argument num must be 2\n");
    }

    memset(header, 0, sizeof(struct myftph));
    // header->type = HEADER_TYPE_QUIT;

    send_reply(s, NULL, HEADER_TYPE_CWD, 0, strlen(argv[1]), argv[1]);

    recive_header(s, header);

    if (header->type != HEADER_TYPE_CMD_OK) {
        mem_alloc(str_err, char, ERR_LEN, "str_err");
        if (str_err == NULL) {
            free(header);
            free_argv(argv);
            close(s);
            exit(EXIT_FAILURE);
        }

        err_reason(header->type, header->code, str_err);
        fprintf(stderr, "Cannot change directory. %s\n", str_err);
        free(str_err);

    } else if (header->code != HEADER_CODE_CMD_OK) {
        fprintf(stderr, "Recived header code field is invalid\n");

    } else if (header->length != 0) {
        fprintf(stderr, "Recived header length field is invalid\n");

    }

    free(header);

    return;
}


/* dirコマンド(サーバに存在するファイル情報の取得) */
void dir_proc(int argc, char **argv, int s)
{
    struct myftph *header;
    char data[DATASIZE], *str_err;

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        close(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    }

    if (argc > 2) {
        fprintf(stderr, "Too many argument\n");
    }

    memset(header, 0, sizeof(struct myftph));

    send_reply(s, NULL, HEADER_TYPE_LIST, 0, (argc == 1) ? 0 : strlen(argv[1]), (argc == 1) ? NULL : argv[1]);

    /* リプライメッセージ */
    recive_header(s, header);

    if (header->type != HEADER_TYPE_CMD_OK) {
        mem_alloc(str_err, char, ERR_LEN, "str_err");
        if (str_err == NULL) {
            free(header);
            free_argv(argv);
            close(s);
            exit(EXIT_FAILURE);
        }

        err_reason(header->type, header->code, str_err);
        fprintf(stderr, "reply: Cannot display files. %s\n", str_err);
        free(str_err);
        return;

    } else if (header->code != HEADER_CODE_CMD_OK_FROM_SERVER) {
        fprintf(stderr, "reply: Recived header code field is invalid\n");
        return;

    } else if (header->length != 0) {
        fprintf(stderr, "reply: Recived header length field is invalid\n");
        return;

    }

    do {
        recive_header(s, header);

        //printf("type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

        if (header->type != HEADER_TYPE_DATA) {
            //printf("%d %d\n", header->type, header->code);
            mem_alloc(str_err, char, ERR_LEN, "str_err");
            if (str_err == NULL) {
                free(header);
                free_argv(argv);
                close(s);
                exit(EXIT_FAILURE);
            }

            err_reason(header->type, header->code, str_err);
            fprintf(stderr, "data: Cannot display files. %s\n", str_err);
            free(str_err);
            break;

        } else if (header->code != HEADER_CODE_NO_MORE_DATA && header->code != HEADER_CODE_FOLLOWED_DATA) {
            fprintf(stderr, "data: Recived header code field is invalid\n");

        } else if (header->length == 0) {
            fprintf(stderr, "data: Recived header length field is invalid\n");

        }

        
        recv_data(s, header, ntohs(header->length), data);
        printf("%s\n", data);

    } while(header->code == 0x01);

    return;
}


void lpwd_proc(int argc, char **argv, int s)
{
    char cur_path[PATH_MAX];

    if (argc != 1) {
        fprintf(stderr, "Argument num must be 1\n");
    }

    if (getcwd(cur_path, PATH_MAX) == NULL) {
        perror("myftpc.c->lpwd_proc()->getcwd()");
        close_socket(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    }

    printf("%s\n", cur_path);

    return;
}


void lcd_proc(int argc, char **argv, int s)
{
    if (argc == 1) {
        fprintf(stderr, "Please input a path\n");
        return;

    } else if (argc != 2) {
        fprintf(stderr, "Argumrnt num must be 2\n");
    }

    if (chdir(argv[1]) == -1) {
        perror("myftpc.c->lcd_proc()->chidir()");
        close_socket(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    }

    return;
}


void ldir_proc(int argc, char **argv, int s)
{
    int err, i, count;
    char cur_path[PATH_MAX];
    char *list[NUM];

    if (argc == 1) {
        if (getcwd(cur_path, PATH_MAX) == NULL) {
            perror("myftpc.c->ldir_proc()->getcwd()");
            close_socket(s);
            free_argv(argv);
            exit(EXIT_FAILURE);
        }

    /* lengthが0でない場合(データを受信) */
    } else if (argc == 2){
        strcpy(cur_path, argv[1]);

    } else {
        strcpy(cur_path, argv[1]);
        fprintf(stderr, "Argumrnt num must be 1 or 2\n");
    }

    /* ファイルの情報取得 */
    if ((count = myls(cur_path, list, &err)) == -1) {
        /* エラーの場合 */
        close_socket(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    } else {
        /* それ以外の場合 */
        for (i = 0; i < count; i++) {
            printf("%s\n", list[i]);
        }

        free_list(list, count);
    }

    return;
}


void get_proc(int argc, char **argv, int s)
{
    char path1[PATH_MAX], path2[PATH_MAX], *str_err;
    struct myftph *header;
    int fd;

    if (argc == 1) {
        fprintf(stderr, "Please input a path\n");
        return;

    } else if (argc == 2) {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[1]);

    } else if (argc == 3) {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[2]);
    } else {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[2]);
        fprintf(stderr, "Arguments must be no more than 3\n");
    }

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        close(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    }

    send_reply(s, header, HEADER_TYPE_RETR, 0, strlen(path1), path1);

    /* リプライメッセージ */
    recive_header(s, header);
    //printf("type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

    if (header->type != HEADER_TYPE_CMD_OK) {
        mem_alloc(str_err, char, ERR_LEN, "str_err");
        if (str_err == NULL) {
            free(header);
            free_argv(argv);
            close(s);
            exit(EXIT_FAILURE);
        }

        //printf("type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

        err_reason(header->type, header->code, str_err);
        fprintf(stderr, "reply: Cannot display files. %s\n", str_err);
        free(str_err);
        return;

    } else if (header->code != HEADER_CODE_CMD_OK_FROM_SERVER) {
        fprintf(stderr, "reply: Recived header code field is invalid\n");
        return;

    } else if (header->length != 0) {
        fprintf(stderr, "reply: Recived header length field is invalid\n");
        return;
    }

    file_open(fd, path2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        switch(errno) {
            /* fileが存在しない場合 */
            case ENOENT:  
                fprintf(stderr, "No such a file\n");
                break;
            /* Permissionがない場合 */
            case EACCES: 
                fprintf(stderr, "Permission denied\n");
                break;
            /* それ以外のエラーである場合 */
            default: 
                fprintf(stderr, "Undefined err happened\n");
                break;
        }
        close(s);
        free(header);
        free_argv(argv);
        exit(EXIT_FAILURE);
    } else {
        recv_message_content(s, header, fd);
    }
}



void put_proc(int argc, char **argv, int s)
{
    char path1[PATH_MAX], path2[PATH_MAX], *str_err;
    struct myftph *header;
    int fd;

    if (argc == 1) {
        fprintf(stderr, "Please input a path\n");
        return;

    } else if (argc == 2) {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[1]);

    } else if (argc == 3) {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[2]);
    } else {
        strcpy(path1, argv[1]);
        strcpy(path2, argv[2]);
        fprintf(stderr, "Arguments must be no more than 3\n");
    }

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        close(s);
        free_argv(argv);
        exit(EXIT_FAILURE);
    }

    send_reply(s, header, HEADER_TYPE_STOR, 0, strlen(path2), path2);

    /* リプライメッセージ */
    recive_header(s, header);

    if (header->type != HEADER_TYPE_CMD_OK) {
        mem_alloc(str_err, char, ERR_LEN, "str_err");
        if (str_err == NULL) {
            free(header);
            free_argv(argv);
            close(s);
            exit(EXIT_FAILURE);
        }

        //printf("type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

        err_reason(header->type, header->code, str_err);
        fprintf(stderr, "reply: Cannot display files. %s\n", str_err);
        free(str_err);
        return;

    } else if (header->code != HEADER_CODE_CMD_OK_FROM_CLIENT) {
        fprintf(stderr, "reply: Recived header code field is invalid\n");
        return;

    } else if (header->length != 0) {
        fprintf(stderr, "reply: Recived header length field is invalid\n");
        return;
    }

    file_open(fd, path1, O_RDONLY, 0644);
    if (fd < 0) {
        switch(errno) {
            /* fileが存在しない場合 */
            case ENOENT:  
                break;
            /* Permissionがない場合 */
            case EACCES: 
                break;
            /* それ以外のエラーである場合 */
            default: 
                fprintf(stderr, "Undefined err happened\n");
                break;
        }
        close(s);
        //free(header);
        free_argv(argv);
        exit(EXIT_FAILURE);
    } else {
        printf("send file conetent\n");
        send_message_content(s, header, fd);
    }
}


void help_proc(int argc, char **argv, int s)
{
    (void) argc;
    (void) argv;
    (void) s;

    printf("            -- command list --\n"
           "quit              : Finish myftp\n"
           "pwd               : Get current directory at server\n"
           "cd path1          : Move to path1 at server\n"
           "dir [path1]       : Retrieve file lists under path1 at server. If you don't specify arguments, you get under current directory files\n"
           "lpwd              : Get current directory at client\n"
           "lcd path1         : Move to path1 at client\n"
           "ldir [path1]      : Retrieve file lists under path1 at client. If you don't specify arguments, you get under current directory files\n"
           "get path1 [path2] : Transfer a file of path1 from server to client. File name at client is path2. If you dont't specify path2, file name is path1\n"
           "put path1 [path2] : Transfer a file of path1 from client to server. File name at server is path2. If you dont't specify path2, file name is path1\n"
           "help              : You can get this help message\n");
}

/* ソケットのクローズ */
void close_socket(int s)
{
    if (close(s) < 0) {
        perror("myftpc.c->close_socket()->close()");
        exit(EXIT_FAILURE);
    }

    return;
}



int main(int argc, char *argv[])
{
    int i, s, ac = 0;
    char cmd[CMD_LEN];
    char *av[ARG_NUM];
    struct proctable *pt;

    /* ホスト名 */
    if (argc == 1) {
        fprintf(stderr, "Please input a host name\n");
        exit(EXIT_FAILURE);
    } else if (argc == 2) {
        get_ip_and_set_socket(argv[1], &s);
    } else {
        fprintf(stderr, "Please input an init directory\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < ARG_NUM; i++) {
        if ((av[i] = (char *)malloc(sizeof(char) * PATH_MAX)) == NULL) {
            fprintf(stderr, "Cannot allocate memory\n");
            close_socket(s);
            exit(EXIT_FAILURE);
        }
    }

        // free_argv(av);
        // return 0;

    
    /* ソケットの作成 */
    //set_socket(&s);

    /* シグナルの設定 */
    // signal_set();

    // /* コネクション要求受信準備 */
    // if (listen(s, PACKET_WAIT_MATRIX) < 0) {
    //     perror("listen");
    //     close_socket(s);
    //     exit(EXIT_FAILURE);
    // }

    while(1) {
        printf("myFTP%% ");

        // free_argv(av);
        // return 0;

        fgets(cmd, CMD_LEN, stdin);

        // free_argv(av);
        // return 0;

        getargs(&ac, av, cmd); // argcとargvの設定

        if (ac == 1) {
            av[1][0] = av[2][0] = '\0';
        } else if (ac == 2) {
            av[2][0] = '\0';
        }

        //printf("cmd = %s, path1 = %s, path2 = %s\n", av[0], av[1], av[2]);

        
        
        /* コマンドテーブルを探索 */
        for (pt = ptab; pt->cmd; pt++) {
            //printf("pt = %s, av = %s\n", pt->cmd, av[0]);
            if (strcmp(pt->cmd, av[0]) == 0) {
                (*pt->func)(ac, av, s);
                break;
            }
        }

        if (pt->cmd == NULL) {
            fprintf(stderr, "Undefined command\n");
            exit(EXIT_FAILURE);
        }
        

    }


}