/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <ctype.h>
#include <libgen.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/ioctl.h>
#include "pt1_ioctl.h"

#include "config.h"
#include "decoder.h"
#include "recpt1.h"
#include "version.h"
#include "mkpath.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include "pt1_dev.h"
#include "tssplitter_lite.h"

/* maximum write length at once */
#define SIZE_CHANK 1316

/* ipc message size */
#define MSGSZ     255

/* type definitions */
typedef int boolean;

typedef struct sock_data {
    int sfd;    /* socket fd */
    struct sockaddr_in addr;
} sock_data;

typedef struct thread_data {
    QUEUE_T *queue;
    decoder *decoder;
    decoder_options *dopt;
    int ch;
    int lnb;    /* LNB voltage */
    int tfd;    /* tuner fd */
    int wfd;    /* output file fd */
    ISDB_T_FREQ_CONV_TABLE *table;
    sock_data *sock_data;
    pthread_t signal_thread;
    int recsec;
    time_t start_time;
    boolean indefinite;
    int msqid;
    splitter *splitter;
} thread_data;

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

/* globals */
boolean f_exit = FALSE;
char  bs_channel_buf[8];
ISDB_T_FREQ_CONV_TABLE isdb_t_conv_set = { 0, CHTYPE_SATELLITE, 0, bs_channel_buf };

/* prototypes */
ISDB_T_FREQ_CONV_TABLE *searchrecoff(char *channel);
void calc_cn(int fd, int type);
int tune(char *channel, thread_data *tdata, char *device);
int close_tuner(thread_data *tdata);

//read 1st line from socket
int read_line(int socket, char *p){
    int len = 0;
    while (1){
        int ret;
	ret = read(socket, p, 1);
        if ( ret == -1 ){
            perror("read");
            exit(1);
        } else if ( ret == 0 ){
            break;
        }
	if ( *p == '\n' ){
	    p++;
	    len++;
	    break;
	}
	p++;
	len++;
    }
    *p = '\0';
    return len;
}

/* ipc message receive */
void *
mq_recv(void *t)
{
    thread_data *tdata = (thread_data *)t;
    message_buf rbuf;
    char channel[16];
    int ch = 0, recsec = 0, time_to_add = 0;

    while(1) {
        if(msgrcv(tdata->msqid, &rbuf, MSGSZ, 1, 0) < 0) {
            return NULL;
        }

        sscanf(rbuf.mtext, "ch=%s t=%d e=%d", channel, &recsec, &time_to_add);
        ch = atoi(channel);
//        fprintf(stderr, "ch=%d time=%d extend=%d\n", ch, recsec, time_to_add);

        if(ch && tdata->ch != ch) {
#if 0
            /* re-initialize decoder */
            if(tdata->decoder) {
//                b25_finish(tdata->decoder);
                b25_shutdown(tdata->decoder);
                tdata->decoder = b25_startup(tdata->dopt);
                if(!tdata->decoder) {
                    fprintf(stderr, "Cannot start b25 decoder\n");
                    fprintf(stderr, "Fall back to encrypted recording\n");
                }
            }
#endif
            int current_type = tdata->table->type;
            ISDB_T_FREQ_CONV_TABLE *table = searchrecoff(channel);
            if (table == NULL) {
                fprintf(stderr, "Invalid Channel: %s\n", channel);
                goto CHECK_TIME_TO_ADD;
            }
            tdata->table = table;

            /* stop stream */
            ioctl(tdata->tfd, STOP_REC, 0);

            /* wait for remainder */
            while(tdata->queue->num_used > 0) {
                usleep(10000);
            }

            if (tdata->table->type != current_type) {
                /* re-open device */
                if(close_tuner(tdata) != 0)
                    return NULL;

                tune(channel, tdata, NULL);
            } else {
                /* SET_CHANNEL only */
                const FREQUENCY freq = {
                  .frequencyno = tdata->table->set_freq,
                  .slot = tdata->table->add_freq,
                };
                if(ioctl(tdata->tfd, SET_CHANNEL, &freq) < 0) {
                    fprintf(stderr, "Cannot tune to the specified channel\n");
                    tdata->ch = 0;
                    goto CHECK_TIME_TO_ADD;
                }
                tdata->ch = ch;
                calc_cn(tdata->tfd, tdata->table->type);
            }
            /* restart recording */
            if(ioctl(tdata->tfd, START_REC, 0) < 0) {
                fprintf(stderr, "Tuner cannot start recording\n");
                return NULL;
            }
        }

CHECK_TIME_TO_ADD:
        if(time_to_add) {
            tdata->recsec += time_to_add;
            fprintf(stderr, "Extended %d sec\n", time_to_add);
        }

        if(recsec) {
            time_t cur_time;
            time(&cur_time);
            if(cur_time - tdata->start_time > recsec) {
                f_exit = TRUE;
            }
            else {
                tdata->recsec = recsec;
                fprintf(stderr, "Total recording time = %d sec\n", recsec);
            }
        }

        if(f_exit)
            return NULL;
    }
}


