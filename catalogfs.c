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
#include <unistd.h>

static int entry_size=84;
static int catalog_len=15;
/*----------------------------------------------------------------------------------------*/
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

/*Check file access permissions.This will be called for the access() system call. 
If the 'default_permissions' mount option is given, this method is not called. More ;man access*/
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

/*----------------------------------------------------------------------------------------*/


typedef struct stat_with_depth_filepath{
	int depth;
	char	*file_path;
	struct stat st_data;
}stat_wdf;

static long int calculateMid(long int low,long int high){
	return low+(high-low)/2;
}
	
static int findDepth(char *path){
	// returns depth of path . 
	// depth = number of "/" in the path
	// limitation: path should NOT contain escaped "/"
	int path_len=strlen(path);
	int count=0;
	int i;
	
	if(path==NULL || path[0]!='/'){
		return -1;
	}
	
	for(i=0;i<path_len;i++){
		if(path[i]=='/')
			count++;
	}
	return count; //
}

// given a location of a file/dir  , return it's attributes in 'st_data'
static int getEntry_from_catalog(long int location,stat_wdf *st_wdf){
	// presumption:  location is valid.
	
	char c;
	long int mode;
	long int nlink;
	long int uid;
	long int gid;
	long long int size;
	long long int atime;
	long long int mtime;
	long long int ctime;
	int depth;
	char	file_path[MAXPATHLEN];
	
//         struct stat {
//               dev_t     st_dev;     // ID of device containing file 
//               ino_t     st_ino;     // inode number 
//               mode_t    st_mode;    // protection :Integer type
//               nlink_t   st_nlink;   // number of hard links :Integer type
//               uid_t     st_uid;     // user ID of owner :Integer type
//               gid_t     st_gid;     / group ID of owner :Integer type
//               dev_t     st_rdev;    // device ID (if special file) 
//               off_t     st_size;    // total size, in bytes : Signed integer type
//              blksize_t st_blksize; // blocksize for filesystem I/O 
//               blkcnt_t  st_blocks;  // number of blocks allocated 
//               time_t    st_atime;   // time of last access :Integer type
//               time_t    st_mtime;   // time of last modification :Integer type
//               time_t    st_ctime;   // time of last status change :Integer type
//           };

	// stat information ,which we have to get from output of 'find' in the catalog listing.
	// find -L /tmp/ -printf "%y %#m %n %U %G %s %A@ %T@ %C@ \11%p\11\n"
	// ls -mlARnQu --color=no --group-directories-first  /tmp


/*	//toodo checks for wrong cases of output in 'entry' variable.
	
	int location=get_file_location(path);
	if(location==-ENOENT || location < 0){
	return -ENOENT;  //toodo take care of this return value in caller function.
}*/

	int fd;
	char *catalogFile="catalog.list";
	
	char errormsg[]="\nCannot open catalog file: ";
	char entry[entry_size+1];
	
//	strcat(errormsg,catalogFile);  //stack smashing problem.
	
	//fd=open("catalog.list",O_RDONLY,0);
	fd=open(catalogFile,O_RDONLY,0);
	if(fd==-1){
		perror(errormsg);
		exit(-1);
	}
	// read nbyte=85 bytes from the filedes into entry
	
	int offset=(entry_size)*location;
	
	//printf(" offset=%d\n",offset);
	if(lseek(fd,offset,SEEK_SET)<0){
		perror("Error in reading entry(file seek) from catalog ");
		return -1;
	}
	
	if(read(fd,entry,entry_size)==-1){
		perror("Error in reading entry from catalog ");
		return -1;
	}
	
	close(fd);
	entry[entry_size]='\0';
	//printf(":entry=%s:\n",entry);
	//sscanf(catalog[location],"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);
	//sscanf(entry,"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);  
	
	sscanf(entry,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%[^\1]s\1\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,&depth,file_path); //\1 is used as separator.
	//\13 is in octal (Vertical Tab).
	
	//printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld depth=%d  path=abc%sabc\n",c,mode,nlink,uid,gid,size,atime,mtime,ctime,depth,file_path); 

/*	if(strcmp(file_path,path)!=0){
		// both strings are not equal. The entry(in catalog) for the requested path is improper.
	return -ENOENT; //toodo take care of this return value in caller function.
}*/
	
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
	
	st_wdf->st_data.st_mode=mode;  // %y  and %#m
	st_wdf->st_data.st_nlink=nlink;  //%n
	st_wdf->st_data.st_uid=uid; //%U
	st_wdf->st_data.st_gid=gid; //%G
	st_wdf->st_data.st_size=size; //%s
	st_wdf->st_data.st_atime=atime; //%A@
	st_wdf->st_data.st_mtime=mtime; //%T@
	st_wdf->st_data.st_ctime=ctime; //%C@
	
	st_wdf->depth=depth;
	//printf("%d\n",st_wdf->depth);
	st_wdf->file_path=file_path;
	//printf("%s\n",st_wdf->file_path);
				
	return 0;//take care of this return value in caller function.
}
					   
