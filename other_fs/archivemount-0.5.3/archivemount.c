/*

   Copyright (c) 2005 Andre Landwehr <andrel@cybernoia.de>

   This program can be distributed under the terms of the GNU GPL.
   See the file COPYING.

   Based on: fusexmp.c by Miklos Szeredi

*/

#define VER_MAJOR 0
#define VER_MINOR 5
#define VER_RELEASE 3

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE 1
#endif

#define FUSE_USE_VERSION 22
#define MAXBUF 4096

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <utime.h>
#include <string.h>
#include <wchar.h>
#include <archive.h>
#include <archive_entry.h>

  /**********/
 /* macros */
/**********/
#ifdef NDEBUG
#   define log(format, ...)
#else
#   define log(format, ...) \
{ \
	FILE *FH = fopen( "/tmp/archivemount.log", "a" ); \
	if( FH ) { \
		fprintf( FH, "l. %4d: " format "\n", __LINE__, ##__VA_ARGS__ ); \
		fclose( FH ); \
	} \
}
#endif

  /*******************/
 /* data structures */
/*******************/

typedef struct node {
	struct node *parent;
	struct node *prev; /* previous in same directory */
	struct node *next; /* next in same directory */
	struct node *child; /* first child for directories */
	char *name; /* fully qualified with prepended '/' */
	char *location; /* location on disk for new/modified files, else NULL */
	int namechanged; /* true when file was renamed */
	struct archive_entry *entry; /* libarchive header data */
	int modified; /* true when node was modified */
} NODE;


  /***********/
 /* globals */
/***********/

static int archiveFd; /* file descriptor of archive file, just to keep the
			 beast alive in case somebody deletes the file while
			 it is mounted */
static int archiveModified = 0;
static int archiveWriteable = 0;
static NODE *root;


  /**********************/
 /* internal functions */
/**********************/

static void
init_node( NODE *node )
{
	node->parent = NULL;
	node->prev = NULL;
	node->next = NULL;
	node->child = NULL;
	node->name = NULL;
	node->location = NULL;
	node->namechanged = 0;
	node->entry = NULL;
	node->modified = 0;
}

static void
remove_child( NODE *node )
{
	if( node->prev ) {
		node->prev->next = node->next;
	} else {
		if( node->parent ) {
			node->parent->child = node->next;
		}
	}
	if( node->next ) {
		node->next->prev = node->prev;
	}
}

static void
insert_as_child( NODE *node, NODE *parent )
{
	node->parent = parent;
	if( ! parent->child ) {
		parent->child = node;
	} else {
		/* find last child of parent, insert node behind it */
		NODE *b = parent->child;
		while( b->next ) {
			b = b->next;
		}
		b->next = node;
		node->prev = b;
		node->next = NULL;
	}
	//log( "inserted '%s' as child of '%s'", node->name, parent->name );
}

/*
 * inserts "node" into tree starting at "root" according to the path
 * specified in node->name
 * @return 0 on success, 0-errno else (ENOENT or ENOTDIR)
 */
static int
insert_by_path( NODE *root, NODE *node )
{
	char *temp;
	NODE *cur = root;
	char *key = node->name;

	key++;
	while( ( temp = strchr( key, '/' ) ) ) {
		size_t namlen = temp - key;
		char nam[namlen + 1];
		NODE *last = cur;

		strncpy( nam, key, namlen );
		nam[namlen] = '\0';
		if( strcmp( strrchr( cur->name, '/' ) + 1, nam ) != 0 ) {
			cur = cur->child;
			while( cur && strcmp( strrchr( cur->name, '/' ) + 1,
						nam ) != 0 )
			{
				cur = cur->next;
			}
		}
		if( ! cur ) {
			/* parent path not found, create a temporary one */
			NODE *tempnode;
			tempnode = malloc( sizeof( NODE ) );
			init_node( tempnode );
			tempnode->name = malloc(
					strlen( last->name ) + namlen + 1 );
			if( last != root ) {
				sprintf( tempnode->name, "%s/%s", last->name, nam );
			} else {
				sprintf( tempnode->name, "/%s", nam );
			}
			tempnode->entry = archive_entry_clone( root->entry );
			/* insert it recursively */
			insert_by_path( root, tempnode );
			/* now inserting node should work, correct cur for it */
			cur = tempnode;
		}
		/* iterate */
		key = temp + 1;
	}
	if( S_ISDIR( archive_entry_mode( cur->entry ) ) ) {
		/* check if a child of this name already exists */
		NODE *tempnode;
		int found = 0;
		tempnode = cur->child;
		while( tempnode ) {
			if( strcmp( strrchr( tempnode->name, '/' ) + 1,
						strrchr( node->name, '/' ) + 1 )
					== 0 )
			{
				/* this is a dupe due to a temporarily inserted
				   node, just update the entry */
				archive_entry_free( node->entry );
				node->entry = archive_entry_clone(
						tempnode->entry );
				found = 1;
				break;
			}
			tempnode = tempnode->next;
		}
		if( ! found ) {
			insert_as_child( node, cur );
		}
	} else {
		return -ENOTDIR;
	}
	return 0;
}

static int
build_tree( const char *mtpt )
{
	struct archive *archive;
	struct archive_entry *entry;
	struct stat st;
	int format;
	int compression;

	/* open archive */
	archive = archive_read_new();
	if( archive_read_support_compression_all( archive ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	if( archive_read_support_format_all( archive ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	if( archive_read_open_fd( archive, archiveFd, 10240 ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	/* check if format or compression prohibits writability */
	format = archive_format( archive );
	compression = archive_compression( archive );
	if( format & ARCHIVE_FORMAT_ISO9660
			|| format & ARCHIVE_FORMAT_ISO9660_ROCKRIDGE
			|| format & ARCHIVE_FORMAT_ZIP
			|| compression == ARCHIVE_COMPRESSION_COMPRESS )
	{
		archiveWriteable = 0;
	}
	/* create root node */
	root = malloc( sizeof( NODE ) );
	init_node( root );
	root->name = strdup( "/" );
	/* fill root->entry */
	root->entry = archive_entry_new();
	if( fstat( archiveFd, &st ) != 0 ) {
		perror( "Error stat'ing archiveFile" );
		return errno;
	}
	archive_entry_set_gid( root->entry, getgid() );
	archive_entry_set_gid( root->entry, getuid() );
	archive_entry_set_mode( root->entry, st.st_mtime );
	archive_entry_set_pathname( root->entry, "/" );
	archive_entry_set_size( root->entry, st.st_size );
	stat( mtpt, &st );
	archive_entry_set_mode( root->entry, st.st_mode );
	/* read all entries in archive, create node for each */
	while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ) {
		NODE *cur;
		const char *name;
		/* find name of node */
		name = archive_entry_pathname( entry );
		if( strncmp( name, "./\0", 3 ) == 0 ) {
			/* special case: the directory "./" must be skipped! */
			continue;
		}
		/* create node and clone the entry */
		cur = malloc( sizeof( NODE ) );
		init_node( cur );
		cur->entry = archive_entry_clone( entry );
		/* normalize the name to start with "/" */
		if( strncmp( name, "./", 2 ) == 0 ) {
			/* remove the "." of "./" */
			cur->name = strdup( name + 1 );
		} else if( name[0] != '/' ) {
			/* prepend a '/' to name */
			cur->name = malloc( strlen( name ) + 2 );
			sprintf( cur->name, "/%s", name );
		} else {
			/* just set the name */
			cur->name = strdup( name );
		}
		/* remove trailing '/' for directories */
		if( cur->name[strlen(cur->name)-1] == '/' ) {
			cur->name[strlen(cur->name)-1] = '\0';
		}
		/* references */
		if( insert_by_path( root, cur ) != 0 ) {
			log( "ERROR: could not insert %s into tree",
					cur->name );
			return -ENOENT;
		}
		archive_read_data_skip( archive );
	}
	/* close archive */
	archive_read_finish( archive );
	lseek( archiveFd, 0, SEEK_SET );
	return 0;
}

static NODE *
find_modified_node( NODE *start )
{
	NODE *ret = NULL;
	NODE *run = start;

	while( run ) {
		if( run->modified ) {
			ret = run;
			break;
		}
		if( run->child ) {
			if( ( ret = find_modified_node( run->child ) ) ) {
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static NODE *
get_node_for_path( NODE *start, const char *path )
{
	NODE *ret = NULL;
	NODE *run = start;

	while( run ) {
		if( strcmp( path, run->name ) == 0 ) {
			ret = run;
			break;
		}
		if( run->child && strncmp( path, run->name,
					strlen( run->name ) ) == 0 )
		{
			if( ( ret = get_node_for_path( run->child, path ) ) ) {
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static NODE *
get_node_for_entry( NODE *start, struct archive_entry *entry )
{
	NODE *ret = NULL;
	NODE *run = start;
	const char *path = archive_entry_pathname( entry );

	if( *path == '/' ) {
		path++;
	}
	while( run ) {
		const char *name = archive_entry_pathname( run->entry );
		if( *name == '/' ) {
			name++;
		}
		if( strcmp( path, name ) == 0 ) {
			ret = run;
			break;
		}
		if( run->child ) {
			if( ( ret = get_node_for_entry( run->child, entry ) ) )
			{
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static int
rename_recursively( NODE *start, const char *from, const char *to )
{
	char *individualName;
	char *newName;
	int ret = 0;
	NODE *node = start;

	while( node ) {
		if( node->child ) {
			/* recurse */
			ret = rename_recursively( node->child, from, to );
		}
		/* change node name */
		individualName = node->name + strlen( from );
		if( *to != '/' ) {
			newName = ( char * )malloc( strlen( to ) +
					strlen( individualName ) + 2 );
			sprintf( newName, "/%s%s", to, individualName );
		} else {
			newName = ( char * )malloc( strlen( to ) +
					strlen( individualName ) + 1 );
			sprintf( newName, "%s%s", to, individualName );
		}
		free( node->name );
		node->name = newName;
		node->namechanged = 1;
		/* iterate */
		node = node->next;
	}
	return ret;
}

static int
get_temp_file_name( const char *path, char **location )
{
	char *tmppath;
	char *tmp;
	int fh;

	/* create name for temp file */
	tmp = tmppath = strdup( path );
	do if( *tmp == '/' ) *tmp = '_'; while( *( tmp++ ) );
	*location = ( char * )malloc(
			strlen( P_tmpdir ) +
			strlen( "_archivemount" ) +
			strlen( tmppath ) + 8 );
	sprintf( *location, "%s/archivemount%s_XXXXXX", P_tmpdir, tmppath );
	free( tmppath );
	if( ( fh = mkstemp( *location ) == -1 ) ) {
		log( "Could not create temp file name %s: %s",
				*location, strerror( errno ) );
		free( *location );
		return 0 - errno;
	}
	close( fh );
	unlink( *location );
	return 0;
}

/**
 * Updates given nodes node->entry by stat'ing node->location. Does not update
 * the name!
 */
static int
update_entry_stat( NODE *node )
{
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	if( lstat( node->location, &st ) != 0 ) {
		return 0 - errno;
	}
	archive_entry_set_gid( node->entry, st.st_gid );
	archive_entry_set_uid( node->entry, st.st_uid );
	archive_entry_set_mtime( node->entry, st.st_mtime, 0 );
	archive_entry_set_size( node->entry, st.st_size );
	archive_entry_set_mode( node->entry, st.st_mode );
	archive_entry_set_rdevmajor( node->entry, st.st_dev );
	archive_entry_set_rdevminor( node->entry, st.st_dev );
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	}
	return 0;
}

/*
 * write a new or modified file to the new archive; used from save()
 */
static void
write_new_modded_file( NODE *node, struct archive_entry *wentry,
		struct archive *newarc )
{
	if( node->location ) {
		struct stat st;
		int fh = 0;
		off_t offset = 0;
		void *buf;
		ssize_t len;
		/* copy stat info */
		if( lstat( node->location, &st ) != 0 ) {
			log( "Could not lstat temporary file %s: ",
					node->location,
					strerror( errno ) );
			archive_entry_free( wentry );
			close( fh );
			return;
		}
		archive_entry_copy_stat( wentry, &st );
		/* open temporary file */
		fh = open( node->location, O_RDONLY );
		if( fh == -1 ) {
			log( "Fatal error opening modified file %s at "
					"location %s, giving up",
					node->name, node->location );
			archive_entry_free( wentry );
			return;
		}
		/* write header */
		archive_write_header( newarc, wentry );
		/* copy data */
		buf = malloc( MAXBUF );
		while( ( len = pread( fh, buf, ( size_t )MAXBUF,
						offset ) ) > 0 )
		{
			archive_write_data( newarc, buf, len );
			offset += len;
		}
		free( buf );
		if( len == -1 ) {
			log( "Error reading temporary file %s for file %s: ",
					node->location,
					node->name,
					strerror( errno ) );
			archive_entry_free( wentry );
			close( fh );
			return;
		}
		/* clean up */
		close( fh );
		if( S_ISDIR( st.st_mode ) ) {
			if( rmdir( node->location ) == -1 ) {
				log( "WARNING: rmdir '%s' failed: %s",
						node->location, strerror( errno ) );
			}
		} else {
			if( unlink( node->location ) == -1 ) {
				log( "WARNING: unlinking '%s' failed: %s",
						node->location, strerror( errno ) );
			}
		}
	} else {
		/* no data, only write header (e.g. when node is a link!) */
		/* FIXME: hardlinks are saved, symlinks not. why??? */
		//log( "writing header for file %s", archive_entry_pathname( wentry ) );
		archive_write_header( newarc, wentry );
	}
	/* mark file as written */
	node->modified = 0;
}

static int
save( const char *archiveFile )
{
	struct archive *oldarc;
	struct archive *newarc;
	struct archive_entry *entry;
	int tempfile;
	int format;
	int compression;
	char oldfilename[PATH_MAX];
	NODE *node;
	char buf[PATH_MAX];

	getcwd( buf, PATH_MAX );
	/* unfortunately libarchive does not support modification of
	 * compressed archives, so a new archive has to be written */
	/* rename old archive */
	sprintf( oldfilename, "%s.orig", archiveFile );
	close( archiveFd );
	if( rename( archiveFile, oldfilename ) < 0 ) {
		int err = errno;
		log( "Could not rename old archive file (%s/%s): %s",
				buf, archiveFile, strerror( err ) );
		archiveFd = open( archiveFile, O_RDONLY );
		return 0 - err;
	}
	archiveFd = open( oldfilename, O_RDONLY );
	/* open old archive */
	oldarc = archive_read_new();
	if( archive_read_support_compression_all( oldarc ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	if( archive_read_support_format_all( oldarc ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	if( archive_read_open_fd( oldarc, archiveFd, 10240 ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	format = archive_format( oldarc );
	compression = archive_compression( oldarc );
	/*
	log( "format of old archive is %s (%d)",
			archive_format_name( oldarc ),
			format );
	log( "compression of old archive is %s (%d)",
			archive_compression_name( oldarc ),
			compression );
	*/
	/* open new archive */
	newarc = archive_write_new();
	switch( compression ) {
		case ARCHIVE_COMPRESSION_GZIP:
			archive_write_set_compression_gzip( newarc );
			break;
		case ARCHIVE_COMPRESSION_BZIP2:
			archive_write_set_compression_bzip2( newarc );
			break;
		case ARCHIVE_COMPRESSION_COMPRESS:
		case ARCHIVE_COMPRESSION_NONE:
		default:
			archive_write_set_compression_none( newarc );
			break;
	}
#if 0
	if( archive_write_set_format( newarc, format ) != ARCHIVE_OK ) {
		return -ENOTSUP;
	}
#endif
	if( format & ARCHIVE_FORMAT_CPIO
			|| format & ARCHIVE_FORMAT_CPIO_POSIX )
	{
		archive_write_set_format_cpio( newarc );
		log( "set write format to posix-cpio" );
	} else if( format & ARCHIVE_FORMAT_SHAR
			|| format & ARCHIVE_FORMAT_SHAR_BASE
			|| format & ARCHIVE_FORMAT_SHAR_DUMP )
	{
		archive_write_set_format_shar( newarc );
		log( "set write format to binary shar" );
	} else if( format & ARCHIVE_FORMAT_TAR_PAX_RESTRICTED )
	{
		archive_write_set_format_pax_restricted( newarc );
		log( "set write format to binary pax restricted" );
	} else if( format & ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE )
	{
		archive_write_set_format_pax( newarc );
		log( "set write format to binary pax interchange" );
	} else if( format & ARCHIVE_FORMAT_TAR_USTAR
			|| format & ARCHIVE_FORMAT_TAR
			|| format & ARCHIVE_FORMAT_TAR_GNUTAR
			|| format == 0 )
	{
		archive_write_set_format_ustar( newarc );
		log( "set write format to ustar" );
	} else {
		log( "writing archives of format %d (%s) is not "
				"supported", format,
				archive_format_name( oldarc ) );
		return -ENOTSUP;
	}
	tempfile = open( archiveFile,
			O_WRONLY | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if( tempfile == -1 ) {
		log( "could not open new archive file for writing" );
		return 0 - errno;
	}
	if( archive_write_open_fd( newarc, tempfile ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( newarc ) );
		return archive_errno( newarc );
	}
	while( archive_read_next_header( oldarc, &entry ) == ARCHIVE_OK ) {
		off_t offset;
		const void *buf;
		struct archive_entry *wentry;
		size_t len;
		const char *name;
		/* find corresponding node */
		name = archive_entry_pathname( entry );
		node = get_node_for_entry( root, entry );
		if( ! node ) {
			log( "WARNING: no such node for '%s'", name );
			archive_read_data_skip( oldarc );
			continue;
		}
		/* create new entry, copy metadata */
		wentry = archive_entry_new();
		if( archive_entry_gname_w( node->entry ) ) {
			archive_entry_copy_gname_w( wentry,
					archive_entry_gname_w( node->entry ) );
		}
		if( archive_entry_hardlink( node->entry ) ) {
			archive_entry_copy_hardlink( wentry,
					archive_entry_hardlink( node->entry ) );
		}
		if( archive_entry_hardlink_w( node->entry ) ) {
			archive_entry_copy_hardlink_w( wentry,
					archive_entry_hardlink_w(
						node->entry ) );
		}
		archive_entry_copy_stat( wentry,
				archive_entry_stat( node->entry ) );
		if( archive_entry_symlink_w( node->entry ) ) {
			archive_entry_copy_symlink_w( wentry,
					archive_entry_symlink_w(
						node->entry ) );
		}
		if( archive_entry_uname_w( node->entry ) ) {
			archive_entry_copy_uname_w( wentry,
					archive_entry_uname_w( node->entry ) );
		}
		/* set correct name */
		if( node->namechanged ) {
			if( *name == '/' ) {
				archive_entry_set_pathname(
						wentry, node->name );
			} else {
				archive_entry_set_pathname(
						wentry, node->name + 1 );
			}
		} else {
			archive_entry_set_pathname( wentry, name );
		}
		/* write header and copy data */
		if( node->modified ) {
			/* file was modified */
			write_new_modded_file( node, wentry, newarc );
		} else {
			/* file was not modified */
			archive_entry_copy_stat( wentry,
					archive_entry_stat( node->entry ) );
			archive_write_header( newarc, wentry );
			while( archive_read_data_block( oldarc, &buf,
						&len, &offset ) == ARCHIVE_OK )
			{
				archive_write_data( newarc, buf, len );
			}
		}
		/* clean up */
		archive_entry_free( wentry );
	} /* end: while read next header */
	/* find new files to add (those do still have modified flag set */
	while( ( node = find_modified_node( root ) ) ) {
		write_new_modded_file( node, node->entry, newarc );
	}
	/* clean up, re-open the new archive for reading */
	archive_read_finish( oldarc );
	archive_write_finish( newarc );
	close( tempfile );
	close( archiveFd );
	archiveFd = open( archiveFile, O_RDONLY );
	return 0;
}


  /*****************/
 /* API functions */
/*****************/

static int
ar_read( const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi )
{
	int ret = -1;
	const char *realpath;
	NODE *node;
	( void )fi;

	/* find node */
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_read( archive_entry_hardlink( node->entry ),
				buf, size, offset, fi );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return ar_read( archive_entry_symlink( node->entry ),
				buf, size, offset, fi );
	}
	if( node->modified ) {
		/* the file is new or modified, read temporary file instead */
		int fh;
		fh = open( node->location, O_RDONLY );
		if( fh == -1 ) {
			log( "Fatal error opening modified file '%s' at "
					"location '%s', giving up",
					path, node->location );
			return 0 - errno;
		}
		/* copy data */
		if( ( ret = pread( fh, buf, size, offset ) ) == -1 ) {
			log( "Error reading temporary file '%s': %s",
					node->location, strerror( errno ) );
			close( fh );
			ret = 0 - errno;
		}
		/* clean up */
		close( fh );
	} else {
		struct archive *archive;
		struct archive_entry *entry;
		/* search file in archive */
		realpath = archive_entry_pathname( node->entry );
		archive = archive_read_new();
		if( archive_read_support_compression_all( archive ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		if( archive_read_support_format_all( archive ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		if( archive_read_open_fd( archive, archiveFd, 10240 ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		/* search for file to read */
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ) {
			const char *name;
			name = archive_entry_pathname( entry );
			if( strcmp( realpath, name ) == 0 ) {
				void *trash;
				trash = malloc( MAXBUF );
				/* skip offset */
				while( offset > 0 ) {
					int skip = offset > MAXBUF ? MAXBUF : offset;
					ret = archive_read_data(
							archive, trash, skip );
					if( ret == ARCHIVE_FATAL
							|| ret == ARCHIVE_WARN
							|| ret == ARCHIVE_RETRY )
					{
						log( "ar_read (skipping offset): %s",
							archive_error_string( archive ) );
						errno = archive_errno( archive );
						ret = -1;
						break;
					}
					offset -= skip;
				}
				free( trash );
				if( offset ) {
					/* there was an error */
					break;
				}
				/* read data */
				ret = archive_read_data( archive, buf, size );
				if( ret == ARCHIVE_FATAL
						|| ret == ARCHIVE_WARN
						|| ret == ARCHIVE_RETRY )
				{
					log( "ar_read (reading data): %s",
						archive_error_string( archive ) );
					errno = archive_errno( archive );
					ret = -1;
					break;
				}
			}
			archive_read_data_skip( archive );
		}
		/* close archive */
		archive_read_finish( archive );
		lseek( archiveFd, 0, SEEK_SET );
	}
	return ret;
}

static int
ar_getattr( const char *path, struct stat *stbuf )
{
	NODE *node;

	//log( "getattr got path: '%s'\n", path );
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_getattr( archive_entry_hardlink(
					node->entry ), stbuf );
	}
	memcpy( stbuf,
			archive_entry_stat( node->entry ),
			sizeof( struct stat ) );
	return 0;
}

/*
 * mkdir is nearly identical to mknod...
 */
static int
ar_mkdir( const char *path, mode_t mode )
{
	NODE *node;
	char *location;
	int tmp;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	/* check for existing node */
	node = get_node_for_path( root, path );
	if( node ) {
		return -EEXIST;
	}
	/* create name for temp file */
	if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
		return tmp;
	}
	/* create temp file */
	if( mkdir( location, mode ) == -1 ) {
		log( "Could not create temporary dir %s: %s",
				location, strerror( errno ) );
		free( location );
		return 0 - errno;
	}
	/* build node */
	node = ( NODE * )malloc( sizeof( NODE ) );
	init_node( node );
	node->location = location;
	node->modified = 1;
	node->name = strdup( path );
	node->namechanged = 0;
	/* build entry */
	node->entry = archive_entry_new();
	if( root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "mknod: error stat'ing dir %s: %s", node->location,
				strerror( 0 - tmp ) );
		rmdir( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return tmp;
	}
	/* add node to tree */
	if( insert_by_path( root, node ) != 0 ) {
		log( "ERROR: could not insert %s into tree",
				node->name );
		rmdir( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return -ENOENT;
	}
	/* clean up */
	archiveModified = 1;
	return 0;
}

/*
 * ar_rmdir is called for directories only and does not need to do any
 * recursive stuff
 */
static int
ar_rmdir( const char *path )
{
	NODE *node;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( node->child ) {
		return -ENOTEMPTY;
	}
	if( node->name[strlen(node->name)-1] == '.' ) {
		return -EINVAL;
	}
	if( ! S_ISDIR( archive_entry_mode( node->entry ) ) ) {
		return -ENOTDIR;
	}
	if( node->location ) {
		/* remove temp directory */
		if( rmdir( node->location ) == -1 ) {
			int err = errno;
			log( "ERROR: removing temp directory %s failed: %s",
					node->location, strerror( err ) );
			return err;
		}
		free( node->location );
	}
	remove_child( node );
	free( node->name );
	free( node );
	archiveModified = 1;
	return 0;
}

static int
ar_symlink( const char *from, const char *to )
{
	NODE *node;
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	return -ENOSYS; /* somehow saving symlinks does not work. The code below is ok.. see write_new_modded_file() */
	log( "symlink called, %s -> %s", from, to );
	if( ! archiveWriteable ) {
		return -EROFS;
	}
	/* check for existing node */
	node = get_node_for_path( root, to );
	if( node ) {
		return -EEXIST;
	}
	/* build node */
	node = ( NODE * )malloc( sizeof( NODE ) );
	init_node( node );
	node->name = strdup( to );
	node->modified = 1;
	/* build stat info */
	st.st_dev = 0;
	st.st_ino = 0;
	st.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
	st.st_nlink = 1;
	st.st_uid = getuid();
	st.st_gid = getgid();
	st.st_rdev = 0;
	st.st_size = strlen( from );
	st.st_blksize = 4096;
	st.st_blocks = 0;
	st.st_atime = st.st_ctime = st.st_mtime = time( NULL );
	/* build entry */
	node->entry = archive_entry_new();
	if( root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	archive_entry_copy_stat( node->entry, &st );
	archive_entry_set_symlink( node->entry, strdup( from ) );
	/* get user/group name */
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		/* a name was found for the uid */
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getpwuid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			return 0 - errno;
		}
		/* on other errors the uid just could
		   not be resolved into a name */
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		/* a name was found for the uid */
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getgrgid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			return 0 - errno;
		}
		/* on other errors the gid just could
		   not be resolved into a name */
	}
	/* add node to tree */
	if( insert_by_path( root, node ) != 0 ) {
		log( "ERROR: could not insert symlink %s into tree",
				node->name );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return -ENOENT;
	}
	/* clean up */
	archiveModified = 1;
	return 0;
}

static int
ar_link( const char *from, const char *to )
{
	NODE *node;
	NODE *fromnode;
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	//log( "hardlink called, %s -> %s", from, to );
	if( ! archiveWriteable ) {
		return -EROFS;
	}
	/* find source node */
	fromnode = get_node_for_path( root, from );
	if( ! fromnode ) {
		return -ENOENT;
	}
	/* check for existing target */
	node = get_node_for_path( root, to );
	if( node ) {
		return -EEXIST;
	}
	/* extract originals stat info */
	ar_getattr( from, &st );
	/* build new node */
	node = ( NODE * )malloc( sizeof( NODE ) );
	init_node( node );
	node->name = strdup( to );
	node->modified = 1;
	/* build entry */
	node->entry = archive_entry_new();
	if( node->name[0] == '/' &&
			archive_entry_pathname( fromnode->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	archive_entry_copy_stat( node->entry, &st );
	archive_entry_set_hardlink( node->entry, strdup( from ) );
	/* get user/group name */
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		/* a name was found for the uid */
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getpwuid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			return 0 - errno;
		}
		/* on other errors the uid just could
		   not be resolved into a name */
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		/* a name was found for the uid */
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getgrgid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			return 0 - errno;
		}
		/* on other errors the gid just could
		   not be resolved into a name */
	}
	/* add node to tree */
	if( insert_by_path( root, node ) != 0 ) {
		log( "ERROR: could not insert hardlink %s into tree",
				node->name );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return -ENOENT;
	}
	/* clean up */
	archiveModified = 1;
	return 0;
}

static int
ar_truncate( const char *path, off_t size )
{
	NODE *node;
	char *location;
	int ret;
	int tmp;
	int fh;

	//log( "truncate called" );
	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_truncate(
				archive_entry_hardlink( node->entry ), size );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return ar_truncate(
				archive_entry_symlink( node->entry ), size );
	}
	if( node->location ) {
		/* open existing temp file */
		location = node->location;
		if( ( fh = open( location, O_WRONLY ) ) == -1 ) {
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
	} else {
		/* create new temp file */
		char *tmpbuf = NULL;
		int tmpoffset = 0;
		int64_t tmpsize;
		struct fuse_file_info fi;
		if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
			return tmp;
		}
		if( ( fh = open( location, O_WRONLY | O_CREAT | O_EXCL,
				archive_entry_mode( node->entry ) ) ) == -1 )
		{
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
		/* copy original file to temporary file */
		tmpsize = archive_entry_size( node->entry );
		tmpbuf = ( char * )malloc( MAXBUF );
		while( tmpsize ) {
			int len = tmpsize > MAXBUF ? MAXBUF : tmpsize;
			/* read */
			if( ( tmp = ar_read( path, tmpbuf, len, tmpoffset, &fi ) )
					< 0 )
			{
				log( "ERROR reading while copying %s to "
						"temporary location %s: %s",
						path, location,
						strerror( 0 - tmp ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			/* write */
			if( write( fh, tmpbuf, tmp ) == -1 ) {
				tmp = 0 - errno;
				log( "ERROR writing while copying %s to "
					       "temporary location %s: %s",
						path, location,
						strerror( errno ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			tmpsize -= len;
			tmpoffset += len;
			if( tmpoffset >= size ) {
				/* copied enough, exit the loop */
				break;
			}
		}
		/* clean up */
		free( tmpbuf );
		lseek( fh, 0, SEEK_SET );
	}
	/* truncate temporary file */
	if( ( ret = truncate( location, size ) ) == -1 ) {
		tmp = 0 - errno;
		log( "ERROR truncating %s (temporary location %s): %s",
				path, location, strerror( errno ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* record location, update entry */
	node->location = location;
	node->modified = 1;
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "write: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* clean up */
	close( fh );
	archiveModified = 1;
	return ret;
}

static int
ar_write( const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi )
{
	NODE *node;
	char *location;
	int ret;
	int tmp;
	int fh;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( S_ISLNK( archive_entry_mode( node->entry ) ) ) {
		/* file is a symlink, recurse into it */
		return ar_write( archive_entry_symlink( node->entry ),
				buf, size, offset, fi );
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_write( archive_entry_hardlink( node->entry ),
				buf, size, offset, fi );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return ar_write( archive_entry_symlink( node->entry ),
				buf, size, offset, fi );
	}
	if( node->location ) {
		/* open existing temp file */
		location = node->location;
		if( ( fh = open( location, O_WRONLY ) ) == -1 ) {
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
	} else {
		/* create new temp file */
		char *tmpbuf = NULL;
		int tmpoffset = 0;
		int64_t tmpsize;
		if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
			return tmp;
		}
		if( ( fh = open( location, O_WRONLY | O_CREAT | O_EXCL,
				archive_entry_mode( node->entry ) ) ) == -1 )
		{
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
		/* copy original file to temporary file */
		tmpsize = archive_entry_size( node->entry );
		tmpbuf = ( char * )malloc( MAXBUF );
		while( tmpsize ) {
			int len = tmpsize > MAXBUF ? MAXBUF : tmpsize;
			/* read */
			if( ( tmp = ar_read( path, tmpbuf, len, tmpoffset, fi ) )
					!= 0 )
			{
				log( "ERROR reading while copying %s to "
						"temporary location %s: %s",
						path, location,
						strerror( 0 - tmp ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			/* write */
			if( write( fh, tmpbuf, len ) == -1 ) {
				tmp = 0 - errno;
				log( "ERROR writing while copying %s to "
					       "temporary location %s: %s",
						path, location,
						strerror( errno ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			tmpsize -= len;
			tmpoffset += len;
		}
		/* clean up */
		free( tmpbuf );
		lseek( fh, 0, SEEK_SET );
	}
	/* write changes to temporary file */
	if( ( ret = pwrite( fh, buf, size, offset ) ) == -1 ) {
		tmp = 0 - errno;
		log( "ERROR writing changes to %s (temporary "
				"location %s): %s",
				path, location, strerror( errno ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* record location, update entry */
	node->location = location;
	node->modified = 1;
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "write: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* clean up */
	close( fh );
	archiveModified = 1;
	return ret;
}

static int
ar_mknod( const char *path, mode_t mode, dev_t rdev )
{
	NODE *node;
	char *location;
	int tmp;

	//log( "mknod called, %s", path );
	if( ! archiveWriteable ) {
		return -EROFS;
	}
	/* check for existing node */
	node = get_node_for_path( root, path );
	if( node ) {
		return -EEXIST;
	}
	/* create name for temp file */
	if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
		return tmp;
	}
	/* create temp file */
	if( mknod( location, mode, rdev ) == -1 ) {
		log( "Could not create temporary file %s: %s",
				location, strerror( errno ) );
		free( location );
		return 0 - errno;
	}
	/* build node */
	node = ( NODE * )malloc( sizeof( NODE ) );
	init_node( node );
	node->location = location;
	node->modified = 1;
	node->name = strdup( path );
	/* build entry */
	node->entry = archive_entry_new();
	if( root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "mknod: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return tmp;
	}
	/* add node to tree */
	if( insert_by_path( root, node ) != 0 ) {
		log( "ERROR: could not insert %s into tree",
				node->name );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		return -ENOENT;
	}
	/* clean up */
	archiveModified = 1;
	return 0;
}

static int
ar_unlink( const char *path )
{
	NODE *node;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( S_ISDIR( archive_entry_mode( node->entry ) ) ) {
		return -EISDIR;
	}
	if( node->location ) {
		/* remove temporary file */
		if( unlink( node->location ) == -1 ) {
			int err = errno;
			log( "ERROR: could not unlink temporary file '%s'",
					node->location, strerror( errno ) );
			return err;
		}
		free( node->location );
	}
	remove_child( node );
	free( node->name );
	free( node );
	archiveModified = 1;
	return 0;
}

static int
ar_chmod( const char *path, mode_t mode )
{
	NODE *node;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_chmod( archive_entry_hardlink( node->entry ), mode );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return ar_chmod( archive_entry_symlink( node->entry ), mode );
	}
	archive_entry_set_mode( node->entry, mode );
	archiveModified = 1;
	return 0;
}

static int
ar_chown( const char *path, uid_t uid, gid_t gid )
{
	NODE *node;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_chown( archive_entry_hardlink( node->entry ),
				uid, gid );
	}
	/* changing ownership of symlinks is allowed, however */
	archive_entry_set_uid( node->entry, uid );
	archive_entry_set_gid( node->entry, gid );
	archiveModified = 1;
	return 0;
}

static int
ar_utime( const char *path, struct utimbuf *buf )
{
	NODE *node;

	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return ar_utime( archive_entry_hardlink( node->entry ), buf );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return ar_utime( archive_entry_symlink( node->entry ), buf );
	}
	archive_entry_set_mtime( node->entry, buf->modtime, 0 );
	archive_entry_set_atime( node->entry, buf->actime, 0 );
	archiveModified = 1;
	return 0;
}

static int
ar_statfs( const char *path, struct statfs *stbuf )
{
	/* ENOSYS is ok for this, we have no statistics */
	( void )path;
	( void )stbuf;

	return -ENOSYS;
}

static int
ar_rename( const char *from, const char *to )
{
	NODE *node;
	int ret;

	//log( ">>ar_rename: got from: '%s', to: '%s'", from, to );
	if( ! archiveWriteable ) {
		return -EROFS;
	}
	node = get_node_for_path( root, from );
	if( ! node ) {
		return -ENOENT;
	}
	if( node->child ) {
		/* it is a directory, recursive change of all nodes
		 * below it is required */
		ret = rename_recursively( node->child, from, to );
	}
	/* meta data is changed in save() */
	/* change node name */
	free( node->name );
	if( *to != '/' ) {
		node->name = malloc( strlen( to ) + 2 );
		sprintf( node->name, "/%s", to );
	} else {
		node->name = strdup( to );
	}
	node->namechanged = 1;
	remove_child( node );
	ret = insert_by_path( root, node );
	archiveModified = 1;
	return ret;
}

static int
ar_fsync( const char *path, int isdatasync, struct fuse_file_info *fi )
{
	/* Just a stub.  This method is optional and can safely be left
	   unimplemented */
	( void )path;
	( void )isdatasync;
	( void )fi;
	return 0;
}

static int
ar_readlink( const char *path, char *buf, size_t size )
{
	NODE *node;
	const char *tmp;

	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( ! S_ISLNK( archive_entry_mode( node->entry ) ) ) {
		return -ENOLINK;
	}
	tmp = archive_entry_symlink( node->entry );
	snprintf( buf, size, "%s", tmp );
	return 0;
}

static int
ar_open( const char *path, struct fuse_file_info *fi )
{
	NODE *node;

	node = get_node_for_path( root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( fi->flags & O_WRONLY || fi->flags & O_RDWR ) {
		if( ! archiveWriteable ) {
			return -EROFS;
		}
	}
	/* no need to recurse into links since function doesn't do anything */
	/* no need to save a handle here since archives are stream based */
	fi->fh = 0;
	return 0;
}

static int
ar_release( const char *path, struct fuse_file_info *fi )
{
	( void )fi;
	( void )path;
	return 0;
}

static int
ar_readdir( const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi )
{
	NODE *node;
	(void) offset;
	(void) fi;

	//log( "readdir got path: '%s'", path );
	node = get_node_for_path( root, path );
	if( ! node ) {
		log( "path '%s' not found", path );
		return -ENOENT;
	}

	node = node->child;
	while( node ) {
		struct stat st;
		char *name;
		st.st_ino = archive_entry_ino( node->entry );
		st.st_mode = archive_entry_mode( node->entry );
		name = strrchr( node->name, '/' ) + 1;
		if( filler( buf, name, &st, 0 ) )
			break;
		node = node->next;
	}
	return 0;
}

static struct fuse_operations ar_oper = {
	.getattr	= ar_getattr,
	.readlink	= ar_readlink,
	.readdir	= ar_readdir,
	.mknod		= ar_mknod,
	.mkdir		= ar_mkdir,
	.symlink	= ar_symlink,
	.unlink		= ar_unlink,
	.rmdir		= ar_rmdir,
	.rename		= ar_rename,
	.link		= ar_link,
	.chmod		= ar_chmod,
	.chown		= ar_chown,
	.truncate	= ar_truncate,
	.utime		= ar_utime,
	.open		= ar_open,
	.read		= ar_read,
	.write		= ar_write,
	.statfs		= ar_statfs,
	.release	= ar_release,
	.fsync		= ar_fsync,
/*
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
*/
};

void
showUsage()
{
	fprintf( stderr, "Usage: archivemount <fuse-options> <archive> <mountpoint>\n" );
	fprintf( stderr, "Usage:              (-v|--version)\n" );
}

int
main( int argc, char **argv )
{
	int fuse_ret;
	struct stat st;
	char *mtpt;
	char *archiveFile;
	int oldpwd;
	int argi = argc;

	/* parse cmdline args */
	if( argc < 2 ) {
		showUsage();
		exit( EXIT_FAILURE );
	}
	if( strcmp( argv[1], "-v" ) == 0 ||
			strcmp( argv[1], "--version" ) == 0 )
	{
		/* print version information and exit */
		printf( "%d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_RELEASE );
		return EXIT_SUCCESS;
	}
	if( argc < 3 ) {
		showUsage();
		exit( EXIT_FAILURE );
	}
	argi--;
	mtpt = argv[argi];
	argi--;
	archiveFile = argv[argi];
	argv[argi] = mtpt;

	/* check if mtpt is ok and writeable */
	if( stat( mtpt, &st ) != 0 ) {
		perror( "Error stat'ing mountpoint" );
		exit( EXIT_FAILURE );
	}
	if( ! S_ISDIR( st.st_mode ) ) {
		fprintf( stderr, "Problem with mountpoint: %s\n",
				strerror( ENOTDIR ) );
		exit( EXIT_FAILURE );
	}

	/* check if archive is writeable */
	archiveFd = open( archiveFile, O_RDWR );
	if( archiveFd != -1 ) {
		archiveWriteable = 1;
		close( archiveFd );
	}
	/* open archive and read meta data */
	archiveFd = open( archiveFile, O_RDONLY );
	if( archiveFd == -1 ) {
		perror( "opening archive failed" );
		return EXIT_FAILURE;
	}
	build_tree( mtpt );

	/* save directory this was started from */
	oldpwd = open( ".", 0 );

	/* now do the real mount */
	fuse_ret = fuse_main( argc - 1, argv, &ar_oper );

	/* go back to saved dir */
	fchdir( oldpwd );

	/* save changes if modified */
	if( archiveWriteable && archiveModified ) {
		if( save( archiveFile ) != 0 ) {
			fprintf( stderr, "Saving new archive failed\n" );
		}
	}

	/* clean up */
	close( archiveFd );

	return EXIT_SUCCESS;
}

/*
vim:ts=8:softtabstop=8:sw=8:noexpandtab
*/

