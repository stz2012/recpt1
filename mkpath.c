/* mkpath is originally written by Jonathan Leffler.
 * see "http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux" for detail.
 * copyright: (C) JLSS 1990-91,1997-98,2001,2005,2008
 */

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int
do_mkdir(const char *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0) {
        /* Directory does not exist */
        if (mkdir(path, mode) != 0)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
    }

    return(status);
}

int
mkpath(const char *path, mode_t mode)
{
    char *pp;
    char *sp;
    int status;
    char *copypath = strdup(path);

    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = do_mkdir(path, mode);
    free(copypath);
    return (status);
}
