populatefs 1.0
==========

Is a nearly drop-in replace for genext2fs.

Image generation
================
To produce a something like 42mb image:
dd if=/dev/zero of=root.ext4 seek=42720 count=0 bs=1k
mkfs.ext4 -F -E root_owner=0:0 -L [Label] -N [inode count] -O [ext4 options] -m 0 root.ext4

I made the util for OpenWrt and Raspberry Pi, so I might use..
[Label] = OpenWrt
[inode count] = 8096
[ext4 options] = has_journal,ext_attr,resize_inode,dir_index,filetype,extent,flex_bg,sparse_super,large_file,uninit_bg,dir_nlink,extra_isize

and then I will use populatefs to populate empty filesystem.
populatefs -U -d [directory containing files to go into root] -D devtable.txt root.ext4

Usage
=====
Usage: ./populatefs [options] image
Manipulate disk image from directories/files

 -d <directory>   Add the given directory and contents at a particular path to root
 -D <file>        Add device nodes and directories from filespec
 -U               Squash owners making all files be owner by root:root
 -P               Squash permissions on all files
 -q               Same as "-U -P"
 -h               Display this usage guide
 -V               Show version
 -v               Be verbose
 -w               Be even more verbose
 
Advanced use
============
You can add multiple directories of filespecs with multiple -d or -D options. These are processed in the order you give 'em to command-line. If these sources contain files that will overlap, previously added files will be overwritten, unless, previously added file is a directory, or if source is trying to add a directory and that filename is previously used with file, error will be printed out. Rules are simple: do not overwrite directory with file, or file with directory.

Compilation
===========
I did not provide a configure script. Everything choice is given as environment variables. For example:
CC=gcc make
or
CC=gcc DESTDIR=/tmp make install

Study the values from Makefile.
If your system did not ship or does not have getopt.. Try:
EXTRA_CFLAGS="" make

Requirements
============
libext2fs and libmath must be available for compilation.
For runtime, kernel must have support for debugfs.

Credits
=======
Partly a resize of original genext2fs and debugfs from e2fsprogs
