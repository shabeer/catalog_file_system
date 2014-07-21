/*
    fuse.gunzip
    Copyright (C) 2006 Kyle Sallee <kyle.sallee@gmail.com>
    All Rights Reserved

    This may be modified and distributed using the same license
    as fuse, filesystem in user space, is provided that you change
    the name of your fork to avoid confusion with this project.
    I prefer to be emailed contributions for improvements
    unless modifications completely divirge from the goal 
    of providing a filesystem performance booster.

    Provided that fuse is started at boot
    followed by the usr.fuse.gunzip script
    and they both worked correctly
    then files on /usr will be available
    for transparent decompression.

    The compilation command listed in the comments below
    does not work for fuse.gunzip.c
    Use make instead.
*/


/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2006  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    gcc -Wall `pkg-config fuse --cflags --libs` fusexmp_fh.c -o fusexmp_fh
*/

#define FUSE_USE_VERSION 26

/*
#include <config.h>
*/

#define HAVE_SETXATTR 1
#define HAVE_FDATASYNC 1

#define _GNU_SOURCE

#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <unistd.h> */
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


#include <sys/param.h>			/* for MAXPATHLEN */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <pthread.h>
#include <zlib.h>
#include <err.h>

#ifndef MAX_CACHED_FILES
#define	MAX_CACHED_FILES	4096
#endif

#ifndef LEN_CACHED_FILES
#define LEN_CACHED_FILES	64
#endif

#ifndef DEFAULT_CACHE_SIZE
#define DEFAULT_CACHE_SIZE	96 * 1024 * 1024
#endif

typedef	struct	sname_s {
    off_t	size;
    char	name[LEN_CACHED_FILES];  /* 294912 bytes */
} sname;


typedef	struct	history_s {
    int			head, tail;
    sname		list[MAX_CACHED_FILES];
    off_t		avail, size;
    pthread_mutex_t	mtx;
} history;

history		cache;
uid_t		who;
char		cpath[MAXPATHLEN];
char		magic[2];
const char    gzmagic[2] = { 31, 139 };
const struct rlimit   rl = { 65536, 65536 };	/* limit of 65536 open files */

