/* This program implements binary search on a file(record separator=new line) with duplicates.*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/stat.h>

#define ENTRY_SIZE 84
/*includes a newline character at end of each line*/

void showUsage(){fprintf( stderr, "Usage: binsearchinfile <sorted filename>\n" );fprintf( stderr, "Usage: Each line should be of same size in bytes. \n" );fprintf( stderr, "Usage: New line is the record separator. \n" );}

typedef struct stat_with_depth_filepath{
	int depth;
	char	*file_path;
	struct stat st_data;
}stat_wdf;char *catalog[]={
"d 0755 2 1001 1001 1000 1213101162 1213101161 1213113161 1 /\n",
"f 0701 1 0 1 2000 1213102162 1213102161 1213112161 1 /.hidden file 1\n",
"c 0760 1 111 121 22222 1213112162 1213112161 1213101161 1 /character special file\n",
"d 0702 2 108 7 3000 1213103162 1213103161 1213111161 1 /dir1\n",
"d 0705 2 122 127 6000 1213106162 1213106161 1213108161 1 /dir2\n",
"d 0702 2 108 7 3000 1213103162 1213103161 1213111161 2 /dir1/\n",
"f 0726 1 115 125 7000 1213107162 1213107161 1213107161 2 /dir1/.hidden file 2\n",
"p 0750 1 110 120 11111 1213111162 1213111161 1213102161 2 /dir1/fifo file\n",
"f 0703 1 1002 1002 4000 1213104162 1213104161 1213110161 2 /dir1/file 1\n",
"f 0704 1 41 41 5000 1213105162 1213105161 1213109111 2 /dir1/file 2\n",
"l 0710 1 119 29 9000 1213109162 1213109161 1213105161 2 /dir1/link file\n",
"d 0705 2 122 127 6000 1213106162 1213106161 1213108161 2 /dir2/\n",
"f 0747 1 117 126 8000 1213108162 1213108161 1213106161 2 /dir2/hello 2\n",
"s 0740 1 113 124 10000 1213110162 1213110161 1213103161 2 /dir2/semaphore file\n"
};
	
long int calculateMid(long int low,long int high){
	return low+(high-low)/2;
}
	
