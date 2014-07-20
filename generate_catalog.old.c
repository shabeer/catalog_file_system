/*
 * This file generates catalog_file out of a given directory. 
 * 
 * Compilation;
 *	todo: gcc -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE generate_catalog.c -o generate_catalog
 *	
 * Execution:
 *	./generate_catalog directory_path catalog_file
 *
 *	directory_path:
 *	catalog_file: File in which catalog will be written.
 *	catalog_file File will be created if not present. Overwrites, if present.
 * 
 * Limitations; 
 * 1. Currently, extended file attributes are not put in catalog_file,since catalogfs doesnot support it currently.
 */


//	sscanf(entry,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%[^\1]s\1\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,&depth,file_path); //\1 is used as separator.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <dirent.h>
#include <errno.h>

struct dir_entries{
	char *entry_name;
	int entry_type;
	struct dir_entries* next;
};

// Entries of a directory(only one level).
struct dir_entries* directory_listing_sorted(char *path){
	DIR *dp;
	struct dirent *de;
	struct dir_entries *head,*current;

	head=NULL;
	dp = opendir(path);

	if(dp == NULL) {
		return NULL;
	}
	
	while( (de = readdir(dp)) != NULL ) {
		// skipping . and .. entries.
		if(strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0){
			struct dir_entries *temp=NULL;
			temp=(struct dir_entries *)(malloc(sizeof(struct dir_entries)));
		
			temp->entry_name=de->d_name;
			temp->entry_type=de->d_type;
			temp->next=NULL;
		
			if(head==NULL){
				head=temp;
				current=head;
			}else{
				current->next=temp;
				current=current->next;
			}
		}
	}

	closedir(dp);
	return head; 
}

int main(){
	char input_dir[MAXPATHLEN];
	char catalog_file[MAXPATHLEN];
	struct dir_entries *head,*current;
	
	strcpy(input_dir,"/tmp/temp");
	//todo if input_dir is actually present, and if it is directory. It could be (regular/non-regular) file.

	strcpy(catalog_file,"catalog.list.gen");
	
	head=directory_listing_sorted(input_dir);
	if(head==NULL){
		printf("Empty directory:%s\n",input_dir);
		return 1;
	}
	current=head;
	
	while(current){
		printf("d_name=%s,d_type=%d\n",current->entry_name,current->entry_type);
		current=current->next;
	}
	
	return 0;
}
