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
#include <sys/wait.h>

#include "myftp.h"
#include "myftpd.h"
#include "util.h"
#include "myls.h"


/* コマンドの構造体 */ 
struct proctable ptab[] = {
    {HEADER_TYPE_QUIT, recv_quit},
    {HEADER_TYPE_PWD,  recv_pwd},
    {HEADER_TYPE_CWD,  recv_cwd},
    {HEADER_TYPE_LIST, recv_list},
    {HEADER_TYPE_RETR, recv_retr},
    {HEADER_TYPE_STOR, recv_stor},
    {0,                NULL}
};


/* ソケットの設定 */
void set_socket(int *s)
{
    int soval;
    struct sockaddr_in myskt;
    in_port_t myport = SERVER_PORT;

    if ((*s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("myftpcd.c->set_socket()->socket()");
        exit(EXIT_FAILURE);
    }

    /* アドレス再利用の設定 */ // 入れるほうがいいらしい
    soval = 1;
    if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &soval, sizeof(soval)) == -1) {
        perror("myftpcd.c->set_socket()->setsockopt()");
        close_socket(*s);
        exit(EXIT_FAILURE);
    }

    memset(&myskt, 0, sizeof myskt);
    myskt.sin_family = AF_INET;
    myskt.sin_port = htons(myport);
    myskt.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(*s, (struct sockaddr *)&myskt, sizeof myskt) < 0) {
        perror("myftpcd.c->set_socket()->bind()");
        close_socket(*s);
        exit(EXIT_FAILURE);
    }

    return;
}


/* ソケットのクローズ */
void close_socket(int s)
{
    if (close(s) < 0) {
        perror("myftpcd.c->close_socket()->close()");
        exit(EXIT_FAILURE);
    }

    return;
}


/*sig_childハンドラ*/
void sig_child_handler(int sig)             
{
    (void) sig;

    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) != 0) {
        if (pid == -1) {
            if (errno == ECHILD) {
                break;
            } else if (errno == EINTR) {
                continue;
            }
            perror("myftpcd.c->sig_child_handler()->waitpid()");
            break;
        }
    }
}


/*シグナルのセット*/
void signal_set()
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);

    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigact, NULL);

    sigact.sa_handler = sig_child_handler;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGTTOU, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigact, NULL);
}


/*シグナルのリセット*/
void reset_signal_handlers()
{
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));

    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = 0;

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTTOU, &sigact, NULL);
    sigaction(SIGCHLD, &sigact, NULL);
    sigaction(SIGTSTP, &sigact, NULL);
}


/* QUITを受信したときの処理 */
void recv_quit(int acc_s, struct myftph *header, char *data, int cli_num)
{
    (void) data;
    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* lengthが0でない場合 */
    } else if (ntohs(header->length) != 0) {
        fprintf(stderr, "Header length field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* 正しい場合 */
    } else {
        printf("client %d: correct quit\n", cli_num);
        send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK, 0, NULL);
        close(acc_s);
        free(header);
        exit(EXIT_SUCCESS);
    }

    return;
}


/* PWDを受信したときの処理 */
void recv_pwd(int acc_s, struct myftph *header, char *data, int cli_num)
{
    char cur_path[PATH_MAX];

    (void) data;
    (void) cli_num;

    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* lengthが0でない場合 */
    } else if (ntohs(header->length) != 0) {
        fprintf(stderr, "Header length field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* 正しい場合 */
    } else {
        if (getcwd(cur_path, PATH_MAX) == NULL) {
            perror("myftpcd.c->recv_pwd()->getcwd()");
            close_socket(acc_s);
            free(header);
            exit(EXIT_FAILURE);
        }
        /* パス名の最後には改行や\0を含まない */
        send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK, strlen(cur_path), cur_path); 
    }

    return;
}


