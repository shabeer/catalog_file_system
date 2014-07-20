/*
 * This file generates catalog_file out of a given directory. 
 * 
 * Compilation;
 *	 gcc -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE generate_catalog.c -o generate_catalog
 *	
 * Execution:
 *	./generate_catalog mount_dir_path catalog_file
 *
 *	mount_dir_path: should be directory for which, file list(catalog) need to be created.
 *	catalog_file: File in which catalog will be written.
 *	catalog_file File will be created if not present. Overwrites, if present.
 * 
 * Limitations; 
 * 1. Currently, extended file attributes are not put in catalog_file,since catalogfs doesnot support it currently.
 */

/* Status: 
 *
 * [done]Try to correct pad_each_line. All lines are not having same number of characters in catalog_file
 * [done]fixed memory leaks.
 * [done]Need to implement, padding \1 characters , so that each line has same no. of characters. Can be done using awk/sed.
 * [done]need to implement print_entry_to_catalog function().
 *
 * Checked memory leaks with
 *  valgrind  --leak-check=full --show-reachable=yes ./generate_catalog /home/shabeer catalog_home_shabeer2.list.gen
 * Checked code coverage using gcov  ( gcc -fprofile-arcs -ftest-coverage.....)
 *
 * Todo:
 * -ERROR:lstat return -1 for path=/home/shabeer/.VirtualBox/VDI/WindowsXP.vdi
 * -handle advanced cases of link file
 * -Profiling memory and time usage. Try to minimize memory usage.
 * -Clean up the comments.
 * -modify to have DEBUG statements,with different level of debug info.
 */

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


FILE *fp_catalog_file;
FILE *fp_catalog_file_temp;

int filter(const struct dirent *ent){
	if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0){
		return 0;
	}
	return 1;
}

// filter_dir only succeeds for directory entry.
int filter_dir(const struct dirent *ent){
	if(ent->d_type==DT_DIR){
		return 1;
	}
	return 0;
}

//breadth-first queue
struct bf_queue{
	char dirname[MAXPATHLEN];
	struct bf_queue* next;
}*head,*tail;

void init_queue(){
	head=NULL;
	tail=NULL;
}

void print_queue(){
	struct bf_queue *current;
	current=head;
	while(current){
		printf("DEBUG:printing queue=%s\n",current->dirname);
		current=current->next;
	}
}

int insert_queue(char dirname[MAXPATHLEN]){
	struct bf_queue *temp=NULL;
	
	//printf("DEBUG:Inserting =%s\n",dirname);
	temp=(struct bf_queue*)malloc(sizeof(struct bf_queue));
	if(temp==NULL)
		return -1;
	
	strcpy(temp->dirname,dirname);
	temp->next=NULL;
	
	if(head==NULL){
		//printf("DEBUG: head is null:%s\n",dirname);
		head=temp;
		tail=temp;
	}else{
		//printf("DEBUG: head is NOT null:%s\n",dirname);
		tail->next=temp;
		tail=tail->next;
	}
	//print_queue();
	return 0;
}

char *remove_queue(){
	struct bf_queue *temp=NULL;
	char *dirname;
	
	dirname=(char *)malloc(sizeof(char)*MAXPATHLEN);
	if(dirname==NULL){
		printf("ERROR: could not allocate memory, line number:%d\n",__LINE__);
	}
	
	if(head==NULL){
		return NULL;
	}
	temp=head;
	head=head->next;
	
	//print_queue();
	strcpy(dirname,temp->dirname);
	free(temp);
	
	return dirname;
}

typedef struct stat_with_depth_filepath{
	int depth;
	char	*file_path;
	struct stat st_data;
	char *link_dest;
}stat_wdf;

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

