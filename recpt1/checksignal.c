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

#define MAX_RETRY (2)

/* type definitions */
typedef int boolean;

typedef struct thread_data {
    int ch;
    int lnb;    /* LNB voltage */
    int tfd;    /* tuner fd */
    ISDB_T_FREQ_CONV_TABLE *table;
} thread_data;

/* globals */
boolean f_exit = FALSE;
boolean use_bell = FALSE;

/* prototypes */
int tune(char *channel, thread_data *tdata, char *device);
int close_tuner(thread_data *tdata);

void
cleanup(thread_data *tdata)
{
    f_exit = TRUE;
}

/* will be signal handler thread */
void *
process_signals(void *data)
{
    sigset_t waitset;
    int sig;
    thread_data *tdata = (thread_data *)data;

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGUSR1);

    sigwait(&waitset, &sig);

    switch(sig) {
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
    }

    return NULL; /* dummy */
}

void
init_signal_handlers(pthread_t *signal_thread, thread_data *tdata)
{
    sigset_t blockset;

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGUSR1);

    if(pthread_sigmask(SIG_BLOCK, &blockset, NULL))
        fprintf(stderr, "pthread_sigmask() failed.\n");

    pthread_create(signal_thread, NULL, process_signals, tdata);
}

/* lookup frequency conversion table*/
ISDB_T_FREQ_CONV_TABLE *
searchrecoff(char *channel)
{
    int lp;

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

void
show_usage(char *cmd)
{
    fprintf(stderr, "Usage: \n%s [--device devicefile] [--lnb voltage] [--bell] channel\n", cmd);
    fprintf(stderr, "\n");
}

void
show_options(void)
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "--device devicefile: Specify devicefile to use\n");
    fprintf(stderr, "--lnb voltage:       Specify LNB voltage (0, 11, 15)\n");
    fprintf(stderr, "--bell:              Notify signal quality by bell\n");
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

    fprintf(stderr, "101ch: NHK BS1\n");
    fprintf(stderr, "102ch: NHK BS2\n");
    fprintf(stderr, "103ch: NHK BShi\n");
    fprintf(stderr, "141ch: BS Nittele\n");
    fprintf(stderr, "151ch: BS Asahi\n");
    fprintf(stderr, "161ch: BS-TBS\n");
    fprintf(stderr, "171ch: BS Japan\n");
    fprintf(stderr, "181ch: BS Fuji\n");
    fprintf(stderr, "191ch: WOWOW\n");
    fprintf(stderr, "192ch: WOWOW2\n");
    fprintf(stderr, "193ch: WOWOW3\n");
    fprintf(stderr, "200ch: Star Channel\n");
    fprintf(stderr, "211ch: BS11 Digital\n");
    fprintf(stderr, "222ch: TwellV\n");
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
do_bell(int bell)
{
    int i;
    for(i=0; i < bell; i++) {
        fprintf(stderr, "\a");
        usleep(400000);
    }
}

void
calc_cn(int fd, int type)
{
    int     rc;
    double  P;
    double  CNR;
    int bell = 0;

    if(ioctl(fd, GET_SIGNAL_STRENGTH, &rc) < 0) {
        fprintf(stderr, "Tuner Select Error\n");
        return ;
    }

    if(type == CHTYPE_GROUND) {
        P = log10(5505024/(double)rc) * 10;
        CNR = (0.000024 * P * P * P * P) - (0.0016 * P * P * P) +
                    (0.0398 * P * P) + (0.5491 * P)+3.0965;
    }
    else {
        CNR = getsignal_isdb_s(rc);
    }

    if(CNR >= 30.0)
        bell = 3;
    else if(CNR >= 15.0 && CNR < 30.0)
        bell = 2;
    else if(CNR < 15.0)
        bell = 1;

    fprintf(stderr, "\rC/N = %fdB ", CNR);
    if(use_bell)
        do_bell(bell);
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
        while(ioctl(tdata->tfd, SET_CHANNEL, &freq) < 0) {
            if(f_exit) {
                close_tuner(tdata);
                return 1;
            }
            fprintf(stderr, "No signal. Still trying: %s\n", device);
        }

        fprintf(stderr, "device = %s\n", device);
        tdata->ch = atoi(channel);
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
            int count = 0;

            tdata->tfd = open(tuner[lp], O_RDONLY);
            if(tdata->tfd >= 0) {
                /* power on LNB */
                if(tdata->table->type == CHTYPE_SATELLITE) {
                    if(ioctl(tdata->tfd, LNB_ENABLE, tdata->lnb) < 0) {
                        fprintf(stderr, "Warning: Power on LNB failed: %s\n", tuner[lp]);
                    }
                }

                /* tune to specified channel */
                while(ioctl(tdata->tfd, SET_CHANNEL, &freq) < 0 &&
                      count < MAX_RETRY) {
                    if(f_exit) {
                        close_tuner(tdata);
                        return 1;
                    }
                    fprintf(stderr, "No signal. Still trying: %s\n", tuner[lp]);
                    count++;
                }

                if(count >= MAX_RETRY) {
                    close_tuner(tdata);
                    count = 0;
                    continue;
                }

                fprintf(stderr, "device = %s\n", tuner[lp]);
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

    return 0; /* success */
}

int
close_tuner(thread_data *tdata)
{
    int rv = 0;

    if(tdata->tfd == -1)
        return rv;

    if(tdata->table->type == CHTYPE_SATELLITE) {
        if(ioctl(tdata->tfd, LNB_DISABLE, 0) < 0) {
            rv = 1;
        }
    }
    close(tdata->tfd);
    tdata->tfd = -1;

    return rv;
}

int
main(int argc, char **argv)
{
    pthread_t signal_thread;
    static thread_data tdata;
    int result;
    int option_index;
    struct option long_options[] = {
        { "bell",      0, NULL, 'b'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        { "list",      0, NULL, 'l'},
        { "LNB",       1, NULL, 'n'},
        { "lnb",       1, NULL, 'n'},
        { "device",    1, NULL, 'd'},
        {0, 0, NULL, 0} /* terminate */
    };

    char *device = NULL;
    int val;
    char *voltage[] = {"0V", "11V", "15V"};

    while((result = getopt_long(argc, argv, "bhvln:d:",
                                long_options, &option_index)) != -1) {
        switch(result) {
        case 'b':
            use_bell = TRUE;
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
            fprintf(stderr, "signal check utility for PT1/2 digital tuner.\n");
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
        case 'd':
            device = optarg;
            break;
        }
    }

    if(argc - optind < 1) {
        fprintf(stderr, "channel must be specified!\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    /* spawn signal handler thread */
    init_signal_handlers(&signal_thread, &tdata);

    /* tune */
    if(tune(argv[optind], &tdata, device) != 0)
        return 1;

    while(1) {
        if(f_exit)
            break;
        /* show signal strength */
        calc_cn(tdata.tfd, tdata.table->type);
        sleep(1);
    }

    /* wait for signal thread */
    pthread_kill(signal_thread, SIGUSR1);
    pthread_join(signal_thread, NULL);

    /* close tuner */
    if(close_tuner(&tdata) != 0)
        return 1;

    return 0;
}