/*  FILE *README;  */

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    int res;

    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_fgetattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi)
{
    int res;

    (void) path;

    res = fstat(fi->fh, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char *path, int mask)
{
    int res;

    res = access(path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    int res;

    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp = opendir(path);
    if (dp == NULL)
        return -errno;

    fi->fh = (unsigned long) dp;
    return 0;
}

static inline DIR *get_dirp(struct fuse_file_info *fi)
{
    return (DIR *) (uintptr_t) fi->fh;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    DIR *dp = get_dirp(fi);
    struct dirent *de;

    (void) path;
    seekdir(dp, offset);
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, telldir(dp)))
            break;
    }

    return 0;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp = get_dirp(fi);
    (void) path;
    closedir(dp);
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    if (S_ISFIFO(mode))
        res = mkfifo(path, mode);
    else
        res = mknod(path, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    int res;

    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char *path)
{
    int res;

    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char *path)
{
    int res;

    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
    int res;

    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rename(const char *from, const char *to)
{
    int res;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_link(const char *from, const char *to)
{
    int res;

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
    int res;

    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;

    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
    int res;

    res = truncate(path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
                         struct fuse_file_info *fi)
{
    int res;

    (void) path;

    res = ftruncate(fi->fh, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    res = utimes(path, tv);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int fd;

    fd = open(path, fi->flags, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}


void    sniptail(void)
{
    shm_unlink(    cache.list[cache.tail  ].name);
    cache.avail += cache.list[cache.tail++].size;
    if (MAX_CACHED_FILES  ==  cache.tail) cache.tail = 0;
}


void	freespace(off_t amount)
{
    while ( (amount     >  cache.avail) &&
            (cache.head != cache.tail) ) sniptail();
}


void	takespace(off_t amount, char *name)
{
                 cache.avail                   -= amount;
                 cache.list[cache.head  ].size  = amount;
         strncpy(cache.list[cache.head++].name, name, LEN_CACHED_FILES);
    if (MAX_CACHED_FILES == cache.head) cache.head = 0;
    if (cache.tail       == cache.head) sniptail();
}


int  decomp(int out, int in, off_t size)
{
    z_stream	z;
    void	*ibuf, *obuf;
    int		zerror;

    if (ftruncate(out, size)  == -1) return errno;
    if ((obuf = mmap(NULL, size, PROT_WRITE, MAP_SHARED, out, 0)) == MAP_FAILED) return errno;
    if ((ibuf = mmap(NULL, size, PROT_READ,  MAP_SHARED, in,  0)) == MAP_FAILED)  { zerror = errno; munmap(obuf, size); return zerror; }

    z.zalloc    = Z_NULL;
    z.zfree     = Z_NULL;
    z.opaque    = Z_NULL;
    z.next_in   = ibuf;
    z.next_out  = obuf;
    z.avail_out = \
    z.avail_in  = size;
    zerror      = inflateInit2(&z, 31);

    if ( zerror != Z_OK ) {
        if (zerror == Z_ERRNO) zerror = errno; else zerror = EIO;
        munmap(ibuf, size);
        munmap(obuf, size);
        return zerror;
    }

    zerror      = inflate(&z, Z_FINISH);
                  inflateEnd(&z);

                                munmap(ibuf, size);
    msync(obuf, size, MS_SYNC); munmap(obuf, size);

    if (zerror == Z_STREAM_END) return 0;     else
    if (zerror == Z_ERRNO)      return errno; else
                                return EIO;
}


static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    int			fd, fdn, ouch;
    struct stat		sbuf;

    if ( (fd = open(path, fi->flags)) == -1) return -errno;

/*
    The original conditional was split into two parts to make it faster.

    true if writing
    true if failure to lstat
    true if writeable
    true if failure to read
    true if failure to rewind
    true if magic not gzip

    If no conditions are true it is a reduced file.
*/

    if ( ! ( ( fi->flags & (O_WRONLY | O_RDWR)                ) ||
             ( lstat(path, &sbuf) == -1                       ) ||
             ( sbuf.st_mode & ( S_IWUSR | S_IWGRP | S_IWOTH ) ) ||
             ( sbuf.st_size < 30                              )
           )
       ) {

        /* Probably a reduced file, is it cached? */

        pthread_mutex_lock(&cache.mtx);

        snprintf(cpath, MAXPATHLEN, "/.%li.%li.%li.%li.%li.fguz", \
                 ( long int ) sbuf.st_size,  \
                 ( long int ) sbuf.st_ctime, \
                 ( long int ) sbuf.st_ino,   \
                 ( long int ) sbuf.st_dev,   \
                 ( long int ) who);

        if ( (fdn = shm_open(cpath, fi->flags, S_IRUSR)) != -1) {
            fi->fh = fdn;
            close(fd);
            pthread_mutex_unlock(&cache.mtx);
            return 0;
        }  else
        if ( ! ( (  read( fd, magic, 2        ) == -1  ) ||
                 ( lseek( fd, 0,     SEEK_SET ) == -1  ) ||
                 ( * ( uint16_t * )   magic     != \
                   * ( uint16_t * ) gzmagic ) )
           ) {
            /* Definitely an uncached reduced file */
            freespace(sbuf.st_size);
            if ((fdn = shm_open(cpath, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) != -1) {
                if ((ouch = decomp(fdn, fd, sbuf.st_size)) != 0) {
                    close(fdn); close(fd);
                    shm_unlink(cpath);
                    pthread_mutex_unlock(&cache.mtx);
                    return -ouch;
                }
                close(fdn); close(fd);
                takespace(sbuf.st_size, cpath);
                if ( (fd = shm_open(cpath, fi->flags, S_IRUSR)) == -1) {
                    pthread_mutex_unlock(&cache.mtx);
                    return -errno;
                }
            } else {
                ouch = errno;
                close(fd);
                pthread_mutex_unlock(&cache.mtx);
                return -ouch;
            }
        }
        pthread_mutex_unlock(&cache.mtx);
    }
    fi->fh = fd;
    return 0;
}


static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
    int res;

    (void) path;
    /* This is called from every close on an open file, so call the
       close on the underlying filesystem.  But since flush may be
       called multiple times for an open file, this must not really
       close the file.  This is important if used on a network
       filesystem like NFS which flush the data/metadata on close() */
    res = close(dup(fi->fh));
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);

    return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    int res;
    (void) path;

#ifndef HAVE_FDATASYNC
    (void) isdatasync;
#else
    if (isdatasync)
        res = fdatasync(fi->fh);
    else
#endif
        res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    int res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    int res = lgetxattr(path, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    int res = llistxattr(path, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    int res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
                    struct flock *lock)
{
    (void) path;

    return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
                       sizeof(fi->lock_owner));
}

static struct fuse_operations xmp_oper = {
    .getattr	= xmp_getattr,
    .fgetattr	= xmp_fgetattr,
    .access	= xmp_access,
    .readlink	= xmp_readlink,
    .opendir	= xmp_opendir,
    .readdir	= xmp_readdir,
    .releasedir	= xmp_releasedir,
    .mknod	= xmp_mknod,
    .mkdir	= xmp_mkdir,
    .symlink	= xmp_symlink,
    .unlink	= xmp_unlink,
    .rmdir	= xmp_rmdir,
    .rename	= xmp_rename,
    .link	= xmp_link,
    .chmod	= xmp_chmod,
    .chown	= xmp_chown,
    .truncate	= xmp_truncate,
    .ftruncate	= xmp_ftruncate,
    .utimens	= xmp_utimens,
    .create	= xmp_create,
    .open	= xmp_open,
    .read	= xmp_read,
    .write	= xmp_write,
    .statfs	= xmp_statfs,
    .flush	= xmp_flush,
    .release	= xmp_release,
    .fsync	= xmp_fsync,
#ifdef HAVE_SETXATTR
    .setxattr	= xmp_setxattr,
    .getxattr	= xmp_getxattr,
    .listxattr	= xmp_listxattr,
    .removexattr= xmp_removexattr,
#endif
    .lock	= xmp_lock,
};

int main(int argc, char *argv[])
{
    char	onfile[MAXPATHLEN];
    int		onfd;

/*  README=fopen("/tmp/read.me","w");  */

    setrlimit(RLIMIT_NOFILE, &rl);
    who = getuid();

    sprintf(onfile,"/.fuse.gunzip.%lli", (long long int) getpid());

    if ((onfd = shm_open(onfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) != -1)
        close(onfd); else
        err(1, "POSIX shared memory required.\n# mkdir -p /dev/shm\n# mount -t tmpfs none /dev/shm");

    if (getenv("FUSE_GUNZIP_SIZE") == NULL)
           cache.size  =  DEFAULT_CACHE_SIZE; else
    if ( (sscanf(getenv("FUSE_GUNZIP_SIZE"), "%lli", &cache.size) != 1) ||
         ( cache.size  <    16 * 1024 * 1024 ) ||
         ( cache.size  >  1024 * 1024 * 1024 ) )
           cache.size  =  DEFAULT_CACHE_SIZE;
    /* 1G > reasonable cache size > 16M */

    cache.avail = cache.size;
    cache.head  = cache.tail = 0;

    pthread_mutex_init(&cache.mtx, NULL);

    umask(0);
/*    return fuse_main(argc, argv, &xmp_oper, NULL); */
    onfd = fuse_main(argc, argv, &xmp_oper, NULL);

    freespace(cache.size);
    shm_unlink(onfile);
    return onfd;
}