void print_entry_to_catalog(stat_wdf *st_wdf,const char *input_dir){
	// sscanf(entry,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%[^\1]s\1\n",
	// &c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,&depth,file_path); //\1 is used as separator.
	
	int mode,temp;
	char c='f'; //by default, file is considered to be 'regular file'
	int input_dir_len;
	int depth;
	char errormsg[256];
	
	if(strlen(input_dir)==0){
		sprintf(errormsg,"\nERROR:Empty input_dir:%s",input_dir);
		error(0,0,errormsg);
		exit(-1);
	}
	input_dir_len=strlen(input_dir);
	
	temp=((st_wdf->st_data.st_mode) & 0170000) >> 12 ;
	switch(temp){
		case DT_FIFO: c='p';break;
		case DT_CHR:  c='c';break;
		case DT_DIR:  c='d';break;
		case DT_BLK:  c='b';break;
		case DT_REG:  c='f';break;
		case DT_LNK:  c='l';break;
		case DT_SOCK: c='s';break;
		// in other cases, 'regular file' would be assumed.
	}
	
	mode=(st_wdf->st_data.st_mode) & 07777; //get last 4 octals
	depth=findDepth((st_wdf->file_path) + input_dir_len);
	
	
	if(c=='l'){
		fprintf(fp_catalog_file,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%s\2%s\1\n",
			c,
			(long unsigned int)mode,
			(long)st_wdf->st_data.st_nlink,
			(long)st_wdf->st_data.st_uid,
			(long)st_wdf->st_data.st_gid,
			(long long)st_wdf->st_data.st_size,
			(long long)st_wdf->st_data.st_atime,
			(long long)st_wdf->st_data.st_mtime,
			(long long)st_wdf->st_data.st_ctime,
			depth,
			(st_wdf->file_path) + input_dir_len,
			st_wdf->link_dest);
	}else{
		fprintf(fp_catalog_file,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%s\1\n",
			c,
			(long unsigned int)mode,
			(long)st_wdf->st_data.st_nlink,
			(long)st_wdf->st_data.st_uid,
			(long)st_wdf->st_data.st_gid,
			(long long)st_wdf->st_data.st_size,
			(long long)st_wdf->st_data.st_atime,
			(long long)st_wdf->st_data.st_mtime,
			(long long)st_wdf->st_data.st_ctime,
			depth,
			(st_wdf->file_path) + input_dir_len);
	}
	
	fflush(fp_catalog_file);
	free(st_wdf);
	return;
}


void print_to_catalog(char *path,const char *input_dir){
	int result;
	struct stat *st_data;
	stat_wdf *st_wdf;
	char link_buf[MAXPATHLEN];
				
	st_data=(struct stat *)malloc(sizeof(struct stat));
	if(st_data==NULL){
		printf("ERROR: could not allocate memory\n");
	}
	
	st_wdf=(stat_wdf*)malloc(sizeof(stat_wdf));
	if(st_wdf==NULL){
		printf("ERROR: could not allocate memory\n");
	}
	
	result = lstat(path, st_data);
	if(result == -1) {
		printf("ERROR:lstat return -1 for path=%s\n",path);
	}

	st_wdf->st_data.st_mode  =st_data->st_mode;  // %y  and %#m
	st_wdf->st_data.st_nlink =st_data->st_nlink;  //%n
	st_wdf->st_data.st_uid   =st_data->st_uid; //%U
	st_wdf->st_data.st_gid   =st_data->st_gid; //%G
	st_wdf->st_data.st_size  =st_data->st_size; //%s
	st_wdf->st_data.st_atime =st_data->st_atime; //%A@
	st_wdf->st_data.st_mtime =st_data->st_mtime; //%T@
	st_wdf->st_data.st_ctime =st_data->st_ctime; //%C@
	
	st_wdf->depth=findDepth(path);
	st_wdf->file_path=path;
	
	//todo, handle advanced cases of link file
	// 1. link destination points to , within the input_dir(/tmp/temp). eg   /tmp/temp/dir1/linkfile1 -> /tmp/temp/dir2
	// 2. link destination points to , outside the input_dir eg:   /tmp/temp/dir1/linkfile1 -> /usr/dir/file
	// 3a. link destination points to , relative location.(which is outside input_dir)
	//	eg: /tmp/temp/dir1/linkfile -> ../../../home (which actually points to /home)
	// 3b. link destination points to , relative location.(which is outside input_dir) 
	//	eg: /tmp/temp/dir1/linkfile -> ../dir2 (which actually points to /tmp/temp/dir2)
	
	if(  (((st_wdf->st_data.st_mode) & 0170000) >> 12) == DT_LNK){
		result = readlink(path, link_buf, MAXPATHLEN - 1);
		if(result == -1) {
			printf("ERROR: could not readlink of path=%s\n",path);
			//return -errno;
		}
		link_buf[result] = '\0';
		st_wdf->link_dest=link_buf;
		//st_wdf->link_dest="abc";
	}else{
		st_wdf->link_dest="\0";
	}
	
	print_entry_to_catalog(st_wdf,input_dir);
	//printf("%s\n",path);
	free(st_data);
}

