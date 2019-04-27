#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ext2fs/ext2fs.h>

#include "linklist.h"
#include "log.h"
#include "debugfs.h"
#include "mod_path.h"

static int modPath_path_len = 0;

void modPath_set_pathLen(int pathLen)
{
	modPath_path_len = pathLen;
}

void addPath(const char *path, int squash_uids, int squash_perms)
{
	size_t len;
	char *full_name = NULL, *lnk = NULL;
	struct dirent *file = NULL;
	DIR *direc = NULL;
	struct stat st;
	int overWrite;
	__u16 inodeType;

	direc = opendir(path);

	if ( !direc )
		log_error("[Local fs opendir] Cannot open directory %s", path);

	while (( file = readdir(direc)) != NULL ) {

		if (( !strcmp(file->d_name, "." )) || ( !strcmp(file->d_name, ".." )))
			continue;

		len = strlen(path) + strlen( file->d_name ) + 3;

		if ( full_name ) {
			free( full_name );
			full_name = NULL;
		}

		full_name = (char*)malloc(len + 2);
		if ( !full_name )
			log_error("[Local fs stat] Memory allocation error ( full_name --> %s/%s )", path, file->d_name);

		memset(full_name, 0, len + 2);
		snprintf(full_name, len, "%s/%s", path, file->d_name);

		lstat(full_name, &st);
		mode_t fmt = st.st_mode & S_IFMT;

		if ( squash_uids )
			st.st_uid = st.st_gid = 0;

		if ( squash_perms )
			st.st_mode &= ~( LINUX_S_IRWXG | LINUX_S_IRWXO );

		overWrite = name_to_inode( file->d_name );

		if ( st.st_nlink > 1 ) {
			if (( lnk = linklist_add(st.st_dev, st.st_ino, full_name + modPath_path_len))) {

				if ( overWrite ) {
					log_action(ACT_RM, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
					if ( !do_rm(file->d_name))
						log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), file->d_name);
				}

				log_action(ACT_HARDLINK, file->d_name, lnk, 0, 0, 0, 0, 0, 0, overWrite);
				do_hardlink(lnk, file->d_name);
				continue;
			}
		}

		if ( overWrite )
			inodeType = inode_mode( file->d_name );

		if (( fmt == S_IFDIR ) && ( overWrite ) && ( !LINUX_S_ISDIR(inodeType)))
			log_error("[Remote fs mismatch] %s/%s exists but isn't a directory when it should be.", log_cwd(), file->d_name);
		else if (( fmt != S_IFDIR) && ( overWrite )) {
			if ( LINUX_S_ISDIR(inodeType))
				log_error("[Remote fs mismatch] %s/%s exists but is a directory when it shouldn't be.", log_cwd(), file->d_name);

			if ((!LINUX_S_ISREG(inodeType)) && (!LINUX_S_ISLNK(inodeType)) &&
			    (!LINUX_S_ISBLK(inodeType)) && (!LINUX_S_ISCHR(inodeType)) &&
			    (!LINUX_S_ISFIFO(inodeType)) && (!LINUX_S_ISSOCK(inodeType)))
				log_error("[Remote fs mismatch] Existing file %s/%s has unknown/unsupported type [0x%x].", log_cwd(), file->d_name);
		}

		switch ( fmt ) {
		case S_IFDIR:         // Directory
			log_action(ACT_MKDIR, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);

			if ( !overWrite )
				if ( !do_mkdir( file->d_name ))
					log_error("[Filesystem error] cannot mkdir %s/%s", log_cwd(), file->d_name);

			log_action(ACT_CHMODE, file->d_name, NULL, st.st_mode, 0, 0, 0, 0, 0, overWrite);
			if ( !do_chmode(file->d_name, st.st_mode))
				log_error("[Filesystem error] Failed to chmode 0x%x for directory %s/%s", st.st_mode, log_cwd(), file->d_name);
			log_action(ACT_CHOWN, file->d_name, NULL, 0, st.st_uid, st.st_gid, 0, 0, 0, 0);
			if ( !do_chown(file->d_name, st.st_uid, st.st_gid))
				log_error("[Filesystem error] Failed to chown %ld, %ld for directory %s/%s", st.st_uid, st.st_gid, log_cwd(), file->d_name);

			log_action(ACT_CHDIR, file->d_name, NULL, 0, 0, 0, 0, 0, 0, 0);
			if ( !do_chdir( file->d_name ))
				log_error("[Filesystem error] cannot chdir to newly created %s/%s", log_cwd(), file->d_name);

			addPath(full_name, squash_uids, squash_perms);

			log_action(ACT_CHDIR, "..", NULL, 0, 0, 0, 0, 0, 0, 0);
			if ( !do_chdir(".."))
				log_error("[Filesystem error] cannot chdir to parent directory");
			break;

		case S_IFREG:         // Regular file

			if ( overWrite ) {
				log_action(ACT_RM, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
				if ( !do_rm(file->d_name))
					log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), file->d_name);
			}

			log_action(ACT_WRITE, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
			if ( !do_write(full_name, file->d_name))
				log_error("[Filesystem error] cannot write %s/%s", log_cwd(), file->d_name);
			break;

		case S_IFLNK:         // Symbolic link

			lnk = (char*)malloc(MAX_PATHSIZE + 2);
			if ( !lnk )
				log_error("[symlink] Memory allocation error (lnk)");
			int len = readlink(full_name, lnk, MAX_PATHSIZE);
			if ( len == -1 ) {
				free(lnk);
				log_error("[Local filesystem error] Cannot read destination for link %s/%s", log_cwd(), file->d_name);
			} else lnk[len] = '\0';

			if ( overWrite ) {
				log_action(ACT_RM, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
				if ( !do_rm(file->d_name))
					log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), file->d_name);
			}

			log_action(ACT_SYMLINK, file->d_name, lnk, 0, 0, 0, 0, 0, 0, overWrite);
			if ( !do_symlink(lnk, file->d_name))
				log_error("[Filesystem error] cannot symlink %s/%s --> %s", log_cwd(), file->d_name, lnk);

			free(lnk);
			break;

		case S_IFBLK:           // Block device node
		case S_IFCHR:           // Character device node
		case S_IFSOCK:          // socket
		case S_IFIFO:           // fifo

			if ( overWrite ) {
				log_action(ACT_RM, file->d_name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
				if ( !do_rm(file->d_name))
					log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), file->d_name);
			}

			char nodetype = ( fmt == S_IFBLK ? 'b' : ( fmt == S_IFCHR ? 'c' : ( fmt == S_IFSOCK ? 's' : 'p' )));
			unsigned long major = 0, minor = 0;

			if (( nodetype == 'b' ) || ( nodetype == 'c' )) {
				major = (long)major(st.st_rdev);
				minor = (long)minor(st.st_rdev);
			}

			log_action(ACT_MKNOD, file->d_name, NULL, 0, 0, 0, nodetype, major, minor, overWrite);
			if ( !do_mknod(file->d_name, nodetype, major, minor))
				log_error("[Filesystem error] cannot mknod %c %ld,%ld %s/%s", log_cwd(), nodetype, major, minor, log_cwd(), file->d_name);
			break;
		}

		if ( fmt != S_IFDIR ) { // Not dir ?
			log_action(ACT_CHMODE, file->d_name, NULL, st.st_mode, 0, 0, 0, 0, 0, overWrite);
			if ( !do_chmode(file->d_name, st.st_mode))
				log_error("[Filesystem error] Failed to chmode 0x%x for file %s/%s", st.st_mode, log_cwd(), file->d_name);
			log_action(ACT_CHOWN, file->d_name, NULL, 0, st.st_uid, st.st_gid, 0, 0, 0, 0);
			if ( !do_chown(file->d_name, st.st_uid, st.st_gid))
				log_error("[Filesystem error] Failed to chown %ld, %ld for file %s/%s", st.st_uid, st.st_gid, log_cwd(), file->d_name);
		}

		if ( full_name ) {
			free( full_name );
			full_name = NULL;
		}
	}

	closedir(direc);

}
