#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <getopt.h>
#include "recpt1core.h"

#define MSGSZ     255

void
show_usage(char *cmd)
{
    fprintf(stderr, "Usage: \n%s --pid pid [--channel channel] [--sid SID1,SID2] [--extend time_to_extend] [--time recording_time]\n", cmd);
    fprintf(stderr, "\n");
}

void
show_options(void)
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "--pid:               Process id of recpt1 to control\n");
    fprintf(stderr, "--channel:           Tune to specified channel\n");
    fprintf(stderr, "--sid SID1,SID2,...: Specify SID number in CSV format (101,102,...)\n");
    fprintf(stderr, "--extend:            Extend recording time\n");
    fprintf(stderr, "--time:              Set total recording time\n");
    fprintf(stderr, "--help:              Show this help\n");
    fprintf(stderr, "--version:           Show version\n");
    fprintf(stderr, "--list:              Show channel list\n");
}

int
main(int argc, char **argv)
{
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key = 0;
    int recsec = 0, extsec=0;
    char *channel = NULL;
    message_buf sbuf;
    size_t buf_length;
    char *sid_list = NULL;

    int result;
    int option_index;
    struct option long_options[] = {
        { "pid",       1, NULL, 'p'},
        { "channel",   1, NULL, 'c'},
        { "sid",       1, NULL, 'i'},
        { "extend",    1, NULL, 'e'},
        { "time",      1, NULL, 't'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        { "list",      0, NULL, 'l'},
        {0, 0, NULL, 0} /* terminate */
    };

    while((result = getopt_long(argc, argv, "p:c:i:e:t:hvl",
                                long_options, &option_index)) != -1) {
        switch(result) {
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
            fprintf(stderr, "control command for recpt1.\n");
            exit(0);
            break;
        case 'l':
            show_channels();
            exit(0);
            break;
        /* following options require argument */
        case 'p':
            key = (key_t)atoi(optarg);
            fprintf(stderr, "Pid = %d\n", key);
            break;
        case 'c':
            channel = optarg;
            fprintf(stderr, "Channel = %s\n", channel);
            break;
        case 'e':
            parse_time(optarg, &extsec);
            fprintf(stderr, "Extend %d sec\n", extsec);
            break;
        case 't':
            parse_time(optarg, &recsec);
            fprintf(stderr, "Total recording time = %d sec\n", recsec);
            break;
        case 'i':
            sid_list = optarg;
            fprintf(stderr, "Service ID = %s\n", sid_list);
            break;
        }
    }

    if(!key) {
        fprintf(stderr, "Arguments are necessary!\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        exit(1);
    }

    if ((msqid = msgget(key, msgflg )) < 0) {
        perror("msgget");
        exit(1);
    }

    sbuf.mtype = 1;
    sprintf(sbuf.mtext, "ch=%s t=%d e=%d sid=%s", channel, recsec, extsec, sid_list);

    buf_length = strlen(sbuf.mtext) + 1 ;

    if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}
