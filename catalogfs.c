/*
 * CatalogFS - Catalog filesystem using FUSE.
 * Copyright 2008 Md Shabeeruddin (shabeer821@gmail.com) 
 * v2008.06.07
 * Consider this code GPLv2.
 *
 * CatalogFS: Basic idea is to view file listings(catalog) of removable storage, as a filesytem. 
 * This allows, firstly to view the catalog as a browsable filesytem, and secondly to use most 
 * of find (unix search utility) capabilities. 
 * Major difference with other filesystems is, actual file contents are not available, 
 * but all possible attributes of the file are available. 
 * Multiple catalogs can be  used simultaneously.
 *
 * No warranties. No guarantees. No lawyers.
 * 
 * I read (and borrowed) a lot of other FUSE code to write this. 
 * Similarities possibly exist- Wholesale reuse as well of other GPL code.
 * 
 * Compile:  gcc -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -pthread -lfuse -lrt -ldl catalogfs.c -o catalogfs
 *
 * Mount: todo: catalogfs list-of-files_ie_catalog mount_point
 *
 * list-of-files_ie_catalog  file can be generated using following command
 * find /removable_storage -printf "%s %u %g %c %m '%p'\n" > list-of-files_ie_catalog
 */
 
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/stat.h>

static int catalog_mknod(const char *path, mode_t mode, dev_t rdev){
	(void)path;
	(void)mode;
	(void)rdev;
	return -EROFS;
}

static int catalog_mkdir(const char *path, mode_t mode){
	(void)path;
	(void)mode;
	return -EROFS;
}

static int catalog_unlink(const char *path){
	(void)path;
	return -EROFS;
}

static int catalog_rmdir(const char *path){
	(void)path;
	return -EROFS;
}

static int catalog_symlink(const char *from, const char *to){
	(void)from;
	(void)to;
	return -EROFS;
}

static int catalog_link(const char *from, const char *to){
	(void)from;
	(void)to;
	return -EROFS;
}

static int catalog_rename(const char *from, const char *to){
	(void)from;
	(void)to;
	return -EROFS;
}

static int catalog_chmod(const char *path, mode_t mode){
	(void)path;
	(void)mode;
	return -EROFS;
}

static int catalog_chown(const char *path,uid_t uid, gid_t gid){
	(void)path;
	(void)uid;
	(void)gid;
	return -EROFS;
}

static int catalog_truncate(const char *path, off_t size){
	(void)path;
	(void)size;
	return -EROFS;
}

static int catalog_utimens(const char *path, const struct timespec tv[2]){
	(void)path;
	(void)tv;
	return -EROFS;
}

static int catalog_release(const char *path, struct fuse_file_info *finfo){
  (void) path;
  (void) finfo;
  return 0;
}

static int catalog_releasedir(const char *path, struct fuse_file_info *finfo){
  (void) path;
  (void) finfo;
  return 0;
}

static int catalog_fsync(const char *path, int throw, struct fuse_file_info *finfo){
  (void) path;
  (void) throw;
  (void) finfo;
  return 0;
}

static int catalog_fsyncdir(const char *path, int throw, struct fuse_file_info *finfo){
	(void) path;
	(void) throw;
	(void) finfo;
	return 0;
}

/*#ifdef HAVE_SETXATTR
// Set the value of an extended attribute 
static int catalog_setxattr(const char *path, const char *name, const char *value, size_t size, int flags){
	(void)path;
	(void)name;
	(void)value;
	(void)size;
	(void)flags;
	return -EPERM;
}

// Get the value of an extended attribute. 
static int catalog_getxattr(const char *path, const char *name, char *value, size_t size){

	(void)path;
	(void)name;
	(void)value;
	(void)size;
	return 0;
}

// List the supported extended attributes. 
static int catalog_listxattr(const char *path, char *list, size_t size){
	(void)path;
	(void)list;
	(void)size;
	return 0;
}

// Remove an extended attribute. 
static int catalog_removexattr(const char *path, const char *name){
	(void)path;
	(void)name;
	return -EPERM;
}

#endif */

/*Check file access permissions
This will be called for the access() system call. If the 'default_permissions' mount option is given, this method is not called.
More ;man access*/
static int catalog_access(const char *path,int mode){
	/* returning 0 , give access to all user */
	(void)path;
	(void)mode;
	return 0;
}

static int catalog_open(const char *path, struct fuse_file_info *finfo){
	(void)path;
	(void)finfo;
	return -EPERM;
}

static int catalog_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo){
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)finfo;
	return -EPERM;
}

static int catalog_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo){
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)finfo;
	return -EROFS;
}