// Get Entry at (input)location(line number in catalog file).
// Has side effects, depth and file_path will be appropriately set. "depth, file_path" are output arguments.
static int getEntry_depth_filepath(long int location,int *depth,char *file_path){

/*	int fd;
	char *catalogFile="catalog.list";
	
	char errormsg[]="\nCannot open catalog file: ";
	char entry[entry_size+1];
	
//	strcat(errormsg,catalogFile);  //stack smashing problem.
	
	fd=open("catalog.list",O_RDONLY,0);
	if(fd==-1){
	perror(errormsg);
	exit(-1);
}
	// read nbyte=85 bytes from the filedes into entry
	
	int offset=(entry_size)*location;
	
	//printf(" offset=%d\n",offset);
	if(lseek(fd,offset,SEEK_SET)<0){
	perror("Error in reading entry from catalog ");
	exit(-1);
}
	
	if(read(fd,entry,entry_size)==-1){
	perror("Error in reading entry from catalog ");
	exit(-1);
}
	
	close(fd);
	entry[entry_size]='\0';
	//printf(":entry=%s:\n",entry);
	//sscanf(catalog[location],"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);
	sscanf(entry,"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);  
*/	
	
	stat_wdf *st_wdf=NULL;
	
	if(st_wdf==NULL){
		st_wdf=(stat_wdf*)malloc(sizeof(stat_wdf));
		//printf("address of st_wdf %u\n",st_wdf);
	}
	memset(st_wdf,0,sizeof(stat_wdf));
	
	int ret_val=getEntry_from_catalog(location,st_wdf);
	if(ret_val<0)
		return -1;
	//printf("addr of st_wdf %u\n",st_wdf);
	//printf("in after1 getEntry_depth_filepath=%u\n",&(st_wdf->depth));
	*depth=st_wdf->depth;
	//printf("in after2 getEntry_depth_filepath\n");
	strcpy(file_path,st_wdf->file_path);
	//printf("in after3 getEntry_depth_filepath %d,%s\n",location,file_path);
	return 0;
}

// Has side effects,file_path will be appropriately set.
static int getEntry_file_path(long int location,char *file_path){
	int depth;
	return getEntry_depth_filepath(location,&depth,file_path);
	//printf("in getEntry_file_path %d,%s\n",location,file_path);

}

//finds an entry which has same depth as search_path.
// Has side effects, low,high,mid,depth are modified in this function.
static int find_index(int searchPath_depth,long int *low,long int *high,long int *mid,int *depth){
	
	char file_path[MAXPATHLEN];
	int ret_val;
	while(*low<=*high){ //binary search
		*mid=calculateMid(*low,*high);
		//printf("First loop mid=%ld,low=%ld,high=%ld\n",*mid,*low,*high);
		
		ret_val=getEntry_depth_filepath(*mid,depth,file_path);
		if(ret_val<0)
			return -1;
		
		if(*depth==searchPath_depth){
			break;
		}
		else if(*depth<searchPath_depth){
			*low=*mid+1;
		}else if(*depth>searchPath_depth){
			*high=*mid-1;
		}
	}
	return 0;
}	

