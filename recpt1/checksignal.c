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
#include "recpt1core.h"
#include "mkpath.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include "tssplitter_lite.h"

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
    boolean use_bell = FALSE;

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
            fprintf(stderr, "signal check utility for PT1/2/3 digital tuner.\n");
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

    /* set tune_persistent flag */
    tdata.tune_persistent = TRUE;

    /* spawn signal handler thread */
    init_signal_handlers(&signal_thread, &tdata);

    /* tune */
    if(tune(argv[optind], &tdata, device) != 0)
        return 1;

    while(1) {
        if(f_exit)
            break;
        /* show signal strength */
        calc_cn(tdata.tfd, tdata.table->type, use_bell);
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
