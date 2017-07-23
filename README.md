# Catalog File System

Aim is to develop catalog file system using FUSE (Filesystem in Userspace) framework. Basic idea is to view file listings(catalog) of removable storage, as a filesytem. This allows, firstly to view the catalog as a browsable filesytem, and secondly to use most of find (unix search utility) searching capabilities. Major difference with other filesystems is that the actual file contents are not available, but all possible attributes of the file are available. Multiple catalogs can be viewed as browsable filesystem simultaneously. 

## Technologies used are 
1. gcc
2. fuse
3. gdb
4. gcov
5. valgrind

## Status

This is stable snapshot of project.
Program works perfectly in daemon as well as (Fuse)debug mode.
catalogfsmount file is added to ease, mounting catalog_file using catalogfs.

## Features
1. Catalog_file contents can be dynamically (properly)changed. 
The program will automatically take the updated catalog_file contents.

## Future work

### Possible extensions:

1.Create one README_CFS file(in root path of Filesystem) in which user can write notes for the catalog.
The user will be open only this file. But cannot write to it.

2.Store & show extended attributes of file.

