#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat *status)
{
    (void)file;
    status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int offset, int whence)
{
    (void)file;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int _read(int file, char *buffer, int length)
{
    (void)file;
    (void)buffer;
    (void)length;
    return 0;
}

void *_sbrk(ptrdiff_t increment)
{
    (void)increment;
    errno = ENOMEM;
    return (void *)-1;
}

int _write(int file, char *buffer, int length)
{
    (void)file;
    (void)buffer;
    return length;
}