int findDepth(char *path){
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
int getEntry_from_catalog(long int location,stat_wdf *st_wdf){
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

	// stat information ,which we have to get from output of 'find' in the catalog listing.
	// find -L /tmp/ -printf "%y %#m %n %U %G %s %A@ %T@ %C@ \11%p\11\n"
	// ls -mlARnQu --color=no --group-directories-first  /tmp


/*	//todo checks for wrong cases of output in 'entry' variable.
	
	int location=get_file_location(path);
	if(location==-ENOENT || location < 0){
		return -ENOENT;  //todo take care of this return value in caller function.
	}*/

	int fd;
	char *catalogFile="catalog.list";
	
	char errormsg[]="\nCannot open catalog file: ";
	char entry[ENTRY_SIZE+1];
	
//	strcat(errormsg,catalogFile);  //stack smashing problem.
	
	fd=open("catalog.list",O_RDONLY,0);
	if(fd==-1){
		perror(errormsg);
		exit(-1);
	}
	// read nbyte=85 bytes from the filedes into entry
	
	int offset=(ENTRY_SIZE)*location;
	
	//printf(" offset=%d\n",offset);
	if(lseek(fd,offset,SEEK_SET)<0){
		perror("Error in reading entry from catalog ");
		exit(-1);
	}
	
	if(read(fd,entry,ENTRY_SIZE)==-1){
		perror("Error in reading entry from catalog ");
		exit(-1);
	}
	
	close(fd);
	entry[ENTRY_SIZE]='\0';
	//printf(":entry=%s:\n",entry);
	//sscanf(catalog[location],"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);
	//sscanf(entry,"%*s%*s%*s%*s%*s%*s%*s%*s%*s%d\13%[^\13]s\13",depth,file_path);  
	
	sscanf(entry,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \13%[^\13]s\13\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,&depth,file_path); //\13 is in octal (Vertical Tab).
	
	//printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld depth=%d  path=abc%sabc\n",c,mode,nlink,uid,gid,size,atime,mtime,ctime,depth,file_path); 

/*	if(strcmp(file_path,path)!=0){
		// both strings are not equal. The entry(in catalog) for the requested path is improper.
		return -ENOENT; //todo take care of this return value in caller function.
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
				
	return 0;//todo take care of this return value in caller function.
}
					   
// Get Entry at (input)location(line number in catalog file).
// Has side effects, depth and file_path will be appropriately set. "depth, file_path" are output arguments.
int getEntry_depth_filepath(long int location,int *depth,char *file_path){

/*	int fd;
	char *catalogFile="catalog.list";
	
	char errormsg[]="\nCannot open catalog file: ";
	char entry[ENTRY_SIZE+1];
	
//	strcat(errormsg,catalogFile);  //stack smashing problem.
	
	fd=open("catalog.list",O_RDONLY,0);
	if(fd==-1){
		perror(errormsg);
		exit(-1);
	}
	// read nbyte=85 bytes from the filedes into entry
	
	int offset=(ENTRY_SIZE)*location;
	
	//printf(" offset=%d\n",offset);
	if(lseek(fd,offset,SEEK_SET)<0){
		perror("Error in reading entry from catalog ");
		exit(-1);
	}
	
	if(read(fd,entry,ENTRY_SIZE)==-1){
		perror("Error in reading entry from catalog ");
		exit(-1);
	}
	
	close(fd);
	entry[ENTRY_SIZE]='\0';
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
	
	getEntry_from_catalog(location,st_wdf);
	//printf("addr of st_wdf %u\n",st_wdf);
	//printf("in after1 getEntry_depth_filepath=%u\n",&(st_wdf->depth));
	*depth=st_wdf->depth;
	//printf("in after2 getEntry_depth_filepath\n");
	strcpy(file_path,st_wdf->file_path);
	//printf("in after3 getEntry_depth_filepath %d,%s\n",location,file_path);

}

// Has side effects,file_path will be appropriately set.
int getEntry_file_path(long int location,char *file_path){
	int depth;
	getEntry_depth_filepath(location,&depth,file_path);
	//printf("in getEntry_file_path %d,%s\n",location,file_path);

}

int get_directory_contents(char *file_path,long int *low, long int *high){
	// Gets lowest and highest locations for a directory's(file_path) immediate contents. Not recursive contents.
	
	//check if file_path is a directory. 
	//File_path is a directory if ending with '/'
	int len=strlen(file_path);
	char entry_file_path[MAXPATHLEN];
	int i,depth,entry_depth;
	
	printf(" len of %s = %d\n",file_path,len);
	if(len<=0 || file_path[len-1]!='/'){
		*low=*high=-1;
		return -1; //error , not a directory.
	}
	
	*low=binSearch(file_path);
	printf("low=%d\n",*low);
	depth=findDepth(file_path);
	
	i=*low;
	do{
		i++;
		if(i>=14)
			break;
		getEntry_depth_filepath(i,&entry_depth,entry_file_path);
// 		// Entry of Content should have same depth and entry_path should prefixed by the parent path.
		if(entry_depth !=depth || strncmp(file_path,entry_file_path,len)!=0)
			break;
		
	}while(1);
	
	*high = i-1;	
	
	return 0;
}

//finds an entry which has same depth as search_path.
// Has side effects, low,high,mid,depth are modified in this function.
int find_index(int searchPath_depth,long int *low,long int *high,long int *mid,int *depth){
	
	char file_path[MAXPATHLEN];
	while(*low<=*high){ //binary search
		*mid=calculateMid(*low,*high);
		printf("First loop mid=%d,low=%d,high=%d\n",*mid,*low,*high);
		
		getEntry_depth_filepath(*mid,depth,file_path);
		
		if(*depth==searchPath_depth){
			break;
		}
		else if(*depth<searchPath_depth){
			*low=*mid+1;
		}else if(*depth>searchPath_depth){
			*high=*mid-1;
		}
	}
	return 1;
}	

int find_lowest_index(int searchPath_depth,long int *low,long int *mid,int *depth){
	//find the lowest index of catalog , having same depth as that of searchPath.
	
	int low_index_with_same_depth=*mid;
	char file_path[MAXPATHLEN];
	
	while(*depth==searchPath_depth){ 
		low_index_with_same_depth--;	
		if(low_index_with_same_depth<*low)
			break;
		getEntry_depth_filepath(low_index_with_same_depth,depth,file_path);
	}
	low_index_with_same_depth++;
	*low=low_index_with_same_depth;
}

int find_highest_index(int searchPath_depth,long int *high,long int *mid,int *depth){
	//find the highest index of catalog , having same depth as that of searchPath.
	
	char file_path[MAXPATHLEN];
	int high_index_with_same_depth=*mid;		
	
	while(*depth==searchPath_depth){
		high_index_with_same_depth++;			
		if(high_index_with_same_depth>*high)
			break;
		getEntry_depth_filepath(high_index_with_same_depth,depth,file_path);
	}
	high_index_with_same_depth--;
	*high=high_index_with_same_depth;
}

int binSearch(char *searchPath){
	long int low=0,high=13,mid;
	char file_path[MAXPATHLEN];
	int comp_result=0,no_of_comp=0;
	int depth=1,result=-1;
	
	int searchPath_depth=findDepth(searchPath);
	
	if(searchPath_depth<=0)
		return result;
	
	
	find_index(searchPath_depth,&low,&high,&mid,&depth);
	
	printf("Passed first loop low=%d,high=%d\n",low,high);
	if(low>high){
		result=-1;	
	}
	else if(low<=high){ 
		// Here pre condition is that depth = searchPath_depth. Ensured by find_index()
		find_lowest_index(searchPath_depth,&low,&mid,&depth);
		depth=searchPath_depth;
		find_highest_index(searchPath_depth,&high,&mid,&depth);
		
		printf("Bounds for same depth %d, low=%d,high=%d\n",searchPath_depth,low,high);
		
		while(low<=high){  //now we in entries bounded by low,high  with same depth as searchPath.
			mid=calculateMid(low,high);
			printf("mid=%d,low=%d,high=%d\n",mid,low,high);
		
			getEntry_file_path(mid,file_path);
		
			comp_result=strcmp(file_path,searchPath);
			printf("file_path=%s, searchPath=%s,comp_result=%d\n",file_path,searchPath,comp_result);
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
			printf("mid=%d,low=%d,high=%d\n",mid,low,high);
		}
		printf("No. of comparisons made=%d\n",no_of_comp);
		
	}
	return result;	
}
	
int	main( int argc, char **argv ){
	/* parse cmdline args */
	char	file_path[MAXPATHLEN];

	/*if( argc !=2) {
		showUsage();
		//exit(-1);
	}

	sscanf(entry[i],"%c %lo %ld %ld %ld %lld %lld %lld %lld %[^\13]s\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,file_path); //\13 is in octal (Vertical Tab).
	printf("%s\n",entry[i]);
	printf("c=%c mode=%lo nlink=%ld uid=%ld gid=%ld size=%lld atime=%lld mtime=%lld ctime=%lld path=abc%sabc",c,mode,nlink,uid,gid,size,atime,mtime,ctime,file_path);
	*/
	int i;
	long int low,high;
/*	for(i=0;i<14;i++){
		printf("strlen(catalog[%d])=%d\n",i,strlen(catalog[i]));

		printf(":catalog[%d]=%s:\n",i,file_path);
	} */
	
	/*FILE *fp;
	char catalogFile[]="catalog.list";
	
	fp=fopen(catalogFile,"w");
	if(fp==NULL){
		perror("");
		//exit(-1);
	}
	for(i=0;i<14;i++){
		fputs(catalog[i],fp);
	}*/
	
	//printf(" comparison=%d\n",strcmp("/dir1/link file","/dir2"));
	for(i=0;i<14;i++){
		sscanf(catalog[i],"%*s%*s%*s%*s%*s%*s%*s%*s%*s%*d\13%[^\13]s\13",file_path); //\13 is in octal (Vertical Tab).
		//printf("%s",file_path);
		printf("%d\n",i);
		//printf("position of %s=%d\n",file_path,binSearch(file_path));
		get_directory_contents(file_path,&low,&high);
		printf("directory contents indices for i=%d, file_path=%s, low=%d, high=%d\n",i,file_path,low,high);
	}
	
	
	
	/*printf("%d",findDepth("hello"));
	printf("%d",findDepth("/hello"));
	printf("%d",findDepth("/"));
	printf("%d",findDepth("/hello/.sfa/sfa d/"));
	printf("%d",findDepth("hello/.sfa/sfa d/"));
	printf("%d",findDepth(""));
	*/

	
	//printf("sizeof(int)=%d,INT_MAX=%d",sizeof(int),INT_MAX);

}


