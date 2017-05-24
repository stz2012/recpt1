/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#ifndef _RECPT1_UTIL_H_
#define _RECPT1_UTIL_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "pt1_ioctl.h"
#include "config.h"
#include "decoder.h"
#include "recpt1.h"
#include "mkpath.h"
#include "tssplitter_lite.h"

/* ipc message size */
#define MSGSZ     255

/* used in checksigna.c */
#define MAX_RETRY (2)

/* type definitions */
typedef int boolean;

typedef struct {
    int sfd;    /* socket fd */
    struct sockaddr_in addr;
} sock_data;

typedef struct {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

typedef struct {
    int tfd;    /* tuner fd */ //xxx variable

    int wfd;    /* output file fd */ //invariable
    int lnb;    /* LNB voltage */ //invariable
    int msqid; //invariable
    time_t start_time; //invariable

    int recsec; //xxx variable

    boolean indefinite; //invaliable
    boolean tune_persistent; //invaliable

    QUEUE_T *queue; //invariable
    ISDB_T_FREQ_CONV_TABLE *table; //invariable
    sock_data *sock_data; //invariable
    pthread_t signal_thread; //invariable
    decoder *decoder; //invariable
    decoder_options *dopt; //invariable
    splitter *splitter; //invariable
} thread_data;

extern const char *version;
extern char *bsdev[];
extern char *isdb_t_dev[];

extern boolean f_exit;

/* prototypes */
int tune(char *channel, thread_data *tdata, char *device);
int close_tuner(thread_data *tdata);
void show_channels(void);
ISDB_T_FREQ_CONV_TABLE *searchrecoff(char *channel);
void calc_cn(int fd, int type, boolean use_bell);
int parse_time(char *rectimestr, int *recsec);
void do_bell(int bell);


#endif