/* lookup frequency conversion table*/
ISDB_T_FREQ_CONV_TABLE *
searchrecoff(char *channel)
{
    int lp;

    if(channel[0] == 'B' && channel[1] == 'S') {
        int node = 0;
        int slot = 0;
        char *bs_ch;

        bs_ch = channel + 2;
        while(isdigit(*bs_ch)) {
            node *= 10;
            node += *bs_ch++ - '0';
        }
        if(*bs_ch == '_' && (node&0x01) && node < ISDB_T_NODE_LIMIT) {
            if(isdigit(*++bs_ch)) {
                slot = *bs_ch - '0';
                if(*++bs_ch == '\0' && slot < ISDB_T_SLOT_LIMIT) {
                    isdb_t_conv_set.set_freq = node / 2;
                    isdb_t_conv_set.add_freq = slot;
                    sprintf(bs_channel_buf, "BS%d_%d", node, slot);
                    return &isdb_t_conv_set;
                }
            }
        }
        return NULL;
    }
    for(lp = 0; isdb_t_conv_table[lp].parm_freq != NULL; lp++) {
        /* return entry number in the table when strings match and
         * lengths are same. */
        if((memcmp(isdb_t_conv_table[lp].parm_freq, channel,
                   strlen(channel)) == 0) &&
           (strlen(channel) == strlen(isdb_t_conv_table[lp].parm_freq))) {
            return &isdb_t_conv_table[lp];
        }
    }
    return NULL;
}

QUEUE_T *
create_queue(size_t size)
{
    QUEUE_T *p_queue;
    int memsize = sizeof(QUEUE_T) + size * sizeof(BUFSZ*);

    p_queue = (QUEUE_T*)calloc(memsize, sizeof(char));

    if(p_queue != NULL) {
        p_queue->size = size;
        p_queue->num_avail = size;
        p_queue->num_used = 0;
        pthread_mutex_init(&p_queue->mutex, NULL);
        pthread_cond_init(&p_queue->cond_avail, NULL);
        pthread_cond_init(&p_queue->cond_used, NULL);
    }

    return p_queue;
}

void
destroy_queue(QUEUE_T *p_queue)
{
    if(!p_queue)
        return;

    pthread_mutex_destroy(&p_queue->mutex);
    pthread_cond_destroy(&p_queue->cond_avail);
    pthread_cond_destroy(&p_queue->cond_used);
    free(p_queue);
}

/* enqueue data. this function will block if queue is full. */
void
enqueue(QUEUE_T *p_queue, BUFSZ *data)
{
    struct timeval now;
    struct timespec spec;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered critical section */

    /* wait while queue is full */
    while(p_queue->num_avail == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_avail,
                               &p_queue->mutex, &spec);
        retry_count++;
        if(retry_count > 60) {
            f_exit = TRUE;
        }
        if(f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return;
        }
    }

    p_queue->buffer[p_queue->in] = data;

    /* move position marker for input to next position */
    p_queue->in++;
    p_queue->in %= p_queue->size;

    /* update counters */
    p_queue->num_avail--;
    p_queue->num_used++;

    /* leaving critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_used);
}

/* dequeue data. this function will block if queue is empty. */
BUFSZ *
dequeue(QUEUE_T *p_queue)
{
    struct timeval now;
    struct timespec spec;
    BUFSZ *buffer;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered the critical section*/

    /* wait while queue is empty */
    while(p_queue->num_used == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_used,
                               &p_queue->mutex, &spec);
        retry_count++;
        if(retry_count > 60) {
            f_exit = TRUE;
        }
        if(f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return NULL;
        }
    }

    /* take buffer address */
    buffer = p_queue->buffer[p_queue->out];

    /* move position marker for output to next position */
    p_queue->out++;
    p_queue->out %= p_queue->size;

    /* update counters */
    p_queue->num_avail++;
    p_queue->num_used--;

    /* leaving the critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_avail);

    return buffer;
}

/* this function will be reader thread */
void *
reader_func(void *p)
{
    thread_data *data = (thread_data *)p;
    QUEUE_T *p_queue = data->queue;
    decoder *dec = data->decoder;
    splitter *splitter = data->splitter;
    int wfd = data->wfd;
    boolean use_b25 = dec ? TRUE : FALSE;
    boolean use_udp = data->sock_data ? TRUE : FALSE;
    boolean fileless = FALSE;
    boolean use_splitter = splitter ? TRUE : FALSE;
    int sfd = -1;
    pthread_t signal_thread = data->signal_thread;
    struct sockaddr_in *addr = NULL;
    BUFSZ *qbuf;
    static splitbuf_t splitbuf;
    ARIB_STD_B25_BUFFER sbuf, dbuf, buf;
    int code;
    int split_select_finish = TSS_ERROR;

    buf.size = 0;
    buf.data = NULL;
    splitbuf.size = 0;

    if(wfd == -1)
        fileless = TRUE;

    if(use_udp) {
        sfd = data->sock_data->sfd;
        addr = &data->sock_data->addr;
    }

    while(1) {
        ssize_t wc = 0;
        int file_err = 0;
        qbuf = dequeue(p_queue);
        /* no entry in the queue */
        if(qbuf == NULL) {
            break;
        }

        sbuf.data = qbuf->buffer;
        sbuf.size = qbuf->size;

        buf = sbuf; /* default */

        if(use_b25) {
            code = b25_decode(dec, &sbuf, &dbuf);
            if(code < 0) {
                fprintf(stderr, "b25_decode failed (code=%d). fall back to encrypted recording.\n", code);
                use_b25 = FALSE;
            }
            else
                buf = dbuf;
        }


        if(use_splitter) {
            splitbuf.size = 0;

            while(buf.size) {
                /* 分離対象PIDの抽出 */
                if(split_select_finish != TSS_SUCCESS) {
                    split_select_finish = split_select(splitter, &buf);
                    if(split_select_finish == TSS_NULL) {
                        /* mallocエラー発生 */
                        fprintf(stderr, "split_select malloc failed\n");
                        use_splitter = FALSE;
                        goto fin;
                    }
                    else if(split_select_finish != TSS_SUCCESS) {
                        /* 分離対象PIDが完全に抽出できるまで出力しない
                         * 1秒程度余裕を見るといいかも
                         */
                        time_t cur_time;
                        time(&cur_time);
                        if(cur_time - data->start_time > 4) {
                            use_splitter = FALSE;
                            goto fin;
                        }
                        break;
                    }
                }
                /* 分離対象以外をふるい落とす */
                code = split_ts(splitter, &buf, &splitbuf);
                if(code == TSS_NULL) {
                    fprintf(stderr, "PMT reading..\n");
                }
                else if(code != TSS_SUCCESS) {
                    fprintf(stderr, "split_ts failed\n");
                    break;
                }

                break;
            } /* while */

            buf.size = splitbuf.size;
            buf.data = splitbuf.buffer;
        fin:
            ;
        } /* if */


        if(!fileless) {
            /* write data to output file */
            int size_remain = buf.size;
            int offset = 0;

            while(size_remain > 0) {
                int ws = size_remain < SIZE_CHANK ? size_remain : SIZE_CHANK;

                wc = write(wfd, buf.data + offset, ws);
                if(wc < 0) {
                    perror("write");
                    file_err = 1;
                    pthread_kill(signal_thread,
                                 errno == EPIPE ? SIGPIPE : SIGUSR2);
                    break;
                }
                size_remain -= wc;
                offset += wc;
            }
        }

        if(use_udp && sfd != -1) {
            /* write data to socket */
            int size_remain = buf.size;
            int offset = 0;
            while(size_remain > 0) {
                int ws = size_remain < SIZE_CHANK ? size_remain : SIZE_CHANK;
                wc = write(sfd, buf.data + offset, ws);
                if(wc < 0) {
                    if(errno == EPIPE)
                        pthread_kill(signal_thread, SIGPIPE);
                    break;
                }
                size_remain -= wc;
                offset += wc;
            }
        }

        free(qbuf);
        qbuf = NULL;

        /* normal exit */
        if((f_exit && !p_queue->num_used) || file_err) {

            buf = sbuf; /* default */

            if(use_b25) {
                code = b25_finish(dec, &sbuf, &dbuf);
                if(code < 0)
                    fprintf(stderr, "b25_finish failed\n");
                else
                    buf = dbuf;
            }

            if(use_splitter) {
                /* 分離対象以外をふるい落とす */
                code = split_ts(splitter, &buf, &splitbuf);
                if(code == TSS_NULL) {
                    split_select_finish = TSS_ERROR;
                    fprintf(stderr, "PMT reading..\n");
                }
                else if(code != TSS_SUCCESS) {
                    fprintf(stderr, "split_ts failed\n");
                    break;
                }

                buf.data = splitbuf.buffer;
                buf.size = splitbuf.size;
            }

            if(!fileless && !file_err) {
                wc = write(wfd, buf.data, buf.size);
                if(wc < 0) {
                    perror("write");
                    file_err = 1;
                    pthread_kill(signal_thread,
                                 errno == EPIPE ? SIGPIPE : SIGUSR2);
                }
            }

            if(use_udp && sfd != -1) {
                wc = write(sfd, buf.data, buf.size);
                if(wc < 0) {
                    if(errno == EPIPE)
                        pthread_kill(signal_thread, SIGPIPE);
                }
            }

            break;
        }
    }

    time_t cur_time;
    time(&cur_time);
    fprintf(stderr, "Recorded %dsec\n",
            (int)(cur_time - data->start_time));

    return NULL;
}

