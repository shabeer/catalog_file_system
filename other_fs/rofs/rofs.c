/*
 * ROFS - The read-only filesystem for FUSE.
 * Copyright 2006 Matthew Keller. kellermg@potsdam.edu and others.
 * v2006.11.28
 * 
 * Mount any filesytem, or folder tree read-only, anywhere else.
 * No warranties. No guarantees. No lawyers.
 * 
 * I read (and borrowed) a lot of other FUSE code to write this. 
 * Similarities possibly exist- Wholesale reuse as well of other GPL code.
 * Special mention to R�mi Flament and his loggedfs.
 * 
 * Consider this code GPLv2.
 * 
 * Compile: gcc -o rofs -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse 
main.c
 * Mount: rofsmount readwrite_filesystem mount_point
 * 
 */


#define FUSE_USE_VERSION 25

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>


// Global to store our read-write path
char *rw_path;

// Translate an rofs path into it's underlying filesystem path
static char* translate_path(const char* path)
{

    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(rw_path)+1));
 
    strcpy(rPath,rw_path);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);

    return rPath;
}


/******************************
 *   
 * Callbacks for FUSE
 * 
 * 
 * 
 ******************************/
 
static int callback_getattr(const char *path, struct stat *st_data)
{
    int res;
	path=translate_path(path);
	
    res = lstat(path, st_data);
    free(path);
    if(res == -1) {
        return -errno;
    }
    return 0;
}

static int callback_readlink(const char *path, char *buf, size_t size)
{
   	int res;
	path=translate_path(path);
	
    res = readlink(path, buf, size - 1);
    free(path);
    if(res == -1) {
        return -errno;
    }
    buf[res] = '\0';
    return 0;
}

static int callback_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct 
fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    int res;

    (void) offset;
    (void) fi;

    path=translate_path(path);

    dp = opendir(path);
    free(path);
    if(dp == NULL) {
        res = -errno;
        return res;
    }

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int callback_mknod(const char *path, mode_t mode, dev_t rdev)
{
  (void)path;
  (void)mode;
  (void)rdev;
  return -EPERM;
}

static int callback_mkdir(const char *path, mode_t mode)
{
  (void)path;
  (void)mode;
  return -EPERM;
}

static int callback_unlink(const char *path)
{
  (void)path;
  return -EPERM;
}

static int callback_rmdir(const char *path)
{
  (void)path;
  return -EPERM;
}

static int callback_symlink(const char *from, const char *to)
{
  (void)from;
  (void)to;
  return -EPERM;	
}

static int callback_rename(const char *from, const char *to)
{
  (void)from;
  (void)to;
  return -EPERM;
}

static int callback_link(const char *from, const char *to)
{
  (void)from;
  (void)to;
  return -EPERM;
}

static int callback_chmod(const char *path, mode_t mode)
{
  (void)path;
  (void)mode;
  return -EPERM;
    
}

static int callback_chown(const char *path, uid_t uid, gid_t gid)
{
  (void)path;
  (void)uid;
  (void)gid;
  return -EPERM;
}

static int callback_truncate(const char *path, off_t size)
{
	(void)path;
  	(void)size;
  	return -EPERM;
}

static int callback_utime(const char *path, struct utimbuf *buf)
{
	(void)path;
  	(void)buf;
  	return -EPERM;	
}

static int callback_open(const char *path, struct fuse_file_info *finfo)
{
	int res;
	
	/* We allow opens, unless they're tring to write, sneaky
	 * people.
	 */
	int flags = finfo->flags;
	
	if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & 
O_TRUNC)) {
      return -errno;
  	}
  	
  	path=translate_path(path);
  
    res = open(path, flags);
 
    free(path);
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

static int callback_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info 
*finfo)
{
    int fd;
    int res;
    (void)finfo;

	path=translate_path(path);
    fd = open(path, O_RDONLY);
    free(path);
    if(fd == -1) {
        res = -errno;
        return res;
    } 
    res = pread(fd, buf, size, offset);
    
    if(res == -1) {
        res = -errno;
	}
    close(fd);
    return res;
}

static int callback_write(const char *path, const char *buf, size_t size, off_t offset, struct 
fuse_file_info *finfo)
{
  (void)path;
  (void)buf;
  (void)size;
  (void)offset;
  (void)finfo;
  return -EPERM;
}

static int callback_statfs(const char *path, struct statvfs *st_buf)
{
  int res;
  path=translate_path(path);
  
  res = statvfs(path, st_buf);
  free(path);
  if (res == -1) {
    return -errno;
  }
  return 0;
}

static int callback_release(const char *path, struct fuse_file_info *finfo)
{
  (void) path;
  (void) finfo;
  return 0;
}

static int callback_fsync(const char *path, int crap, struct fuse_file_info *finfo)
{
  (void) path;
  (void) crap;
  (void) finfo;
  return 0;
}

static int callback_access(const char *path, int mode)
{
	int res;
  	path=translate_path(path);
  	
  	res = access(path, mode);
	free(path);
  	if (res == -1) {
    	return -errno;
  	}
  	return res;
}

/*
 * Set the value of an extended attribute
 */
static int callback_setxattr(const char *path, const char *name, const char *value, size_t size, int 
flags)
{
	(void)path;
	(void)name;
	(void)value;
	(void)size;
	(void)flags;
	return -EPERM;
}

/*
 * Get the value of an extended attribute.
 */
static int callback_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int res;
    
    path=translate_path(path);
    res = lgetxattr(path, name, value, size);
    free(path);
    if(res == -1) {
        return -errno;
    }
    return res;
}

/*
 * List the supported extended attributes.
 */
static int callback_listxattr(const char *path, char *list, size_t size)
{
	int res;
	
	path=translate_path(path);
	res = llistxattr(path, list, size);
   	free(path);
    if(res == -1) {
        return -errno;
    }
    return res;

}

/*
 * Remove an extended attribute.
 */
static int callback_removexattr(const char *path, const char *name)
{
	(void)path;
  	(void)name;
  	return -EPERM;

}

struct fuse_operations callback_oper = {
    .getattr	= callback_getattr,
    .readlink	= callback_readlink,
    .readdir	= callback_readdir,
    .mknod		= callback_mknod,
    .mkdir		= callback_mkdir,
    .symlink	= callback_symlink,
    .unlink		= callback_unlink,
    .rmdir		= callback_rmdir,
    .rename		= callback_rename,
    .link		= callback_link,
    .chmod		= callback_chmod,
    .chown		= callback_chown,
    .truncate	= callback_truncate,
    .utime		= callback_utime,
    .open		= callback_open,
    .read		= callback_read,
    .write		= callback_write,
    .statfs		= callback_statfs,
    .release	= callback_release,
    .fsync		= callback_fsync,
    .access		= callback_access,

    /* Extended attributes support for userland interaction */
    .setxattr	= callback_setxattr,
    .getxattr	= callback_getxattr,
    .listxattr	= callback_listxattr,
    .removexattr= callback_removexattr
};

int main(int argc, char *argv[])
{
	
	rw_path = getenv("ROFS_RW_PATH");
  	if (!rw_path)
    {
      fprintf(stderr, "ROFS_RW_PATH not defined in environment.\n");
      exit(1);
    }
	
    
    fuse_main(argc, argv, &callback_oper);
    return 0;
}
