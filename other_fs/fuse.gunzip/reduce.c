/*
    reduce
    Copyright (C) 2006 Kyle Sallee <kyle.sallee@gmail.com>
    All Rights Reserved

    This may be modified and distributed using the same license
    as fuse, filesystem in user space, is provided that you change
    the name of your fork to avoid confusion with this project.

    This program accepts filenames as parameters.
    Files must be read only or readable by no more than owner.
    After reduction the files become read only.
    Reduction is done using zlib.
    Either gzip and gunzip can be used to uncompress files.
    cp will uncompress files when transparent compression is active.
    Compressed files report the same size and time as the originals.
    However, compressed files consume less disk space.
    Compare the size reported by ls with the size reported by du.

    Do not reduce files located in /bin /etc /lib /lib64 /sbin
    # /etc/init.d/usr.fuse.bunzip2 start
    activates transparent decompression for /usr only.
*/

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <utime.h>
#include <zlib.h>

/*  How annoying and unexpected that fuse does not support
    writeable mmaps to files on fuse mounted filesystems
*/

#define  NO_WRITE_MMAP

#ifdef  NO_WRITE_MMAP
#include <stdlib.h>
#endif

long int	gzip(char *opath, char *ipath, int size)
{
    z_stream	z;
    void	*ibuf, *obuf;
    int		zerror, in, out;

    if ((in  = open(ipath, O_RDONLY)) == -1)                                     { warn(NULL); return -1; }
    if ((out = open(opath, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) == -1) { warn(NULL); close(in); return -1; }
    if (  ftruncate(out, size)  == -1)                                           { warn(NULL); close(in); close(out); return -1; }


    if (close(out) == -1 ) { warn("Error closing %s", opath); return -1; }
    if ((out = open(opath, O_RDWR, S_IWUSR | S_IRUSR)) == -1) { warn(NULL); close(in); return -1; }

#ifdef NO_WRITE_MMAP
    if ((obuf = malloc(size)) == NULL ) {
        warn("%s : malloc failed", ipath);
        close(in); close(out);
        return -1;
    }
#else
    if ((obuf = mmap(NULL, size, PROT_WRITE, MAP_SHARED, out, 0)) == MAP_FAILED)  { warn("%s : mmap on %s failed", ipath, opath);                     close(in); close(out); return -1; }
#endif
    if ((ibuf = mmap(NULL, size, PROT_READ,  MAP_SHARED, in,  0)) == MAP_FAILED)  { warn("%s : mmap on %s failed", ipath, ipath); munmap(obuf, size); close(in); close(out); return -1; }

    z.zalloc    = NULL;
    z.zfree     = NULL;
    z.opaque    = NULL;
    z.next_in   = ibuf;
    z.next_out  = obuf;
    z.avail_out = size;
    z.avail_in  = size;
    z.data_type = Z_BINARY;

    zerror      = deflateInit2(&z, 9, Z_DEFLATED, 31, 9, Z_DEFAULT_STRATEGY);

    if ( zerror != Z_OK ) {
        warn("%s : deflateInit2 failed %i", ipath, zerror);
        munmap(ibuf, size);  close(in);
        munmap(obuf, size);  close(out);
        return -1;
    }

    zerror      = deflate(&z, Z_FINISH);
                  deflateEnd(&z);

#ifdef NO_WRITE_MMAP
    write(out, obuf, z.total_out); close(out);
#else
    msync(obuf, z.total_out, MS_SYNC); munmap(obuf, size); close(out);
#endif
                                       munmap(ibuf, size); close(in);

    if ( (zerror != Z_STREAM_END) &&
         (zerror != Z_OK) ) {
         warn("%s : deflate failed %i", ipath, zerror);
         return -1;
    }
    return z.total_out;
}

void	reduce(char *path)
{
    struct stat		st;
    char		ext[16];
    int			plen = strlen(path)+16;
    char		npath[plen];
    mode_t 		mode;
    struct utimbuf	utb;
    long int		gsize;

    if (stat(path,&st) == -1)        { warnx("%s : unable to stat.", path);  return; }
    if (! S_ISREG(st.st_mode))       { warnx("%s : not a regular file.", path); return; }
    if ((st.st_mode & S_IWGRP) != 0) { warnx("%s : group writeable.", path); return; }
    if ((st.st_mode & S_IWOTH) != 0) { warnx("%s : other writeable.", path); return; }
    if (st.st_size > 67108864 )      { warnx("%s : too large >64M.", path); return; }
    if (st.st_blocks == 1)           { warnx("%s : fits in one block.", path); return; }
    if (st.st_nlink  >  1)           { warnx("%s : more than 1 hard link.", path); return; }
    if (st.st_size   <= 4096)        { warnx("%s : too small <= 4096.", path); return; }

    snprintf(ext,17,".%li",st.st_mtime);
      strcpy(npath, path);
     strncat(npath, ext, plen);

    if ( (st.st_mode & S_IWUSR) == 0 )
        mode = st.st_mode;  else
        mode = st.st_mode ^ S_IWUSR;

    utb.actime  = st.st_atime;
    utb.modtime = st.st_mtime;

    if ( (gsize = gzip(npath, path, st.st_size))  == -1) { unlink(npath); return; }
    if ( ( (gsize      + 4095) / 4096 ) >= \
         ( (st.st_size + 4095) / 4096 )                ) { warnx("%s : unable to reduce.", path); unlink(npath); return; }
    if (   chown(path, st.st_uid, st.st_gid)      == -1) { warn(NULL); unlink(npath); return; }
    if (   chmod(npath, mode)                     == -1) { warn(NULL); unlink(npath); return; }
    if (  rename(npath, path)                     == -1) { warn(NULL); unlink(npath); return; }
    utime(path, &utb);
    return;
}


int main(int argc, char *argv[])
{
    int		count;

    for (count=1;count<argc;count++)
      reduce(argv[count]);
    return 0;
}