static int catalog_statfs(const char *path, struct statvfs *st_buf){

	(void)path;
	st_buf->f_bsize=0;    /* file system block size */
	st_buf->f_frsize=0;   /* fragment size */
	st_buf->f_blocks=0;   /* size of fs in f_frsize units */
	st_buf->f_bfree=0;    /* # free blocks */
	st_buf->f_bavail=0;   /* # free blocks for non-root */
	st_buf->f_files=0;    /* # inodes */
	st_buf->f_ffree=0;    /* # free inodes */
	st_buf->f_favail=0;   /* # free inodes for non-root */
	st_buf->f_fsid=0;     /* file system ID */
	st_buf->f_flag=ST_RDONLY|ST_NOSUID|ST_NODEV|ST_NOEXEC;     /* mount flags */
	st_buf->f_namemax=255;  /* maximum filename length */

	return 0;
}

char *catalog[]={
			"d 0701 2 1001 1001 1000 1213101162 1213101161 1213113161 /\n",
			"d 0777 2 108 7 3000 1213103162 1213103161 1213111161 /dir1\n",
			"d 0705 2 122 127 6000 1213106162 1213106161 1213108161 /dir2\n",
			"f 0701 1 0 1 2000 1213102162 1213102161 1213112161 /.hidden file 1\n",
			"c 0760 1 111 121 22222 1213112162 1213112161 1213101161 /character special file\n",

			"f 0703 1 1002 1002 4000 1213104162 1213104161 1213110161 /dir1/file 1\n",
			"f 0704 1 41 41 5000 1213105162 1213105161 1213109111 /dir1/file 2\n",
			"f 0726 1 115 125 7000 1213107162 1213107161 1213107161 /dir1/.hidden file 2\n",
			"l 0710 1 119 29 9000 1213109162 1213109161 1213105161 /dir1/link file\n",
			"p 0750 1 110 120 11111 1213111162 1213111161 1213102161 /dir1/fifo file\n",

			
			"f 0747 1 117 126 8000 1213108162 1213108161 1213106161 /dir2/hello 2\n",
			"s 0740 1 113 124 10000 1213110162 1213110161 1213103161 /dir2/socket file\n",

};

//return location of 'path' in the catalog;
static long get_file_location(const char *path){
	int location=0;
	char	file_path[MAXPATHLEN];
	
	for(location=0; location<12;location++){
		//assuming that given directory is listed prior to its inside files/dirs.
		sscanf(catalog[location],"%*s%*s%*s%*s%*s%*s%*s%*s%*s\13%[^\13]s\13",file_path); //\13 is in octal (Vertical Tab).	
		if(strcmp(file_path,path)==0)
			break;
	}
	if(location>=12){
		return -ENOENT;  //todo take care of this return value in caller function.
	}	
	return location;
}