static int find_lowest_index(int searchPath_depth,long int *low,long int *mid,int *depth){
	//find the lowest index of catalog , having same depth as that of searchPath.
	
	int low_index_with_same_depth=*mid;
	char file_path[MAXPATHLEN];
	int ret_val;
	
	while(*depth==searchPath_depth){ 
		low_index_with_same_depth--;	
		if(low_index_with_same_depth<*low)
			break;
		ret_val=getEntry_depth_filepath(low_index_with_same_depth,depth,file_path);
		if(ret_val<0)
			return -1;
	}
	low_index_with_same_depth++;
	*low=low_index_with_same_depth;
	return 0;
}

static int find_highest_index(int searchPath_depth,long int *high,long int *mid,int *depth){
	//find the highest index of catalog , having same depth as that of searchPath.
	
	char file_path[MAXPATHLEN];
	int high_index_with_same_depth=*mid;		
	int ret_val;
	while(*depth==searchPath_depth){
		high_index_with_same_depth++;			
		if(high_index_with_same_depth>*high)
			break;
		ret_val=getEntry_depth_filepath(high_index_with_same_depth,depth,file_path);
		if(ret_val<0)
			return -1;
	}
	high_index_with_same_depth--;
	*high=high_index_with_same_depth;
	return 0;
}

//return's location of the 'searchPath' input
static int binSearch(char *searchPath){
	long int low=0,high=catalog_len-1,mid;
	char file_path[MAXPATHLEN];
	int comp_result=0,no_of_comp=0;
	int depth=1,result=-1;
	int ret_val;
	int searchPath_depth=findDepth(searchPath);
	
	if(searchPath_depth<=0)
		return result;
	
	
	ret_val=find_index(searchPath_depth,&low,&high,&mid,&depth);
	if(ret_val<0)
		return -1;
	
	//printf("Passed first loop low=%ld,high=%ld\n",low,high);
	if(low>high){
		result=-1;	
	}
	else if(low<=high){ 
		// Here pre condition is that depth = searchPath_depth. Ensured by find_index()
		ret_val=find_lowest_index(searchPath_depth,&low,&mid,&depth);
		if(ret_val<0)
			return -1;
		
		depth=searchPath_depth;
		
		ret_val=find_highest_index(searchPath_depth,&high,&mid,&depth);
		if(ret_val<0)
			return -1;
		
		//printf("Bounds for same depth %d, low=%ld,high=%ld\n",searchPath_depth,low,high);
		
		while(low<=high){  //now we in entries bounded by low,high  with same depth as searchPath.
			mid=calculateMid(low,high);
			//printf("mid=%ld,low=%ld,high=%ld\n",mid,low,high);
		
			ret_val=getEntry_file_path(mid,file_path);
			if(ret_val<0)
				return -1;
		
			comp_result=strcmp(file_path,searchPath);
			//printf("file_path=%s, searchPath=%s,comp_result=%d\n",file_path,searchPath,comp_result);
			no_of_comp++;
			if(comp_result==0){
			//found match:  searchPath is a equal to catalog[mid].
				result=mid; 
				break;
			}else if(comp_result<0){
				low=mid+1;
			}else if(comp_result>0){
				high=mid-1;
			}
			//printf("mid=%ld,low=%ld,high=%ld\n",mid,low,high);
		}
		//printf("No. of comparisons made=%d\n",no_of_comp);
		
	}
	return result;	
}
	
// Gets lowest and highest locations for a directory's(file_path) immediate contents. Not recursive contents.
static int get_directory_contents(char *file_path,long int *low, long int *high){
	int len=strlen(file_path);
	char entry_file_path[MAXPATHLEN];
	int i,depth,entry_depth;
	int ret_val;
	
	//printf(" len of %s = %d\n",file_path,len);
	//check if file_path is a directory. 
	//File_path is a directory if ending with '/'

	if(len<=0 || file_path[len-1]!='/'){
		*low=*high=-1;
		return -1; //error , not a directory.
	}
	
	*low=binSearch(file_path);
	if(*low<0){
		return -ENOENT;
	}
		
	//printf("low=%ld\n",*low);
	depth=findDepth(file_path);
	
	if(depth<=0)
		return -ENOENT;
	
	i=*low;
	do{
		i++;
		if(i>=catalog_len)
			break;
		ret_val=getEntry_depth_filepath(i,&entry_depth,entry_file_path);
		if(ret_val<0)
			return -ENOENT;
// 		// Entry of Content should have same depth and entry_path should prefixed by the parent path.
		if(entry_depth !=depth || strncmp(file_path,entry_file_path,len)!=0)
			break;
		
	}while(1);
	
	*high = i-1;	
	printf("directory contents indices for file_path=%s, low=%ld, high=%ld\n",file_path,*low,*high);
	return 0;
}
	
