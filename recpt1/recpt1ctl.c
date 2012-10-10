#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <getopt.h>

#include "version.h"

#define MSGSZ     255

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

void
show_usage(char *cmd)
{
    fprintf(stderr, "Usage: \n%s --pid pid [--channel channel] [--extend time_to_extend] [--time recording_time]\n", cmd);
    fprintf(stderr, "\n");
}

void
show_options(void)
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "--pid:               Process id of recpt1 to control\n");
    fprintf(stderr, "--channel:           Tune to specified channel\n");
    fprintf(stderr, "--extend:            Extend recording time\n");
    fprintf(stderr, "--time:              Set total recording time\n");
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
    fprintf(stderr, "CS2-CS24: CS Channels\n");
}

int
parse_time(char *rectimestr, int *recsec)
{
    /* indefinite */
    if(!strcmp("-", rectimestr)) {
        *recsec = -1;
    }
    /* colon */
    else if(strchr(rectimestr, ':')) {
        int n1, n2, n3;
        if(sscanf(rectimestr, "%d:%d:%d", &n1, &n2, &n3) == 3)
            *recsec = n1 * 3600 + n2 * 60 + n3;
        else if(sscanf(rectimestr, "%d:%d", &n1, &n2) == 2)
            *recsec = n1 * 3600 + n2 * 60;
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
            *recsec += atoi(p1) * 3600;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* minute */
        if((p2 = strchr(p1, 'M')) || (p2 = strchr(p1, 'm'))) {
            *p2 = '\0';
            *recsec += atoi(p1) * 60;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* second */
        *recsec += atoi(p1);

        free(tmpstr);
    }

    return 0; /* success */
}

int
main(int argc, char **argv)
{
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key = 0;
    char *channel = NULL;
    int ch=0, recsec = 0, extsec=0;
    message_buf sbuf;
    size_t buf_length;

    int result;
    int option_index;
    struct option long_options[] = {
        { "pid",       1, NULL, 'p'},
        { "channel",   1, NULL, 'c'},
        { "extend",    1, NULL, 'e'},
        { "time",      1, NULL, 't'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        { "list",      0, NULL, 'l'},
        {0, 0, NULL, 0} /* terminate */
    };

    while((result = getopt_long(argc, argv, "p:c:e:t:hvl",
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
            ch = atoi(channel);
            if (ch)
                fprintf(stderr, "Channel = %d\n", ch);
            else
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
    if (ch)
        sprintf(sbuf.mtext, "ch=%d t=%d e=%d", ch, recsec, extsec);
    else
        sprintf(sbuf.mtext, "ch=%s t=%d e=%d", channel, recsec, extsec);

    buf_length = strlen(sbuf.mtext) + 1 ;

    if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}
