CC 	   = gcc

CFLAGS = -Wall -Wextra -ggdb

TARGET_S = myftpd
TARGET_C = myftpc

SRCS_S   = myftp.c myftpd.c myls.c
OBJS_S   = myftp.o myftpd.o myls.o

SRCS_C   = myftp.c myftpc.c myls.c
OBJS_C   = myftp.o myftpc.o myls.o

RM 	   = rm -f

all : $(TARGET_S) $(TARGET_C)

$(TARGET_S) : $(OBJS_S)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_C) : $(OBJS_C)
	$(CC) $(CFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $<

myftpd.o: myftp.h myftpd.h myls.h util.h

myftpc.o: myftp.h myftpc.h myls.h util.h

myftp.o: myftp.h

myls.o: myls.h util.h

clean:
	$(RM) $(TARGET_S) $(TARGET_C) $(OBJS_S) $(OBJS_C)

clean_target:
	$(RM) $(TARGET_S) $(TARGET_C)

clean_obj:
	$(RM) $(OBJS_S) $(OBJS_C)