//return location of 'path' in the catalog;
/*static long get_file_location(const char *path){
	int location=0;
	char	file_path[MAXPATHLEN];
	
	for(location=0; location<12;location++){
		//assuming that given directory is listed prior to its inside files/dirs.
		sscanf(catalog[location],"%*s%*s%*s%*s%*s%*s%*s%*s%*s\13%[^\13]s\13",file_path); //\13 is in octal (Vertical Tab).	
		if(strcmp(file_path,path)==0)
			break;
	}
	if(location>=12){
		return -ENOENT;  //toodo take care of this return value in caller function.
	}	
	return location;
}*/

// given full location of a file/dir  , return it's attributes in 'st_data'
static int getattr_by_location(long int location,struct stat *st_data){
	
	int ret_val;
	stat_wdf st_wdf;
	ret_val=getEntry_from_catalog(location,&st_wdf);
	if(ret_val<0)
		return -ENOENT;
	
	//put st_wdf.st_data in *st_data;
	
	st_data->st_mode   = st_wdf.st_data.st_mode;
	st_data->st_nlink  = st_wdf.st_data.st_nlink;
	st_data->st_uid    = st_wdf.st_data.st_uid;
	st_data->st_gid    = st_wdf.st_data.st_gid;
	st_data->st_size   = st_wdf.st_data.st_size;
	st_data->st_atime  = st_wdf.st_data.st_atime;
	st_data->st_mtime  = st_wdf.st_data.st_mtime;
	st_data->st_ctime  = st_wdf.st_data.st_ctime;
	
	return 0;
}

// given full 'path' of a file/dir  , return it's attributes in 'st_data'
static int getattr_by_path(const char *path,struct stat *st_data){
	long int location;
	int ret_val;
		
	//determine location of 'path'
	location=binSearch((char *)path);
	if(location<0)
		return -ENOENT;
	
	ret_val=getattr_by_location(location,st_data);
	if(ret_val<0)
		return -ENOENT;
	
	return 0;
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
	return getattr_by_path(path,st_data);
}

/*
static int get_file_entry_from_catalog(const char *path,struct stat *st_data){
	
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
//               dev_t     st_dev;     // ID of device containing file 
//               ino_t     st_ino;     // inode number 
//               mode_t    st_mode;    // protection :Integer type
//               nlink_t   st_nlink;   // number of hard links :Integer type
//               uid_t     st_uid;     // user ID of owner :Integer type
//               gid_t     st_gid;     / group ID of owner :Integer type
//               dev_t     st_rdev;    // device ID (if special file) 
//               off_t     st_size;    // total size, in bytes : Signed integer type
//              blksize_t st_blksize; // blocksize for filesystem I/O 
//               blkcnt_t  st_blocks;  // number of blocks allocated 
//               time_t    st_atime;   // time of last access :Integer type
//               time_t    st_mtime;   // time of last modification :Integer type
//               time_t    st_ctime;   // time of last status change :Integer type
//           };

	if(st_data==NULL){
		st_data=(struct stat*)malloc(sizeof(struct stat));
	}
 	memset(st_data,0,sizeof(struct stat));
	
	// stat information ,which we have to get from output of 'find' in the catalog listing.
	// find -L /tmp/ -printf "%y %#m %n %U %G %s %A@ %T@ %C@ \11%p\11\n"
	// ls -mlARnQu --color=no --group-directories-first  /tmp


	//toodo checks for wrong cases of output in 'entry' variable.
	
	int location=get_file_location(path);
	if(location==-ENOENT || location < 0){
		return -ENOENT;  //toodo take care of this return value in caller function.
	}

	sscanf(catalog[location],"%c %lo %ld %ld %ld %lld %lld %lld %lld %[^\13]s\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,file_path); //\13 is in octal (Vertical Tab).
	
	//printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld path=abc%sabc",c,mode,nlink,uid,gid,size,atime,mtime,ctime,file_path); 

	if(strcmp(file_path,path)!=0){
		// both strings are not equal. The entry(in catalog) for the requested path is improper.
		return -ENOENT; //toodo take care of this return value in caller function.
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
	
	return 0;//toodo take care of this return value in caller function.
}
*/