int catalog_listing(const char *input_dir){
	struct dirent **namelist;
	char errormsg[256];
	int i=0,n;
	char current_dir[MAXPATHLEN];
	char *remove_queue_retval;
	struct bf_queue *current_q;
	
	init_queue();
	
	strcpy(current_dir,input_dir);
	//current_dir=input_dir;
	
	insert_queue(current_dir);
	
	current_q=head;
	while(current_q){
		// toodo check memory leak , in below 2 lines.
		remove_queue_retval=remove_queue();
		if(remove_queue_retval==NULL){
			printf("ERROR: remove_queue return NULL value\n");
		}
		strcpy(current_dir,remove_queue_retval);
		free(remove_queue_retval);
		
		char temp[MAXPATHLEN];
		strcpy(temp,current_dir);
		strcat(temp,"/");
		print_to_catalog(temp,input_dir);
		//printf("\nd_type=%d,d_name=%s\n",4,temp);
		
		//printf("\nd_type=4,d_name=%s/\n",current_dir);
		//printf("\ndirectory:%s\n",current_dir);

		n = scandir(current_dir, &namelist, filter, alphasort);
	
		if (n < 0){
			sprintf(errormsg,"\nERROR:scandir return non-zero value. \nInput Directory=%s",current_dir);
			error(0,errno,errormsg);
			//return 1;
		
		}else if(n==0){
			//printf("INFO:Empty directory:%s\n",current_dir);
			//return 1;
		
		}else {
			for(i=0;i<n;i++) {
				char temp[MAXPATHLEN];
				strcpy(temp,current_dir);
				strcat(temp,"/");
				strcat(temp,namelist[i]->d_name);
				print_to_catalog(temp,input_dir);
				//printf("d_type=%d,d_name=%s\n",namelist[i]->d_type,temp);
				//printf("d_type=%d,d_name=%s/%s\n",namelist[i]->d_type,current_dir,namelist[i]->d_name);
				
				//print_to_catalog();
				
				if(namelist[i]->d_type==DT_DIR){
				// insert in the breadth-first queue
					char temp[MAXPATHLEN];
					strcpy(temp,"");
					strcat(temp,current_dir);
					strcat(temp,"/");
					strcat(temp,namelist[i]->d_name);
					if(insert_queue(temp)==-1){
						printf("ERROR:Could not insert into to queue\n");
					}
				}
			}
	
			// clean-up dynamic allocated memory.
			for(i=0;i<n;i++) {
				free(namelist[i]);
			}
			free(namelist);
		}
		current_q=head;
	}
	
	return 0;
}


