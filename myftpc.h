#ifndef MYFTPC
    #define MYFTPC

struct proctable {
    char *cmd;
    void (*func)(int, char *[], int);
};


#define CMD_LEN PATH_MAX * 2
#define PORT_LEN 6
#define ARG_NUM 3
#define ERR_LEN  256


void get_ip_and_set_socket(char *, int *);
void getargs(int *, char **, char *);
void free_argv(char **);
void err_reason(int, int, char *);
void quit_proc(int, char **, int);
void pwd_proc(int, char **, int);
void cd_proc(int, char **, int);
void dir_proc(int, char **, int);
void lpwd_proc(int, char **, int);
void lcd_proc(int, char **, int);
void ldir_proc(int, char **, int);
void get_proc(int, char **, int);
void put_proc(int, char **, int);
void help_proc(int, char **, int);
void close_socket(int);

#endif