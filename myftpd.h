#ifndef MYFTPD
    #define MYFTPD

#include "myftp.h"

struct proctable {
    int type;
    void (*func)(int, struct myftph *, char *, int);
};


void set_socket(int *);
void close_socket(int);
void sig_child_handler(int);
void signal_set();
void reset_signal_handlers();
void recv_quit(int, struct myftph *, char *, int);
void recv_pwd(int, struct myftph *, char *, int);
void recv_cwd(int, struct myftph *, char *, int);
void recv_list(int, struct myftph *, char *, int);
void recv_retr(int, struct myftph *, char *, int);
void recv_stor(int, struct myftph *, char *, int);
void communicate_client(int, int);


#endif