void
show_usage(char *cmd)
{
#ifdef HAVE_LIBARIB25
    fprintf(stderr, "Usage: \n%s [--b25 [--round N] [--strip] [--EMM]] [--udp [--addr hostname --port portnumber]] [--http portnumber] [--device devicefile] [--lnb voltage] [--sid SID1,SID2] channel rectime destfile\n", cmd);
#else
    fprintf(stderr, "Usage: \n%s [--strip] [--EMM]] [--udp [--addr hostname --port portnumber]] [--device devicefile] [--lnb voltage] [--sid SID1,SID2] channel rectime destfile\n", cmd);
#endif
    fprintf(stderr, "\n");
    fprintf(stderr, "Remarks:\n");
    fprintf(stderr, "if rectime  is '-', records indefinitely.\n");
    fprintf(stderr, "if destfile is '-', stdout is used for output.\n");
}

void
show_options(void)
{
    fprintf(stderr, "Options:\n");
#ifdef HAVE_LIBARIB25
    fprintf(stderr, "--b25:               Decrypt using BCAS card\n");
    fprintf(stderr, "  --round N:         Specify round number\n");
    fprintf(stderr, "  --strip:           Strip null stream\n");
    fprintf(stderr, "  --EMM:             Instruct EMM operation\n");
#endif
    fprintf(stderr, "--udp:               Turn on udp broadcasting\n");
    fprintf(stderr, "  --addr hostname:   Hostname or address to connect\n");
    fprintf(stderr, "  --port portnumber: Port number to connect\n");
    fprintf(stderr, "--http portnumber:   Turn on http broadcasting (run as a daemon)\n");
    fprintf(stderr, "--device devicefile: Specify devicefile to use\n");
    fprintf(stderr, "--lnb voltage:       Specify LNB voltage (0, 11, 15)\n");
    fprintf(stderr, "--sid SID1,SID2,...: Specify SID number in CSV format (101,102,...)\n");
    fprintf(stderr, "--help:              Show this help\n");
    fprintf(stderr, "--version:           Show version\n");
    fprintf(stderr, "--list:              Show channel list\n");
}

