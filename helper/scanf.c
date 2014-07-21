#include <stdio.h>
#include <string.h>

char entry[]="l 0710 1 119 29 9000 1213109162 1213109161 1213105161 3 \1/dir1/link file\2/dir2 kkdskl\1\1\1\1\n";
char entry2[]="l 0710 1 119 29 9000 1213109162 1213109161 1213105161 3 \1/dir1/link file/dir2 kkdskl\1\n";


int main(){

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
	char file_path[50];
	char link_dest[50];
	char temp[50];

	strcpy(link_dest,"kgn");
//	strcpy(link_dest2,"kgn2");
	
	int value=sscanf(entry,"%c %lo %ld %ld %ld %lld %lld %lld %lld %d \1%[^\1]s\1\n",&c,&mode,&nlink,&uid,&gid,&size,&atime,&mtime,&ctime,&depth,file_path);

	int i;
	for (i = 0; file_path[i] != '\2' && file_path[i] != '\0' ; i++)
	{
	    temp[i] = file_path[i];
	}
	temp[i] = '\0';
	i++;

	if(strlen(file_path)!=strlen(temp)){
		strcpy(link_dest,file_path + i );
	}
	strcpy(file_path,temp);

	/*j = 0;
	for (; link_dest2[i] != '\0'; i++)
	{
	    link_dest[j++] = link_dest2[i];
	}
	link_dest[i] = '\0';

	*/
	
	printf("entry=%s",entry);
	printf("depth=%d\n",depth);
	printf("variables read successfully=%d\n",value);
	printf("file_path=%s abc\n",file_path);
	printf("link_dest=%s xyz\n",link_dest);
//	printf("link_dest2=%s xyz\n",link_dest2);
}
