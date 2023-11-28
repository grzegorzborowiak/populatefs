#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <libgen.h>
#include <ext2fs/ext2fs.h>

#include "log.h"
#include "debugfs.h"
#include "mod_file.h"
#include "util.h"

void addFilespec(FILE *fd, int squash_uids, int squash_perms)
{

	long mode, uid, gid, major, minor, i;
	long start, increment, count;
	char *c, *dir, *name, *dname = NULL, *path = NULL, *line = NULL;
	char type;
	size_t len;
	int overWrite = 0, lineno = 0;
	__u16 inodeType;

	while ( getline(&line, &len, fd) >= 0 ) {
		type = mode = uid = gid = major = minor = start = count = overWrite = 0;
		increment = 1;
		lineno++;

		if (( c = strchr(line, '#'))) {
			*c = 0;
		}

		path = line;

		char * next = nextToken(line, line);

		char * token;

		next = nextToken(next, token = next);
		type = token[0];

		next = nextToken(next, token = next);
		mode = atoi(token);

		next = nextToken(next, token = next);
		uid = atoi(token);

		next = nextToken(next, token = next);
		gid = atoi(token);

		next = nextToken(next, token = next);
		major = atoi(token);

		next = nextToken(next, token = next);
		minor = atoi(token);

		next = nextToken(next, token = next);
		start = atoi(token);

		next = nextToken(next, token = next);
		increment = atoi(token);

		next = nextToken(next, token = next);
		count = atoi(token);

		if ( squash_uids ) uid = gid = 0;

		name = basename( path );
		dir = dirname( path );

		if (( !strcmp(name, ".")) || ( !strcmp(name, "..")) || ( !strcmp(name, "/"))) {
			log_warning("device table[%d]: [skip]", lineno);
			continue;
		}

		log_action(ACT_CHDIR, dir, NULL, 0, 0, 0, 0, 0, 0, 0);
		if ( !do_chdir(dir)) {
			log_warning("device table[%d]: target directory '%s' for entry '%s' does not exist [skip]", lineno, dir, name);
			log_action(ACT_CHDIR, "/", NULL, 0, 0, 0, 0, 0, 0, 0);
			if ( !do_chdir("/"))
				log_error("[Filesystem error] cannot chdir to root");
			continue;
		}

		if (( type != 'd' ) && ( type != 'f' ) && ( type != 'p' ) && ( type != 'c' ) && ( type != 'b' ) && ( type != 's')) {
			log_warning("device table[%d]: bad type '%c' for entry '%s' [skip]", lineno, type, name);
			continue;
		}

		if (squash_perms) {
			mode &= ~( LINUX_S_IRWXG | LINUX_S_IRWXO);
		}

		int index_len = 10;
		int name_len = strlen(name);

		dname = malloc(name_len + index_len + 1);
			
		char * dname_index = dname + name_len;

		strcpy(dname, name);

		if ( count == 0 ) {
			start = -1;
		}

		for ( i = start; i < count; i++ ) {
			if ( count > 0 ) {
				snprintf(dname_index, index_len, "%ld", i);
			}

			if (( overWrite = name_to_inode(dname)))
				inodeType = inode_mode(dname);

			if (( type == 'd' ) && ( overWrite ) && ( !LINUX_S_ISDIR(inodeType)))
				log_error("[Remote fs mismatch] %s/%s exists but isn't a directory when it should be.", log_cwd(), dname);
			else if (( type != 'd' ) && ( overWrite )) {

				if ( LINUX_S_ISDIR(inodeType))
					log_error("[Remote fs mismatch] %s/%s exists but is a directory when it shouldn't be.", log_cwd(), dname);

				if ((!LINUX_S_ISREG(inodeType)) && (!LINUX_S_ISLNK(inodeType)) &&
					(!LINUX_S_ISBLK(inodeType)) && (!LINUX_S_ISCHR(inodeType)) &&
					(!LINUX_S_ISFIFO(inodeType)) && (!LINUX_S_ISSOCK(inodeType)))
					log_error("[Remote fs mismatch] Existing file %s/%s has unknown/unsupported type [0x%x].", log_cwd(), dname, inodeType);
			}

			switch ( type ) {
			case 'd':
				log_action(ACT_MKDIR, dname, NULL, 0, 0, 0, 0, 0, 0, overWrite);

				if ( !overWrite )
					if ( !do_mkdir(dname))
						log_error("[Filesystem error] cannot mkdir %s/%s", log_cwd(), dname);

				break;

			case 'c':
			case 'b':
				if ( overWrite ) {
					log_action(ACT_RM, dname, NULL, 0, 0, 0, 0, 0, 0, overWrite);
					if ( !do_rm(dname))
						log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), dname);
				}

				log_action(ACT_MKNOD, dname, NULL, 0, 0, 0, type, major, minor + ((i * increment) - start), overWrite);
				if ( !do_mknod(dname, type, major, minor + ((i * increment) - start)))
					log_error("[Filesystem error] cannot mknod %s/%s", log_cwd(), dname);
				break;

			case 's':
			case 'p':

				if ( overWrite ) {
					log_action(ACT_RM, dname, NULL, 0, 0, 0, 0, 0, 0, overWrite);
					if ( !do_rm(dname))
						log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), dname);
				}

				log_action(ACT_MKNOD, dname, NULL, 0, 0, 0, type, 0, 0, overWrite);
				if ( !do_mknod(dname, type, 0, 0))
					log_error("[Filesystem error] cannot mknod %s/%s", log_cwd(), dname);

				break;
			}

			log_action(ACT_CHMOD, dname, NULL, mode, 0, 0, 0, 0, 0, 0);
			if ( !do_chmod(dname, mode))
				log_error("[Filesystem error] cannot chmod %s/%s", log_cwd(), dname);
			log_action(ACT_CHOWN, dname, NULL, 0, uid, gid, 0, 0, 0, 0);
			if ( !do_chown(dname, uid, gid))
				log_error("[Filesystem error] cannot chown %s/%s", log_cwd(), dname);
		}

		log_action(ACT_CHDIR, "/", NULL, 0, 0, 0, 0, 0, 0, 0);
		if ( !do_chdir("/"))
			log_error("[Filesystem error] cannot chdir to root");
		free(dname);
		dname = NULL;

	}


	if ( line ) {
		free( line ); line = NULL;
	}

}