void
show_channels(void)
{
    FILE *f;
    char *home;
    char buf[255], filename[255];

    fprintf(stderr, "Available Channels:\n");

    home = getenv("HOME");
    sprintf(filename, "%s/.recpt1-channels", home);
    f = fopen(filename, "r");
    if(f) {
        while(fgets(buf, 255, f))
            fprintf(stderr, "%s", buf);
        fclose(f);
    }
    else
        fprintf(stderr, "13-62: Terrestrial Channels\n");

    fprintf(stderr, "BS01_0: BS朝日\n");
    fprintf(stderr, "BS01_1: BS-TBS\n");
    fprintf(stderr, "BS03_0: WOWOWプライム\n");
    fprintf(stderr, "BS03_1: BSジャパン\n");
    fprintf(stderr, "BS05_0: WOWOWライブ\n");
    fprintf(stderr, "BS05_1: WOWOWシネマ\n");
    fprintf(stderr, "BS07_0: スターチャンネル2/3\n");
    fprintf(stderr, "BS07_1: BSアニマックス\n");
    fprintf(stderr, "BS07_2: ディズニーチャンネル\n");
    fprintf(stderr, "BS09_0: BS11\n");
    fprintf(stderr, "BS09_1: スターチャンネル1\n");
    fprintf(stderr, "BS09_2: TwellV\n");
    fprintf(stderr, "BS11_0: FOX bs238\n");
    fprintf(stderr, "BS11_1: BSスカパー!\n");
    fprintf(stderr, "BS11_2: 放送大学\n");
    fprintf(stderr, "BS13_0: BS日テレ\n");
    fprintf(stderr, "BS13_1: BSフジ\n");
    fprintf(stderr, "BS15_0: NHK BS1\n");
    fprintf(stderr, "BS15_1: NHK BSプレミアム\n");
    fprintf(stderr, "BS17_0: 地デジ難視聴1(NHK/NHK-E/CX)\n");
    fprintf(stderr, "BS17_1: 地デジ難視聴2(NTV/TBS/EX/TX)\n");
    fprintf(stderr, "BS19_0: グリーンチャンネル\n");
    fprintf(stderr, "BS19_1: J SPORTS 1\n");
    fprintf(stderr, "BS19_2: J SPORTS 2\n");
    fprintf(stderr, "BS21_0: IMAGICA BS\n");
    fprintf(stderr, "BS21_1: J SPORTS 3\n");
    fprintf(stderr, "BS21_2: J SPORTS 4\n");
    fprintf(stderr, "BS23_0: BS釣りビジョン\n");
    fprintf(stderr, "BS23_1: 日本映画専門チャンネル\n");
    fprintf(stderr, "BS23_2: D-Life\n");
    fprintf(stderr, "C13-C63: CATV Channels\n");
    fprintf(stderr, "CS2-CS24: CS Channels\n");
}

float
getsignal_isdb_s(int signal)
{
    /* apply linear interpolation */
    static const float afLevelTable[] = {
        24.07f,    // 00    00    0        24.07dB
        24.07f,    // 10    00    4096     24.07dB
        18.61f,    // 20    00    8192     18.61dB
        15.21f,    // 30    00    12288    15.21dB
        12.50f,    // 40    00    16384    12.50dB
        10.19f,    // 50    00    20480    10.19dB
        8.140f,    // 60    00    24576    8.140dB
        6.270f,    // 70    00    28672    6.270dB
        4.550f,    // 80    00    32768    4.550dB
        3.730f,    // 88    00    34816    3.730dB
        3.630f,    // 88    FF    35071    3.630dB
        2.940f,    // 90    00    36864    2.940dB
        1.420f,    // A0    00    40960    1.420dB
        0.000f     // B0    00    45056    -0.01dB
    };

    unsigned char sigbuf[4];
    memset(sigbuf, '\0', sizeof(sigbuf));
    sigbuf[0] =  (((signal & 0xFF00) >> 8) & 0XFF);
    sigbuf[1] =  (signal & 0xFF);

    /* calculate signal level */
    if(sigbuf[0] <= 0x10U) {
        /* clipped maximum */
        return 24.07f;
    }
    else if (sigbuf[0] >= 0xB0U) {
        /* clipped minimum */
        return 0.0f;
    }
    else {
        /* linear interpolation */
        const float fMixRate =
            (float)(((unsigned short)(sigbuf[0] & 0x0FU) << 8) |
                    (unsigned short)sigbuf[0]) / 4096.0f;
        return afLevelTable[sigbuf[0] >> 4] * (1.0f - fMixRate) +
            afLevelTable[(sigbuf[0] >> 4) + 0x01U] * fMixRate;
    }
}

void
calc_cn(int fd, int type)
{
    int     rc ;
    double  P ;
    double  CNR;

    if(ioctl(fd, GET_SIGNAL_STRENGTH, &rc) < 0) {
        fprintf(stderr, "Tuner Select Error\n");
        return ;
    }

    if(type == CHTYPE_GROUND) {
        P = log10(5505024/(double)rc) * 10;
        CNR = (0.000024 * P * P * P * P) - (0.0016 * P * P * P) +
                    (0.0398 * P * P) + (0.5491 * P)+3.0965;
        fprintf(stderr, "C/N = %fdB\n", CNR);
    }
    else {
        CNR = getsignal_isdb_s(rc);
        fprintf(stderr, "C/N = %fdB\n", CNR);
    }
}