// given full 'path' of a file/dir  , return it's attributes in 'st_data'
static int get_file_entry_from_catalog(const char *path,struct stat *st_data){
	/*todo */
	
	char c;
	long int mode;
	long int nlink;
	long int uid;
	long int gid;
	long long int size;
	long long int atime;
	long long int mtime;
	long long int ctime;
	char	file_path[MAXPATHLEN];
	
//         struct stat {
//               dev_t     st_dev;     /* ID of device containing file */
//               ino_t     st_ino;     /* inode number */
//               mode_t    st_mode;    /* protection :Integer type*/
//               nlink_t   st_nlink;   /* number of hard links :Integer type*/
//               uid_t     st_uid;     /* user ID of owner :Integer type*/
//               gid_t     st_gid;     /* group ID of owner :Integer type*/
//               dev_t     st_rdev;    /* device ID (if special file) */
//               off_t     st_size;    /* total size, in bytes : Signed integer type*/   
//              blksize_t st_blksize; /* blocksize for filesystem I/O */
//               blkcnt_t  st_blocks;  /* number of blocks allocated */
//               time_t    st_atime;   /* time of last access :Integer type*/
//               time_t    st_mtime;   /* time of last modification :Integer type*/
//               time_t    st_ctime;   /* time of last status change :Integer type*/
//           };

	if(st_data==NULL){
		st_data=(struct stat*)malloc(sizeof(struct stat));
	}
 	memset(st_data,0,sizeof(struct stat));
	
	// stat information ,which we have to get from output of 'find' in the catalog listing.
	// find -L /tmp/ -printf "%y %#m %n %U %G %s %A@ %T@ %C@ \11%p\11\n"
	// ls -mlARnQu --color=no --group-directories-first  /tmp


	//todo checks for wrong cases of output in 'entry' variable.
	
	int location=get_file_location(path);
	if(location==-ENOENT || location < 0){
		return -ENOENT;  //todo take care of this return value in caller function.
	}

	sscanf(catalog[location],"%c %lo %ld %ld %ld %lld %lld %lld %lld %[^\13]s\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,file_path); //\13 is in octal (Vertical Tab).
	
	//printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld path=abc%sabc",c,mode,nlink,uid,gid,size,atime,mtime,ctime,file_path); 

	if(strcmp(file_path,path)!=0){
		// both strings are not equal. The entry(in catalog) for the requested path is improper.
		return -ENOENT; //todo take care of this return value in caller function.
	}
	
	switch(c){
		case 'd': mode= S_IFDIR|mode; break;
		case 'l': mode= S_IFLNK|mode; break;
		case 'f': mode= S_IFREG|mode; break;
		case 'b': mode= S_IFBLK|mode; break;
		case 'c': mode= S_IFCHR|mode; break;
		case 'p': mode= S_IFIFO|mode; break;
		//case 'n': mode= S_IFNWK|mode; break;
		//case 'm': mode= S_INSHD|mode; break;
		case 's': mode= S_IFSOCK|mode; break;
	}
	
	st_data->st_mode=mode;  // %y  and %#m
	st_data->st_nlink=nlink;  //%n
	st_data->st_uid=uid; //%U
	st_data->st_gid=gid; //%G
	st_data->st_size=size; //%s
	st_data->st_atime=atime; //%A@
	st_data->st_mtime=mtime; //%T@
	st_data->st_ctime=ctime; //%C@
	
	return 0;//todo take care of this return value in caller function.
}

static int catalog_getattr(const char *path, struct stat *st_data){
/*	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;
	return res;
*/

	memset(st_data,0,sizeof(struct stat));
	return get_file_entry_from_catalog(path,st_data);
}

static int catalog_opendir(const char *path,struct fuse_file_info *fi){
	/*Open directory.This method should check if the open operation is permitted for this directory*/
	(void)path;
	(void)fi;
	return 0;
}

/*static int get_dir_listing(const char *path,struct dirent **direntries){
	
	(void)path;
	(void)direntries;
	return 0; // number of entries in 
}*/

static int catalog_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
	/*todo
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
	return 0; */
	
	//struct dirent **direntries;
	(void) offset;
	(void) fi;
	struct stat *st=NULL;
	
	
	//check for filler returning 1 in cases of buf being full; 
	//filler returns 1 when all of RAM and swap memory is full. 
	//Practically, no need to check for the return value;
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	if(strcmp(path,"/")==0){
		
		get_file_entry_from_catalog("/.hidden file 1",st);
		filler(buf,".hidden file 1",st,0);
		st=NULL;
		get_file_entry_from_catalog("/character special file",st);
		filler(buf,"character special file",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir1",st);
		filler(buf,"dir1",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir2",st);
		filler(buf,"dir2",st,0);
		
	}else if(strcmp(path,"/dir1")==0){
		
		get_file_entry_from_catalog("/dir1/file 1",st);
		filler(buf,"file 1",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir1/file 2",st);
		filler(buf,"file 2",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir1/.hidden file 2",st);
		filler(buf,".hidden file 2",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir1/link file",st);
		filler(buf,"link file",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir1/fifo file",st);
		filler(buf,"fifo file",st,0);
		
	}else if(strcmp(path,"/dir2")==0){
		
		get_file_entry_from_catalog("/dir2/hello 2",st);
		filler(buf,"hello 2",st,0);
		st=NULL;
		get_file_entry_from_catalog("/dir2/socket file",st);
		filler(buf,"socket file",st,0);
		
	}else {
		return -ENOENT;
	}
	
	return 0;
}

static int catalog_readlink(const char *path, char *buf, size_t size){
	/*todo
	int res;
	path=translate_path(path);

	res = readlink(path, buf, size - 1);
	free(path);
	if(res == -1) {
		return -errno;
	}
	buf[res] = '\0';
	return 0;*/
	
	(void)path;
	(void)buf;
	(void)size;
	return 0;
}

// Clean up filesystem. Called on filesystem exit.
static void catalog_destroy(void *dummy){
	/* todo */
	/* cleanup datastructures used */
	(void)dummy;
	return;
}

static struct fuse_operations catalog_operations = {
	.getattr	= catalog_getattr,  /* implement */
	.access	= catalog_access,	/* implemented */
	.destroy	= catalog_destroy,	/* implement */

	.open	= catalog_open,
	.read	= catalog_read,
	.write	= catalog_write,
	.rename	= catalog_rename,
	.statfs	= catalog_statfs,  /* implemented */
	.release	= catalog_release,
	.releasedir=catalog_releasedir,
	.fsync	= catalog_fsync,
	.fsyncdir	= catalog_fsyncdir,

	.opendir	= catalog_opendir, /* implement */
	.readdir	= catalog_readdir, /* implement */
	.mkdir	= catalog_mkdir,
	.rmdir	= catalog_rmdir,

	.link	= catalog_link,
	.readlink	= catalog_readlink, /* implement */
	.symlink	= catalog_symlink,
	.unlink	= catalog_unlink,

	.mknod	= catalog_mknod,
	.chmod	= catalog_chmod,
	.chown	= catalog_chown,
	.utimens	= catalog_utimens,
	.truncate	= catalog_truncate,

/*
#ifdef HAVE_SETXATTR
	.setxattr	= catalog_setxattr,
	.getxattr	= catalog_getxattr, // implement 
	.listxattr= catalog_listxattr,// implement 
	.removexattr= catalog_removexattr,
#endif */
	
};

int main(int argc, char *argv[]){
	return fuse_main(argc,argv,&catalog_operations,NULL);
}