// This function pads, at the end of each line, with required number of \1 characters.
int pad_each_line(){
		
	char * line = NULL;
	size_t len = 0;
	ssize_t read_nchars;
	ssize_t max_len=0;
	int i=0;

	//rewind(fp_catalog_file);
	fseek(fp_catalog_file, 0L, SEEK_SET);
	
	while ((read_nchars = getline(&line, &len, fp_catalog_file)) != -1) {
		//printf("Retrieved line of length %zu :\n", read_nchars);
		max_len=(read_nchars>max_len)?read_nchars:max_len;
		//printf("%s", line);
	}
	//printf("Max line length:%d\n",max_len);
	
	fseek(fp_catalog_file, 0L, SEEK_SET);
	fseek(fp_catalog_file_temp, 0L, SEEK_SET);
	
	while ((read_nchars = getline(&line, &len, fp_catalog_file)) != -1) {
		fprintf(fp_catalog_file_temp,"%.*s",strlen(line)-1,line);
		for(i=0; i<(int)max_len-(int)strlen(line) ; i++){
			fprintf(fp_catalog_file_temp,"\1");
		}
		fprintf(fp_catalog_file_temp,"\n");
	}
	
	if (line!=NULL){
		free(line);
	}
	
	return 0;
}

void usage(){
	printf("Usage:\n");
	printf("$0  mount_dir_path catalog_file\n");
	printf("mount_dir_path: should be directory for which, file list(catalog) need to be created.\n");
	printf("catalog_file: File in which catalog will be written.\n");
	printf("catalog_file File will be created if not present. Overwriten, if present.\n");
	printf("\nExample:\n");
	printf("$0  /tmp/temp  catalog.list.gen\n"); 
}

int main(int argc, char *argv[]){
	char input_dir[MAXPATHLEN];
	char catalog_file[MAXPATHLEN];
	char catalog_file_temp[MAXPATHLEN];
	struct stat buf;
	
	char errormsg[256];
	int status;
	
	if(argc!=3){
		usage();
		exit(-1);
	}
		
	//toodo input_dir as command parameter.
	//toodo catalog_file as command parameter.
	//strcpy(input_dir,"/tmp/temp/");
	//strcpy(catalog_file,"catalog.list.gen");
	
	strcpy(input_dir,argv[1]);
	strcpy(catalog_file,argv[2]);
	
	if(strlen(input_dir)<=0 || strlen(catalog_file)<=0){
		printf("Empty input_dir or catalog_filen\n");
		exit(-1);
	}
	
	//toodo check if input_dir is actually present, and if it is a directory. It could be (regular/non-regular) file.
	sprintf(errormsg,"\nERROR:Cannot get statistics of the directory:%s",input_dir);
	if(stat(input_dir,&buf)!=0){
		error(0,errno,errormsg);
		exit(-1);
	}else{
		if( ! S_ISDIR(buf.st_mode)) { // not a directory.
			sprintf(errormsg,"\nERROR:Not a directory:%s",input_dir);
			error(0,errno,errormsg);
			exit(-1);
		}
	}
	
	//toodo : input_dir, should always be ended with /.  Since this is must for cataloging slash(/) directory 
	
	sprintf(errormsg,"\nERROR:Cannot open catalog file for writing:%s",catalog_file);
		
	fp_catalog_file = fopen(catalog_file, "w+");
	
	strcpy(catalog_file_temp,catalog_file);
	strncat(catalog_file_temp,".temp",5);
	fp_catalog_file_temp = fopen(catalog_file_temp, "w+");
	//fp_catalog_file=stdout; 

	if (fp_catalog_file == NULL) {
		error(0,errno,errormsg);
		//fprintf(stderr,"\nCannot open catalog file:%s:%s",catalogFile,sys_errlist[]);
		//fprintf(stderr,"ERROR:Unmount the mount point and retry by giving proper catalog file\n");
		exit(-1);
	}
	
	# Actual work starts from here.
	catalog_listing(input_dir);
	pad_each_line();
	
	fclose(fp_catalog_file);
	fclose(fp_catalog_file_temp);
	
	//printf("old_file=%s",catalog_file_temp);
	sprintf(errormsg,"\nERROR:Cannot rename file:%s to file:%s",catalog_file_temp,catalog_file);
	status=rename(catalog_file_temp,catalog_file);
	if(status!=0){
		error(0,errno,errormsg);
		printf("Created catalog file:%s\n",catalog_file_temp);
		exit(-1);
	}else {
		printf("Successfully created catalog: %s\n",catalog_file);
	}
	
	return 0;
}