/* CWDを受信したときの処理 */
void recv_cwd(int acc_s, struct myftph *header, char *data, int cli_num)
{
    int data_len, err = 0;

    (void) cli_num;

    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* lengthが0である場合 */
    } else if (ntohs(header->length) == 0) {
        fprintf(stderr, "Header length field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);

    /* 正しい場合 */
    } else {
        data_len = ntohs(header->length);
        
        /* データ本体を受信 */
        recv_data(acc_s, header, data_len, data);

        /*　ディレクトリの変更 */
        if (chdir(data) == -1) {
            err = errno;
            perror("myftpd.c->recv_cwd()->chdir()");
            fprintf(stderr, "Cannot change directory: %s\n", data);
        }

        switch(err) {
            /* OKの場合 */
            case 0:
                send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK, 0, NULL);
                break;
            /* Directoryが存在しない場合 */
            case ENOENT:  
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL); 
                break;
            /* Permissionがない場合 */
            case EACCES: 
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_PERMISSION_ERR, 0, NULL); 
                break;
            /* Directoryでなかった場合 */
            case ENOTDIR:
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL); 
                break;
            /* それ以外のエラーである場合 */
            default:
                fprintf(stderr, "Undefined err happened\n");
                send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL); 
                break;
        } 
    }

    return;
}


/* LISTを受信したときの処理 */
void recv_list(int acc_s, struct myftph *header, char *data, int cli_num)
{
    int path_len, err, count;
    char path[PATH_MAX];
    char *list[NUM];

    (void) data;
    (void) cli_num;

    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);
        return;

    /* lengthが0の場合（カレントディレクトリを取得） */
    } else if (header->length == 0) {
        if (getcwd(path, PATH_MAX) == NULL) {
            perror("myftpd.c->recv_list()->getcwd()");
            close_socket(acc_s);
            free(header);
            exit(EXIT_FAILURE);
        }

    /* lengthが0でない場合(データを受信) */
    } else {
        path_len = ntohs(header->length);
        recv_data(acc_s, header, path_len, path);
    }

    /* ファイルの情報取得 */
    if ((count = myls(path, list, &err)) == -1) {
        /* エラーの場合 */
        switch(err) {
            /* ディレクトリがない場合 */
            case ENOENT:
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL);
                break;
            /* パーミションがない場合 */
            case EACCES:
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_PERMISSION_ERR, 0, NULL);
                break;
            /* ディレクトリではない場合 */
            case ENOTDIR: 
                send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL);
                break;
            default:
                send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
                break;
        }
    } else {
        /* それ以外の場合 */
        /* リプライメッセージ */
        send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK_FROM_SERVER, 0, NULL);
        /* データメッセージ */
        send_message_list(acc_s, header, list, count);

        free_list(list, count);
    }


    return;
}


/* RETRを受信したときの処理 */
void recv_retr(int acc_s, struct myftph *header, char *data, int cli_num)
{
    int path_len, fd;

    (void) cli_num;

    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);
        return;

    /* lengthが0の場合 */
    } else if (header->length == 0) {
        fprintf(stderr, "Header length field must not be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);
        return;

    /* lengthが0でない場合(データを受信) */
    } else {
        path_len = ntohs(header->length);
        /* クライアントがget path1 path2でpath1だけを送信しそれをdataに保持 */
        recv_data(acc_s, header, path_len, data);
        
        /* ファイルを開く */
        file_open(fd, data, O_RDONLY, 0644);

        /* ファイルが開けなかった場合 */
        if (fd < 0) {
            switch(errno) {
                /* fileが存在しない場合 */
                case ENOENT:  
                    send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL); 
                    break;
                /* Permissionがない場合 */
                case EACCES: 
                    fprintf(stderr, "Permission denied\n");
                    send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_PERMISSION_ERR, 0, NULL); 
                    break;
                /* それ以外のエラーである場合 */
                default: 
                    send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL); 
                    break;
            }
        
        /* ファイルが開けた場合 */
        } else {
            /* リプライメッセージ */
            send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK_FROM_SERVER, 0, NULL);
            /* メッセージ内容を送信 */
            send_message_content(acc_s, header, fd);
            close(fd);
        }

    }

    return;
}


