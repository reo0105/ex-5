#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "myftp.h"




void free_list(char **data, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        free(data[i]);
    }

    return;
}


/* ヘッダー(4バイト)の受信 */
void recive_header(int acc_s, struct myftph *header)
{
    memset(header, 0, sizeof(struct myftph));

    //printf("type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));

    if (recv(acc_s, header, sizeof(struct myftph), 0) < (ssize_t)0) {
        perror("myftp.c->recive_header()->recv()");
        /* ヘッダが受信できなかったらどうする？？ */ // 終了かな
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL); //いる？？ // いりますね
        close(acc_s);
        free(header);
        exit(EXIT_FAILURE);
    }

    //printf("recv_h: type = %d, code = %d, len = %d\n", header->type, header->code, ntohs(header->length));
}


/* コマンド/リプライメッセージの送信 */
void send_reply(int acc_s, struct myftph *header, int type, int code, int length, char *reply_data)
{
    int send_len;
    struct myftph *reply;

    /* 返信用のメモリを確保 */
    if ((reply = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        close(acc_s);
        exit(EXIT_FAILURE);
    }

    reply->type = type;
    reply->code = code;
    reply->length = htons(length);

    //printf("send: type = %d, code = %d, len = %d\n", type, code, length);

    if (length != 0) {
        /* 返信用データの作成 */
        strcpy(reply->data, reply_data);
    }

    send_len = sizeof(struct myftph) + length;

    if (send(acc_s, reply, (size_t)send_len, 0) < (ssize_t)0) {
        perror("myftp.c->send_reply()->send()");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(reply);
        free(header);
        close(acc_s);
        exit(EXIT_FAILURE);
    }

    free(reply);

    return;
}


/* データ本体を受信 */
void recv_data(int acc_s, struct myftph *header, int len, char *data)
{
    int recv_total = 0, recv_size = 0;

    while (recv_total < len) {
        if ((recv_size = recv(acc_s, data + recv_total, len - recv_total, 0)) < (ssize_t)0) { // 1回のrecvでlen分の読み出しができないことがあるらしい。 while文で等しくなるまで？
            perror("myftp.c->recv_data()->recv()");
            /* データが受信できなかったらどうする？？ */ // 終了かな
            send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
            close(acc_s);
            free(header);
            exit(EXIT_FAILURE);
        }

        recv_total += recv_size; 
        //printf("recv_total = %d\n", recv_total);
    }

    data[len]  = '\0';

    //printf("done\n");
    //printf("data = %s\n", data);

    return;
}


/* データ本体を送信(ファイルの中身) */
void send_message_content(int acc_s, struct myftph *header, int fd)
{
    int remaining_size;
    char buf[DATASIZE];
    struct myftph *data_content;
    off_t file_size;
    ssize_t obj;

    /* 返信用のメモリを確保 */
    if ((data_content = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        close(acc_s);
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* ファイルがちょうど1024バイトのときとかのためにファイル全体のサイズを取得 */
    if ((file_size = lseek(fd, 0, SEEK_END)) < (off_t)0) {
        perror("myftp.c->send_message_content()->lseek()");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        free(data_content);
        close(acc_s);
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* オフセットを先頭に戻す */
    if (lseek(fd, 0, SEEK_SET) < (off_t)0) {
        perror("myftp.c->send_message_content()->lseek");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        free(data_content);
        close(acc_s);
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* ファイルの残りサイズとデータタイプの設定 */
    remaining_size = (int)file_size;
    data_content->type = HEADER_TYPE_DATA;
    //printf("remaining: %d\n", remaining_size);

    do {
        //printf("send ");
        if ((obj = read(fd, buf, DATASIZE)) < (ssize_t)0) {
            perror("myftp.c->send_message_content()->read()");
            send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
            free(data_content);
            free(header);
            close(fd);
            close(acc_s);
            exit(EXIT_FAILURE);
        }
        /*残りのファイルサイズと送信データ内容を更新*/
        remaining_size -= (int)obj;
        data_content->code = (remaining_size == 0) ? HEADER_CODE_NO_MORE_DATA : HEADER_CODE_FOLLOWED_DATA;
        data_content->length = htons(obj);
        strcpy(data_content->data, buf);
        
        if (send(acc_s, data_content, sizeof(struct myftph) + (size_t)obj, 0) < (ssize_t)0) {
            perror("myftp.c->send_message_content()->send()");
            send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
            free(data_content);
            free(header);
            close(fd);
            close(acc_s);
            exit(EXIT_FAILURE);
        }
    } while (remaining_size != 0);

    close(fd);
    free(data_content);

    return;
}


/* データ本体を送信(ファイル一覧) */
void send_message_list(int acc_s, struct myftph *header, char **data, int count)
{
    int i;
    struct myftph *data_content;

    /* 返信用のメモリを確保 */
    if ((data_content = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        close(acc_s);
        exit(EXIT_FAILURE);
    }

    /* ヘッダのタイプ */
    data_content->type = HEADER_TYPE_DATA;

    for (i = 0; i < count; i++) {
        //printf("send list, count = %d\n", count);
        /* ヘッダのcodeとlengthと送信データの設定 */
        data_content->code = (i == count-1) ? HEADER_CODE_NO_MORE_DATA : HEADER_CODE_FOLLOWED_DATA;
        data_content->length = htons(strlen(data[i]));
        strcpy(data_content->data, data[i]);
        
       // printf("type = %d, code = %d, len =%d\n", data_content->type, data_content->code, ntohs(data_content->length));
       // printf("list len = %ld\n", strlen(data[i]));

        /* 送信 */
        if (send(acc_s, data_content, sizeof(struct myftph) + strlen(data[i]), 0) < (ssize_t)0) {
            perror("myftp.c->send_message_list()->send()");
            send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
            free(data_content);
            free(header);
            close(acc_s);
            exit(EXIT_FAILURE);
        }

    }

    free(data_content);

    return;
}



/* データ本体を受信 */
void recv_message_content(int acc_s, struct myftph *header, int fd)
{
    int len;
    struct myftph *data_content;
    ssize_t recv_total, recv_size, write_obj;

    /* 受信用のメモリを確保 */
    if ((data_content = (struct myftph *)malloc(sizeof(struct myftph) + DATASIZE)) == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        free(header);
        close(acc_s);
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* メッセージが終わるまで受信 */
    do {
        recive_header(acc_s, data_content);
        len = ntohs(data_content->length);
        recv_total = 0;
        recv_size = 0;
        /* すべてを受信 */
        while (recv_total < len) {
            /* 受信が失敗 */
            if ((recv_size = recv(acc_s, data_content->data + recv_total, len - recv_total, 0)) < (ssize_t)0) { // 1回のrecvでlen分の読み出しができないことがあるらしい。 while文で等しくなるまで？
                perror("myftp.c->recv_message_content()->recv()");
                /* データが受信できなかったらどうする？？ */ // 終了かな
                send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL); //いる？？ // いりますね
                close(acc_s);
                free(header);
                free(data_content);
                exit(EXIT_FAILURE);
            }

            recv_total += recv_size; 
            printf("recv_total = %ld/%d\n", recv_total, len);
        }

        // if ((recv_obj = recv(acc_s, data_content, sizeof(struct myftph) + DATASIZE, 0)) < (ssize_t)0) {
        //     perror("recv");
        //     send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
        //     close(acc_s);
        //     close(fd);
        //     free(header);
        //     free(data_content);
        //     exit(EXIT_FAILURE);
        // }
        //printf("type = %d, code = %d, len = %d\n", data_content->type, data_content->code, ntohs(data_content->length));

        /* header->typeが不正な場合 */
        if (data_content->type != HEADER_TYPE_DATA) {
            fprintf(stderr, "Header type field is invalid\n");
            //send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_DATA_SYNTAX_ERR, 0, NULL);
            free(data_content);
            close(fd);
            return;

        /* header->codeが不正な場合 */
        } else if (data_content->code != HEADER_CODE_NO_MORE_DATA && data_content->code != HEADER_CODE_FOLLOWED_DATA) {
            fprintf(stderr, "Header code field is invalid\n");
            //send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_DATA_SYNTAX_ERR, 0, NULL);
            free(data_content);
            close(fd);
            return;

        /* header->lengthが不正な場合 */
        } else if (data_content->length == 0) {
            fprintf(stderr, "Header length field is invalid\n");
            //send_reply(acc_s, header, HEADER_TYPE_CMD_ERR, HEADER_CODE_DATA_SYNTAX_ERR, 0, NULL);
            free(data_content);
            close(fd);
            return;

        /* 正しい場合 */
        } else {
            //printf("start write\n");
            //printf("%s\n", data_content->data);
            /* 読み込んだサイズより書き込みサイズが小さい場合 */
            if ((write_obj = write(fd, data_content->data, recv_total)) < recv_total) {
                perror("myftp.c->recv_message_content()->write()");
                send_reply(acc_s, header, HEADER_TYPE_UNKWN_ERR, HEADER_CODE_UNDED_ERR, 0, NULL);
                free(data_content);
                close(fd);
                return;
            }
            //printf("done write: %ld\n", write_obj);
        }
    } while(data_content->code == HEADER_CODE_FOLLOWED_DATA);

    free(data_content);
    close(fd);

    return ;
}