void
cleanup(thread_data *tdata)
{
    /* stop recording */
    ioctl(tdata->tfd, STOP_REC, 0);

    /* xxx need mutex? */
    f_exit = TRUE;

    pthread_cond_signal(&tdata->queue->cond_avail);
    pthread_cond_signal(&tdata->queue->cond_used);
}

/* will be signal handler thread */
void *
process_signals(void *data)
{
    sigset_t waitset;
    int sig;
    thread_data *tdata = (thread_data *)data;

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGPIPE);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGUSR1);
    sigaddset(&waitset, SIGUSR2);

    sigwait(&waitset, &sig);

    switch(sig) {
    case SIGPIPE:
        fprintf(stderr, "\nSIGPIPE received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGINT:
        fprintf(stderr, "\nSIGINT received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGTERM:
        fprintf(stderr, "\nSIGTERM received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGUSR1: /* normal exit*/
        cleanup(tdata);
        break;
    case SIGUSR2: /* error */
        fprintf(stderr, "Detected an error. cleaning up...\n");
        cleanup(tdata);
        break;
    }

    return NULL; /* dummy */
}

void
init_signal_handlers(pthread_t *signal_thread, thread_data *tdata)
{
    sigset_t blockset;

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGPIPE);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGUSR1);
    sigaddset(&blockset, SIGUSR2);

    if(pthread_sigmask(SIG_BLOCK, &blockset, NULL))
        fprintf(stderr, "pthread_sigmask() failed.\n");

    pthread_create(signal_thread, NULL, process_signals, tdata);
}

int
tune(char *channel, thread_data *tdata, char *device)
{
    char **tuner;
    int num_devs;
    int lp;
    FREQUENCY freq;

    /* get channel */
    tdata->table = searchrecoff(channel);
    if(tdata->table == NULL) {
        fprintf(stderr, "Invalid Channel: %s\n", channel);
        return 1;
    }

    freq.frequencyno = tdata->table->set_freq;
    freq.slot = tdata->table->add_freq;

    /* open tuner */
    /* case 1: specified tuner device */
    if(device) {
        tdata->tfd = open(device, O_RDONLY);
        if(tdata->tfd < 0) {
            fprintf(stderr, "Cannot open tuner device: %s\n", device);
            return 1;
        }

        /* power on LNB */
        if(tdata->table->type == CHTYPE_SATELLITE) {
            if(ioctl(tdata->tfd, LNB_ENABLE, tdata->lnb) < 0) {
                fprintf(stderr, "Power on LNB failed: %s\n", device);
            }
        }

        /* tune to specified channel */
        if(ioctl(tdata->tfd, SET_CHANNEL, &freq) < 0) {
            close(tdata->tfd);
            fprintf(stderr, "Cannot tune to the specified channel: %s\n", device);
            return 1;
        }
        else {
            tdata->ch = atoi(channel);
        }
    }
    else {
        /* case 2: loop around available devices */
        if(tdata->table->type == CHTYPE_SATELLITE) {
            tuner = bsdev;
            num_devs = NUM_BSDEV;
        }
        else {
            tuner = isdb_t_dev;
            num_devs = NUM_ISDB_T_DEV;
        }

        for(lp = 0; lp < num_devs; lp++) {
            tdata->tfd = open(tuner[lp], O_RDONLY);
            if(tdata->tfd >= 0) {
                /* power on LNB */
                if(tdata->table->type == CHTYPE_SATELLITE) {
                    if(ioctl(tdata->tfd, LNB_ENABLE, tdata->lnb) < 0) {
                        fprintf(stderr, "Warning: Power on LNB failed: %s\n", tuner[lp]);
                    }
                }

                /* tune to specified channel */
                if(ioctl(tdata->tfd, SET_CHANNEL, &freq) < 0) {
                    close(tdata->tfd);
                    tdata->tfd = -1;
                    continue;
                }

                break; /* found suitable tuner */
            }
        }

        /* all tuners cannot be used */
        if(tdata->tfd < 0) {
            fprintf(stderr, "Cannot tune to the specified channel\n");
            return 1;
        }
        else {
            tdata->ch = atoi(channel);
        }
    }

    /* show signal strength */
    calc_cn(tdata->tfd, tdata->table->type);

    return 0; /* success */
}

