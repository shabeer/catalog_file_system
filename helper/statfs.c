#include <sys/statvfs.h>
#include <sys/types.h>
//#include <bits/statvfs.h>
#include <stdio.h>
#include <sys/param.h>			/* for MAXPATHLEN */

int main(){
    //struct statvfs  stbuf;
	//struct stat st_data;
    //statvfs("/home/extra",&stbuf);
//    unsigned long  f_bsize;    /* file system block size */
//    unsigned long  f_frsize;   /* fragment size */
//    fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
//    fsblkcnt_t     f_bfree;    /* # free blocks */
//    fsblkcnt_t     f_bavail;   /* # free blocks for non-root */
//    fsfilcnt_t     f_files;    /* # inodes */
//    fsfilcnt_t     f_ffree;    /* # free inodes */
//    fsfilcnt_t     f_favail;   /* # free inodes for non-root */
//    unsigned long  f_fsid;     /* file system ID */
//    unsigned long  f_flag;     /* mount flags */
//    unsigned long  f_namemax;  /* maximum filename length */
   /*printf("\n f_bsize=%u",stbuf.f_bsize);
   printf("\n f_frsize=%u",stbuf.f_frsize);
   printf("\n f_blocks=%u",stbuf.f_blocks);
   printf("\n f_bfree=%u",stbuf.f_bfree);
   printf("\n f_bavail=%u",stbuf.f_bavail);
   printf("\n f_files=%u",stbuf.f_files);
   printf("\n f_ffree=%u",stbuf.f_ffree);
   printf("\n f_favail=%u",stbuf.f_favail);
   printf("\n f_fsid=%u",stbuf.f_fsid);
   printf("\n f_flag=%x",stbuf.f_flag);
   printf("\n f_namemax=%u",stbuf.f_namemax);
   printf("\n %d",ST_RDONLY|ST_NOSUID|ST_NODEV|ST_NOEXEC); */ 

   //lstat("/home",st_data);

//         struct stat {
//               dev_t     st_dev;     /* ID of device containing file */
//               ino_t     st_ino;     /* inode number */
//               mode_t    st_mode;    /* protection */
//               nlink_t   st_nlink;   /* number of hard links */
//               uid_t     st_uid;     /* user ID of owner */
//               gid_t     st_gid;     /* group ID of owner */
//               dev_t     st_rdev;    /* device ID (if special file) */
//               off_t     st_size;    /* total size, in bytes */
//              blksize_t st_blksize; /* blocksize for filesystem I/O */
//               blkcnt_t  st_blocks;  /* number of blocks allocated */
//               time_t    st_atime;   /* time of last access */
//               time_t    st_mtime;   /* time of last modification */
//               time_t    st_ctime;   /* time of last status change */
//           };

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
	int i=10; //<11

	char *entry[]={"d 0755 2 1001 1001 1000 1213101162 1213101161 1213113161 /\n",
				"f 0701 1 0 1 2000 1213102162 1213102161 1213112161 /.hidden file 1\n",
				"d 0702 2 108 7 3000 1213103162 1213103161 1213111161 /dir1\n",
				"f 0703 1 1002 1002 4000 1213104162 1213104161 1213110161 /dir1/file 1\n",
				"f 0704 1 41 41 5000 1213105162 1213105161 1213109111 /dir1/file 2\n",
				"d 0705 2 122 127 6000 1213106162 1213106161 1213108161 /dir2\n",
				"f 0726 1 115 125 7000 1213107162 1213107161 1213107161 /dir1/.hidden file 2\n",
				"f 0747 1 117 126 8000 1213108162 1213108161 1213106161 /dir2/hello 2\n",
				"l 0710 1 119 29 9000 1213109162 1213109161 1213105161 /dir1/link file\n",
				"s 0740 1 113 124 10000 1213110162 1213110161 1213103161 /dir2/semaphore file\n",
				"p 0750 1 110 120 11111 1213111162 1213111161 1213102161 /dir1/fifo file\n",
				"c 0760 1 111 121 22222 1213112162 1213112161 1213101161 /character special file\n"
	};
	
	sscanf(entry[i],"%c %lo %ld %ld %ld %lld %lld %lld %lld %[^\13]s\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,file_path); //\13 is in octal (Vertical Tab).
	printf("%s\n",entry[i]);
	printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld path=abc%sabc",c,mode,nlink,uid,gid,size,atime,mtime,ctime,file_path);

}
