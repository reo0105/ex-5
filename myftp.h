#ifndef MYFTP
    #define MYFTP

#include <stdio.h>
#include <stdint.h>

struct myftph {
    uint8_t type;
    uint8_t code;
    uint16_t length;
    char data[];
};

#define SERVER_PORT 50021
#define PACKET_WAIT_MATRIX 5
#define DATASIZE 1024

/* コマンドメッセージタイプ */
#define HEADER_TYPE_NULL 0x00
#define HEADER_TYPE_QUIT 0x01
#define HEADER_TYPE_PWD 0x02
#define HEADER_TYPE_CWD 0x03
#define HEADER_TYPE_LIST 0x04
#define HEADER_TYPE_RETR 0x05
#define HEADER_TYPE_STOR 0x06

/* リプライメッセージタイプ */
#define HEADER_TYPE_CMD_OK 0x10
#define HEADER_TYPE_CMD_ERR 0x11
#define HEADER_TYPE_FILE_ERR 0x12
#define HEADER_TYPE_UNKWN_ERR 0x13

/* データメッセージタイプ */
#define HEADER_TYPE_DATA 0x20

/* ヘッダーコード */
#define HEADER_CODE_CMD_OK 0x00
#define HEADER_CODE_CMD_OK_FROM_SERVER 0x01
#define HEADER_CODE_CMD_OK_FROM_CLIENT 0x02
#define HEADER_CODE_CMD_SYNTAX_ERR 0x01
#define HEADER_CODE_CMD_UNDEF_ERR 0x02
#define HEADER_CODE_CMD_PROTOCOL_ERR 0x03
#define HEADER_CODE_NO_FILE_ERR 0x00
#define HEADER_CODE_NO_PERMISSION_ERR 0x01
#define HEADER_CODE_UNDED_ERR 0x05
#define HEADER_CODE_NO_MORE_DATA 0x00
#define HEADER_CODE_FOLLOWED_DATA 0x01
#define HEADER_CODE_DATA_SYNTAX_ERR 0x02


void free_list(char **, int);
void recive_header(int, struct myftph *);
void send_reply(int, struct myftph *, int, int, int, char *);
void recv_data(int, struct myftph *, int, char *);
void send_message_content(int, struct myftph *, int);
void send_message_list(int, struct myftph *, char **, int);
void recv_message_content(int, struct myftph *, int);

#endif