int
parse_time(char *rectimestr, thread_data *tdata)
{
    /* indefinite */
    if(!strcmp("-", rectimestr)) {
        tdata->indefinite = TRUE;
        tdata->recsec = -1;
    }
    /* colon */
    else if(strchr(rectimestr, ':')) {
        int n1, n2, n3;
        if(sscanf(rectimestr, "%d:%d:%d", &n1, &n2, &n3) == 3)
            tdata->recsec = n1 * 3600 + n2 * 60 + n3;
        else if(sscanf(rectimestr, "%d:%d", &n1, &n2) == 2)
            tdata->recsec = n1 * 3600 + n2 * 60;
    }
    /* HMS */
    else {
        char *tmpstr;
        char *p1, *p2;

        tmpstr = strdup(rectimestr);
        p1 = tmpstr;
        while(*p1 && !isdigit(*p1))
            p1++;

        /* hour */
        if((p2 = strchr(p1, 'H')) || (p2 = strchr(p1, 'h'))) {
            *p2 = '\0';
            tdata->recsec += atoi(p1) * 3600;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* minute */
        if((p2 = strchr(p1, 'M')) || (p2 = strchr(p1, 'm'))) {
            *p2 = '\0';
            tdata->recsec += atoi(p1) * 60;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* second */
        tdata->recsec += atoi(p1);

        free(tmpstr);
    }

    return 0; /* success */
}

int
close_tuner(thread_data *tdata)
{
    int rv = 0;

    if(tdata->table->type == CHTYPE_SATELLITE) {
        if(ioctl(tdata->tfd, LNB_DISABLE, 0) < 0) {
            rv = 1;
        }
    }
    close(tdata->tfd);

    return rv;
}

int
main(int argc, char **argv)
{
    time_t cur_time;
    pthread_t signal_thread;
    pthread_t reader_thread;
    pthread_t ipc_thread;
    QUEUE_T *p_queue = create_queue(MAX_QUEUE);
    BUFSZ   *bufptr;
    decoder *dec = NULL;
    splitter *splitter = NULL;
    static thread_data tdata;
    decoder_options dopt = {
        4,  /* round */
        0,  /* strip */
        0   /* emm */
    };
    tdata.dopt = &dopt;
    tdata.lnb = 0;

    int result;
    int option_index;
    struct option long_options[] = {
#ifdef HAVE_LIBARIB25
        { "b25",       0, NULL, 'b'},
        { "B25",       0, NULL, 'b'},
        { "round",     1, NULL, 'r'},
        { "strip",     0, NULL, 's'},
        { "emm",       0, NULL, 'm'},
        { "EMM",       0, NULL, 'm'},
#endif
        { "LNB",       1, NULL, 'n'},
        { "lnb",       1, NULL, 'n'},
        { "udp",       0, NULL, 'u'},
        { "addr",      1, NULL, 'a'},
        { "port",      1, NULL, 'p'},
        { "http",      1, NULL, 'H'},
        { "device",    1, NULL, 'd'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        { "list",      0, NULL, 'l'},
        { "sid",       1, NULL, 'i'},
        {0, 0, NULL, 0} /* terminate */
    };

    boolean use_b25 = FALSE;
    boolean use_udp = FALSE;
    boolean use_http = FALSE;
    boolean fileless = FALSE;
    boolean use_stdout = FALSE;
    boolean use_splitter = FALSE;
    char *host_to = NULL;
    int port_to = 1234;
    int port_http = 12345;
    sock_data *sockdata = NULL;
    char *device = NULL;
    int val;
    char *voltage[] = {"0V", "11V", "15V"};
    char *sid_list = NULL;

    while((result = getopt_long(argc, argv, "br:smn:ua:H:p:d:hvli:",
                                long_options, &option_index)) != -1) {
        switch(result) {
        case 'b':
            use_b25 = TRUE;
            fprintf(stderr, "using B25...\n");
            break;
        case 's':
            dopt.strip = TRUE;
            fprintf(stderr, "enable B25 strip\n");
            break;
        case 'm':
            dopt.emm = TRUE;
            fprintf(stderr, "enable B25 emm processing\n");
            break;
        case 'u':
            use_udp = TRUE;
            host_to = "localhost";
            fprintf(stderr, "enable UDP broadcasting\n");
            break;
        case 'H':
            use_http = TRUE;
            port_http = atoi(optarg);
            fprintf(stderr, "creating a http daemon\n");
            break;
        case 'h':
            fprintf(stderr, "\n");
            show_usage(argv[0]);
            fprintf(stderr, "\n");
            show_options();
            fprintf(stderr, "\n");
            show_channels();
            fprintf(stderr, "\n");
            exit(0);
            break;
        case 'v':
            fprintf(stderr, "%s %s\n", argv[0], version);
            fprintf(stderr, "recorder command for PT1/2/3 digital tuner.\n");
            exit(0);
            break;
        case 'l':
            show_channels();
            exit(0);
            break;
        /* following options require argument */
        case 'n':
            val = atoi(optarg);
            switch(val) {
            case 11:
                tdata.lnb = 1;
                break;
            case 15:
                tdata.lnb = 2;
                break;
            default:
                tdata.lnb = 0;
                break;
            }
            fprintf(stderr, "LNB = %s\n", voltage[tdata.lnb]);
            break;
        case 'r':
            dopt.round = atoi(optarg);
            fprintf(stderr, "set round %d\n", dopt.round);
            break;
        case 'a':
            use_udp = TRUE;
            host_to = optarg;
            fprintf(stderr, "UDP destination address: %s\n", host_to);
            break;
        case 'p':
            port_to = atoi(optarg);
            fprintf(stderr, "UDP port: %d\n", port_to);
            break;
        case 'd':
            device = optarg;
            fprintf(stderr, "using device: %s\n", device);
            break;
        case 'i':
            use_splitter = TRUE;
            sid_list = optarg;
            break;
        }
    }

    //http broadcasting
    if(use_http){
            fprintf(stderr, "run as a daemon..\n");
            if(daemon(1,1)){
                perror("failed to start");
                exit(1);
            }
            fprintf(stderr, "pid = %d\n", getpid());

            int connected_socket, listening_socket;
            struct sockaddr_in sin;
            unsigned int len;
            int ret;
            int sock_optval = 1;
				
            listening_socket = socket(AF_INET, SOCK_STREAM, 0);
            if ( listening_socket == -1 ){
                perror("socket");
                exit(1);
            }
				
            if ( setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
		            &sock_optval, sizeof(sock_optval)) == -1 ){
	        perror("setsockopt");
                exit(1);
            }
				
            sin.sin_family = AF_INET;
            sin.sin_port = htons(port_http);
            sin.sin_addr.s_addr = htonl(INADDR_ANY);

				
            if ( bind(listening_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0 ){
	        perror("bind");
                exit(1);
            }
				
            ret = listen(listening_socket, SOMAXCONN);
            if ( ret == -1 ){
	        perror("listen");
                exit(1);
            }
            fprintf(stderr,"listening at port %d\n", port_http);

            /* initialize decoder */
            if(use_b25) {
                dec = b25_startup(&dopt);
                if(!dec) {
                    fprintf(stderr, "Cannot start b25 decoder\n");
                    fprintf(stderr, "Fall back to encrypted recording\n");
                    use_b25 = FALSE;
                }
            }
            //set rectime to the infinite
            if(parse_time("-",&tdata) != 0){
                return 1;
            }

            while(1){
              struct sockaddr_in peer_sin;
              
              len = sizeof(peer_sin);
              
              connected_socket = accept(listening_socket, (struct sockaddr *)&peer_sin, &len);
              if ( connected_socket == -1 ){
	            perror("accept");
                exit(1);
              }
            
              int error;
              char hbuf[NI_MAXHOST], nhbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
              error =  getnameinfo((struct sockaddr *)&peer_sin, sizeof(peer_sin), hbuf, sizeof(hbuf), NULL, 0, 0);
              if(error) {
                fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                exit(1);
              }
              error =  getnameinfo((struct sockaddr *)&peer_sin, sizeof(peer_sin), nhbuf, sizeof(nhbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
              if(error) {
                fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                exit(1);
              }
              fprintf(stderr,"connect from: %s [%s] port %s\n", hbuf, nhbuf, sbuf);
              
              int read_size;
              
              char buf[256];
              read_size = read_line(connected_socket, buf);
              fprintf(stderr,"request command is %s\n",buf);
              char s0[256],s1[256],s2[256];
              sscanf(buf,"%s%s%s",s0,s1,s2);
              char delim[] = "/";
              char *channel = strtok(s1,delim);
              char *sidflg = strtok(NULL,delim);
              if(sidflg)
                sid_list = sidflg;
              fprintf(stderr,"channel is %s\n",channel);
              
              if(sid_list == NULL || !strcmp(sid_list, "all")){
                use_splitter = FALSE;
                splitter = NULL;
              }else{
                use_splitter = TRUE;
              }

              /* initialize splitter */
              if(use_splitter) {
                splitter = split_startup(sid_list);
                if(splitter->sid_list == NULL) {
                  fprintf(stderr, "Cannot start TS splitter\n");
                  return 1;
                }
              }
              char header[] =  "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nCache-Control: no-cache\r\n\r\n";
              ret = write(connected_socket, header, strlen(header));
              
              //set write target to http
              tdata.wfd = connected_socket;
              
              //tune
                if(tune(channel, &tdata, device) != 0){
                  fprintf(stderr, "Tuner cannot start recording\n");
                  continue;
                }
                /* prepare thread data */
                tdata.queue = p_queue;
                tdata.decoder = dec;
                tdata.splitter = splitter;
                tdata.sock_data = sockdata;
                
                /* spawn signal handler thread */
                init_signal_handlers(&signal_thread, &tdata);

                /* spawn reader thread */
                tdata.signal_thread = signal_thread;
                pthread_create(&reader_thread, NULL, reader_func, &tdata);

                /* spawn ipc thread */
                key_t key;
                key = (key_t)(getpid());

                if ((tdata.msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
                    perror("msgget");
                }
                pthread_create(&ipc_thread, NULL, mq_recv, &tdata);

                /* start recording */
                if(ioctl(tdata.tfd, START_REC, 0) < 0) {
                    fprintf(stderr, "Tuner cannot start recording\n");
                    exit(1);
                }

                fprintf(stderr, "Broadcasting...\n");

                time(&tdata.start_time);

                /* read from tuner */
                while(1) {
                    if(f_exit)
                        break;

                    time(&cur_time);
                    bufptr = malloc(sizeof(BUFSZ));
                    if(!bufptr) {
                        f_exit = TRUE;
                        break;
                    }
                    bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
                    if(bufptr->size <= 0) {
                        if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
                            f_exit = TRUE;
                            enqueue(p_queue, NULL);
                            break;
                        }
                        else {
                            continue;
                        }
                    }
                    enqueue(p_queue, bufptr);

                    /* stop recording */
                    time(&cur_time);
                    if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
                        ioctl(tdata.tfd, STOP_REC, 0);
                        // read remaining data
                        while(1) {
                            bufptr = malloc(sizeof(BUFSZ));
                            if(!bufptr) {
                                f_exit = TRUE;
                                break;
                            }
                            bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
                            if(bufptr->size <= 0) {
                                f_exit = TRUE;
                                enqueue(p_queue, NULL);
                                break;
                            }
                            enqueue(p_queue, bufptr);
                        }
                        break;
                    }
                }
            
                /* delete message queue*/
                msgctl(tdata.msqid, IPC_RMID, NULL);

            
                pthread_kill(signal_thread, SIGUSR1);
            
                /* wait for threads */
                pthread_join(reader_thread, NULL);
                pthread_join(signal_thread, NULL);
                pthread_join(ipc_thread, NULL);

                //reset queue
                destroy_queue(p_queue);
                p_queue = create_queue(MAX_QUEUE);

                /* close tuner */
                if(close_tuner(&tdata) != 0)
                    return 1;
		/* close splitter */
    		if(use_splitter) {
	               	split_shutdown(splitter);
    		}

                /* close http socket */
                close(tdata.wfd);

                fprintf(stderr,"connection closed. still listening at port %d\n",port_http);
                f_exit = FALSE;
            }
    }

    if(argc - optind < 3) {
        if(argc - optind == 2 && use_udp) {
            fprintf(stderr, "Fileless UDP broadcasting\n");
            fileless = TRUE;
            tdata.wfd = -1;
        }
        else {
            fprintf(stderr, "Arguments are necessary!\n");
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    fprintf(stderr, "pid = %d\n", getpid());

    /* tune */
    if(tune(argv[optind], &tdata, device) != 0)
        return 1;

    /* set recsec */
    if(parse_time(argv[optind + 1], &tdata) != 0)
        return 1;

    /* open output file */
    char *destfile = argv[optind + 2];
    if(destfile && !strcmp("-", destfile)) {
        use_stdout = TRUE;
        tdata.wfd = 1; /* stdout */
    }
    else {
        if(!fileless) {
            int status;
            char *path = strdup(argv[optind + 2]);
            char *dir = dirname(path);
            status = mkpath(dir, 0777);
            if(status == -1)
                perror("mkpath");
            free(path);

            tdata.wfd = open(argv[optind + 2], (O_RDWR | O_CREAT | O_TRUNC), 0666);
            if(tdata.wfd < 0) {
                fprintf(stderr, "Cannot open output file: %s\n",
                        argv[optind + 2]);
                return 1;
            }
        }
    }

    /* initialize decoder */
    if(use_b25) {
        dec = b25_startup(&dopt);
        if(!dec) {
            fprintf(stderr, "Cannot start b25 decoder\n");
            fprintf(stderr, "Fall back to encrypted recording\n");
            use_b25 = FALSE;
        }
    }
    /* initialize splitter */
    if(use_splitter) {
        splitter = split_startup(sid_list);
        if(splitter->sid_list == NULL) {
            fprintf(stderr, "Cannot start TS splitter\n");
            return 1;
        }
    }

    /* initialize udp connection */
    if(use_udp) {
      sockdata = calloc(1, sizeof(sock_data));
      struct in_addr ia;
      ia.s_addr = inet_addr(host_to);
      if(ia.s_addr == INADDR_NONE) {
            struct hostent *hoste = gethostbyname(host_to);
            if(!hoste) {
                perror("gethostbyname");
                return 1;
            }
            ia.s_addr = *(in_addr_t*) (hoste->h_addr_list[0]);
        }
        if((sockdata->sfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            return 1;
        }

        sockdata->addr.sin_family = AF_INET;
        sockdata->addr.sin_port = htons (port_to);
        sockdata->addr.sin_addr.s_addr = ia.s_addr;

        if(connect(sockdata->sfd, (struct sockaddr *)&sockdata->addr,
                   sizeof(sockdata->addr)) < 0) {
            perror("connect");
            return 1;
        }
    }

    /* prepare thread data */
    tdata.queue = p_queue;
    tdata.decoder = dec;
    tdata.splitter = splitter;
    tdata.sock_data = sockdata;

    /* spawn signal handler thread */
    init_signal_handlers(&signal_thread, &tdata);

    /* spawn reader thread */
    tdata.signal_thread = signal_thread;
    pthread_create(&reader_thread, NULL, reader_func, &tdata);

    /* spawn ipc thread */
    key_t key;
    key = (key_t)getpid();

    if ((tdata.msqid = msgget(key, IPC_CREAT | 0666)) < 0) {
        perror("msgget");
    }
    pthread_create(&ipc_thread, NULL, mq_recv, &tdata);

    /* start recording */
    if(ioctl(tdata.tfd, START_REC, 0) < 0) {
        fprintf(stderr, "Tuner cannot start recording\n");
        return 1;
    }

    fprintf(stderr, "Recording...\n");

    time(&tdata.start_time);

    /* read from tuner */
    while(1) {
        if(f_exit)
            break;

        time(&cur_time);
        bufptr = malloc(sizeof(BUFSZ));
        if(!bufptr) {
            f_exit = TRUE;
            break;
        }
        bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
        if(bufptr->size <= 0) {
            if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
                f_exit = TRUE;
                enqueue(p_queue, NULL);
                break;
            }
            else {
                free(bufptr);
                continue;
            }
        }
        enqueue(p_queue, bufptr);

        /* stop recording */
        time(&cur_time);
        if((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
            ioctl(tdata.tfd, STOP_REC, 0);
            /* read remaining data */
            while(1) {
                bufptr = malloc(sizeof(BUFSZ));
                if(!bufptr) {
                    f_exit = TRUE;
                    break;
                }
                bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
                if(bufptr->size <= 0) {
                    f_exit = TRUE;
                    enqueue(p_queue, NULL);
                    break;
                }
                enqueue(p_queue, bufptr);
            }
            break;
        }
    }

    /* delete message queue*/
    msgctl(tdata.msqid, IPC_RMID, NULL);

    pthread_kill(signal_thread, SIGUSR1);

    /* wait for threads */
    pthread_join(reader_thread, NULL);
    pthread_join(signal_thread, NULL);
    pthread_join(ipc_thread, NULL);

    /* close tuner */
    if(close_tuner(&tdata) != 0)
        return 1;

    /* release queue */
    destroy_queue(p_queue);

    /* close output file */
    if(!use_stdout)
        close(tdata.wfd);

    /* free socket data */
    if(use_udp) {
        close(sockdata->sfd);
        free(sockdata);
    }

    /* release decoder */
    if(use_b25) {
        b25_shutdown(dec);
    }
    if(use_splitter) {
        split_shutdown(splitter);
    }

    return 0;
}
