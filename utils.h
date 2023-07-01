#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include "logger.h"

/*!< This function creates a file */
int create_file_if_does_not_exist(const char *filename)
{
    return open(filename, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
}

int does_file_exist(const char *filename)
{
    int res = access(filename, F_OK);
    if (res == -1)
    {
        if (errno == ENOENT)
            return 0;

        switch (errno)
        {
        case EINVAL:
            logger("ERROR", "Call to access failed. The value of mode argument is invalid.");
            break;

        case ENOTDIR:
            logger("ERROR", "Call to access failed. A component of the path prefix is not a directory.");
            break;

        case ENAMETOOLONG:
            logger("ERROR", "Call to access failed. A component of a pathname exceeded {NAME_MAX} characters, or an entire path name exceeded {PATH_MAX} characters.");
            break;

        case ELOOP:
            logger("ERROR", "Call to access failed. Too many symbolic links were encountered in translating the pathname or AT_SYMLINK_NOFOLLOW_ANY was passed and a symbolic link was encountered in translating the pathname.");
            break;

        case EROFS:
            logger("ERROR", "Call to access failed. Write access is requested for a file on a read-only file system.");
            break;

        case ETXTBSY:
            logger("ERROR", "Call to access failed. Write access is requested for a pure procedure (shared text) file presently being executed.");
            break;

        case EACCES:
            logger("ERROR", "Call to access failed. Permission bits of the file mode do not permit the requested access, or search permission is denied on a component of the path prefix.");
            break;
        case EFAULT:
            logger("ERROR", "Call to access failed. The path argument points outside the process's allocated address space.");
            break;
        case EIO:
            logger("ERROR", "Call to access failed. An I/O error occurred while reading from or writing to the file system.");
            break;

        default:
            logger("ERROR", "Call to access failed. Unknown error occured.");
            break;
        }
        return -1;
    }
    return 1;
}

int remove_file(const char *filename)
{
    if (remove(filename) == -1)
    {
        logger("ERROR", "Could not remove file %s", filename);
        return -1;
    }

    return 0;
}

int msleep(long msec)
{
    struct timespec ts;
    int res = 0;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * (long)(1e6);

    nanosleep(&ts, NULL);

    // ? Use this if you need to sleep during interrupts as well.
    // do {
    //     res = nanosleep(&ts, &ts);
    // } while (res && errno == EINTR);

    return res;
}

#endif