/* STORを受信したときの処理 */
void recv_stor(int acc_s, struct myftph *header, char *data, int cli_num)
{
    int path_len, fd;

    (void) cli_num;

    /* codeが0でない場合 */
    if (header->code != 0) {
        fprintf(stderr, "Header code field must be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);
        return;

    /* lengthが0の場合 */
    } else if (header->length == 0) {
        fprintf(stderr, "Header length field must not be 0\n");
        send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_CMD_SYNTAX_ERR, 0, NULL);
        return;

    /* lengthが0でない場合(データを受信) */
    } else {
        path_len = ntohs(header->length);
        /* クライアントがput path1 path2の場合path1の内容とpath2の名前（最初に）を送信
           クライアントがput path1      の場合path1の内容とpath1の名前（最初に）を送信 */
        recv_data(acc_s, header, path_len, data);

        //printf("file = %s/n", data);
        
        /* ファイルを開く ファイルが存在していても上書きする*/
        file_open(fd, data, O_WRONLY | O_CREAT, 0644);

        /* ファイルが開けなかった場合 */
        if (fd < 0) {
            switch(errno) {
                /* fileが存在しない場合 */
                case ENOENT:  
                    fprintf(stderr, "No such a file\n");
                    send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_FILE_ERR, 0, NULL); 
                    break;
                /* Permissionがない場合 */
                case EACCES: 
                    fprintf(stderr, "Permission denied\n");
                    send_reply(acc_s, header, HEADER_TYPE_FILE_ERR, HEADER_CODE_NO_PERMISSION_ERR, 0, NULL); 
                    break;
                /* それ以外のエラーである場合 */
                default: 
                    send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL); 
                    break;
            }
            close(fd);
        
        /* ファイルが開けた場合 */
        } else {
            /* リプライメッセージ */
            send_reply(acc_s, header, HEADER_TYPE_CMD_OK, HEADER_CODE_CMD_OK_FROM_CLIENT, 0, NULL);
            /* メッセージ内容を送信 */
            recv_message_content(acc_s, header, fd);
        }

    }

    return;
}


/* クライアントとの通信 */
void communicate_client(int acc_s, int cli_num)
{
    struct myftph *header;
    struct proctable *pt;
    char data[DATASIZE+1]; // '\0'文字分を確保

    if ((header = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        exit(EXIT_FAILURE);
    }

    while(1) {
        /* 4バイトのヘッダーを受信 */
        recive_header(acc_s, header);

        printf("client %d: recived header\n", cli_num);

        //printf("type = %d ,code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

        /* コマンドテーブルを探索 */
        for (pt = ptab; pt->type; pt++) {
            //printf("type = %d\n", pt->type);
            if (pt->type == header->type) {
                (*pt->func)(acc_s, header, data, cli_num);
                break;
            }
        }

        if (pt->type == 0) {
            fprintf(stderr, "client exit\n");
            //send_reply(acc_s, header, HEADER_TYPE_NULL, HEADER_CODE_CMD_UNDEF_ERR, 0, NULL);
            free(header);
            close_socket(acc_s);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    int s, accept_s, client_num = 0;
    pid_t cpid;
    char current_path[PATH_MAX];
    struct sockaddr_in skt;
    socklen_t sktlen;

    /* 初期のカレントディレクトリの変更 */
    if (argc == 1) {
        if (getcwd(current_path, PATH_MAX) == NULL) {
            perror("myftpd.c->main()->getcwd()");
            exit(EXIT_FAILURE);
        } 
        chdir(current_path);
    } else if (argc == 2) {
        if (chdir(argv[1]) == -1) {
            perror("myftpd.c->main()->chidir()");
            fprintf(stderr, "Cannot change directory: %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Please input an init directory\n");
        exit(EXIT_FAILURE);
    }

    /* ソケットの作成 */
    set_socket(&s);

    /* シグナルの設定 */
    signal_set();

    /* コネクション要求受信準備 */
    if (listen(s, PACKET_WAIT_MATRIX) < 0) {
        perror("myftpd.c->main()->listen");
        close_socket(s);
        exit(EXIT_FAILURE);
    }

    while(1) {
        sktlen = sizeof(skt);
        /* コネクション要求受信 */
        if ((accept_s = accept(s, (struct sockaddr *)&skt, &sktlen)) < 0) {
            perror("myftpd.c->main()->accept()");
            close_socket(s);
            exit(EXIT_FAILURE);
        }

        printf("accept ok\n");
        printf("client %d: connected\n", client_num++);

        if ((cpid = fork()) < 0) {
            perror("myftpd.c->main()->fork()");
            close_socket(s);
            exit(EXIT_FAILURE);
        } else if (cpid == 0) {
            /* 子プロセスの処理 */
            /* コネクション受信用ソケットは閉じる */
            close_socket(s);

            /*シグナルハンドラのリセット*/
            reset_signal_handlers();

            /* メッセージを受信して処理 */
            communicate_client(accept_s, client_num-1);

        } else {
            /* 親プロセスの処理 */
            /* 新しいクライアント用のソケットは閉じる*/
            close_socket(accept_s);
        }

    }


}