static int catalog_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
	/*toodo
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
	
	//check for filler returning 1 in cases of buf being full; 
	//filler returns 1 when all of RAM and swap memory is full. 
	//Practically, no need to check for the return value;
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	
	long int low_index,high_index; //lowest and highest location index of directory in the catalog.
	int location,ret_val,ret_val2;

	// get_directory_contents , all directory paths should end by a slash "/". 
	if(strcmp(path,"/")!=0){
		strcat((char *)path,"/");
	}
	
	ret_val=get_directory_contents((char*)path,&low_index,&high_index);
	
	if(ret_val<0 || low_index<0 || high_index<0){
		//printf("Error: Did not enter for loop\n");
		return -ENOENT;
	}
			
	//location starts from one location ahead of low_index, since contents start from low_index+1
	for(location=low_index+1;location<=high_index;location++){
		struct stat st;
		char pathname[MAXPATHLEN];
		memset(&st, 0, sizeof(st));
		
		//printf("Entered for loop\n");
		ret_val=getattr_by_location(location,&st);
		ret_val2=getEntry_file_path(location,pathname);
		
		//printf("path=%s, size=%d\n",pathname,(int)st.st_size);
		
			
		if(ret_val>=0 && ret_val2>=0){
			//printf("Enter if stmt: pathname=%s\n",pathname);
			if (filler(buf, basename(pathname), &st, 0))
				break;
		}
	}
	
/*
	struct stat st;
	memset(&st, 0, sizeof(st));
	if(strcmp(path,"/")==0){
		getattr_by_path("/.hidden file 1",&st);
		filler(buf,".hidden file 1",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/character special file",&st);
		filler(buf,"character special file",&st,0);	
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/dir1",&st);
		filler(buf,"dir1",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/.dir2",&st);
		filler(buf,".dir2",&st,0);
		
	}else if(strcmp(path,"/dir1")==0){
		
		getattr_by_path("/dir1/file 1",&st);
		filler(buf,"file 1",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/dir1/file 2",&st);
		filler(buf,"file 2",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/dir1/.hidden file 2",&st);
		filler(buf,".hidden file 2",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/dir1/link file",&st);
		filler(buf,"link file",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/dir1/fifo file",&st);
		filler(buf,"fifo file",&st,0);
		
	}else if(strcmp(path,"/.dir2")==0){
		
		getattr_by_path("/.dir2/hello 2",&st);
		filler(buf,"hello 2",&st,0);
		
		memset(&st, 0, sizeof(st));
		getattr_by_path("/.dir2/semaphore file",&st);
		filler(buf,"semaphore file",&st,0);
		
	}else {
		return -ENOENT;
	}*/
		
	return 0;
}




static int catalog_readlink(const char *path, char *buf, size_t size){
	/*int res;
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
	
	//todo
	//printf("size=%d\n",size);
	strcpy(buf,"../.dir2");
	
	
	//return -ENOENT;
	return 0;
}

// Clean up filesystem. Called on filesystem exit.
static void catalog_destroy(void *dummy){
	// todo 
	// cleanup datastructures used 
	(void)dummy;
	return;
}

static struct fuse_operations catalog_operations = {
	.getattr	= catalog_getattr,  /* implemented */
	.access	= catalog_access,	/* implemented */
	.destroy	= catalog_destroy,	/* implemented */

	.open	= catalog_open,
	.read	= catalog_read,
	.write	= catalog_write,
	.rename	= catalog_rename,
	.statfs	= catalog_statfs,  /* implemented */
	.release	= catalog_release,
	.releasedir=catalog_releasedir,
	.fsync	= catalog_fsync,
	.fsyncdir	= catalog_fsyncdir,

	.opendir	= catalog_opendir, /* implemented */
	.readdir	= catalog_readdir, /* implemented */
	.mkdir	= catalog_mkdir,
	.rmdir	= catalog_rmdir,

	.link	= catalog_link,
	.readlink	= catalog_readlink, /* implemented */
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
