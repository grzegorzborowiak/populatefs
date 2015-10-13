#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <libgen.h>
#include <ext2fs/ext2fs.h>

#include "log.h"
#include "debugfs.h"
#include "mod_file.h"

#if 0 // C99 define "a" for floating point, so you can have runtime surprise varying from the version of the library
# define SCANF_PREFIX "a"
# define SCANF_STRING(s) (&s)
#else
# define SCANF_PREFIX "511"
# define SCANF_STRING(s) (s = malloc(512))
#endif

void addFilespec(FILE *fd, int squash_uids, int squash_perms)
{

	unsigned long rmode, mode, uid, gid, major, minor, i;
	unsigned long start, increment, count, octmode, decmode;
	char *c, *dir, *name, *dname = NULL, *path = NULL, *path2 = NULL, *line = NULL;
	char type;
	size_t len;
	int argv, i2, overWrite = 0, lineno = 0;
	__u16 inodeType;

	while ( getline(&line, &len, fd) >= 0 ) {
		rmode = mode = uid = gid = major = minor = start = count = overWrite = 0;
		increment = 1;
		lineno++;

		if (( c = strchr(line, '#'))) *c = 0;
		if ( path ) {
			free( path ); path = NULL;
		}
		if ( path2 ) {
			free( path2 ); path2 = NULL;
		}

		argv = sscanf(line, "%" SCANF_PREFIX "s %c %ld %lu %lu %lu %lu %lu %lu %lu",
			      SCANF_STRING(path), &type, &rmode, &uid, &gid, &major, &minor,
			      &start, &increment, &count);

		if ( argv < 3 ) {
			if ( argv > 0 )
				log_warning("device table[%d]: bad format for entry '%s' [skip]", lineno, path);
			continue;
		}

		i2 = 0;
		octmode = rmode;
		decmode = 0;
		while ( octmode != 0 ) {
			decmode = decmode + (octmode % 10) * pow(8, i2++);
			octmode = octmode / 10;
		}

		if ( squash_uids ) uid = gid = 0;

		mode = decmode;
		path2 = strdup( path );
		name = basename( path );
		dir = dirname( path2 );

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
			mode &= ~( LINUX_S_IRWXG | LINUX_S_IRWXO );
			rmode &= ~( LINUX_S_IRWXG | LINUX_S_IRWXO);
		}

		if ( count > 0 ) {

			if ( dname ) {
				free( dname );
				dname = NULL;
			}

			unsigned len;
			len = strlen(name) + 10;
			dname = malloc(len + 1);

			for ( i = start; i < count; i++ ) {
				snprintf(dname, len, "%s%lu", name, i);

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
					mode |= LINUX_S_IFDIR;
					log_action(ACT_MKDIR, dname, NULL, 0, 0, 0, 0, 0, 0, overWrite);

					if ( !overWrite )
						if ( !do_mkdir(dname))
							log_error("[Filesystem error] cannot mkdir %s/%s", log_cwd(), dname);

					break;

				case 'c':
				case 'b':
					if ( type == 'c' )
						mode |= LINUX_S_IFCHR;
					else
						mode |= LINUX_S_IFBLK;

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
					if ( type == 's' )
						mode |= LINUX_S_IFSOCK;
					else mode |= LINUX_S_IFIFO;

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

				log_action(ACT_CHMOD, dname, NULL, rmode, 0, 0, 0, 0, 0, 0);
				if ( !do_chmod(dname, rmode))
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

		} else {

			if (( overWrite = name_to_inode(name)))
				inodeType = inode_mode(name);

			if (( type == 'd' ) && ( overWrite ) && ( !LINUX_S_ISDIR(inodeType)))
				log_error("[Remote fs mismatch] %s/%s exists but isn't a directory when it should be.", log_cwd(), dname);
			else if ( type != 'd' ) {
				if (( overWrite ) && ( LINUX_S_ISDIR(inodeType)))
					log_error("[Remote fs mismatch] %s/%s exists but is a directory when it shouldn't be.", log_cwd(), dname);

				if (( overWrite ) && (!LINUX_S_ISREG(inodeType)) && (!LINUX_S_ISLNK(inodeType)) &&
				    (!LINUX_S_ISBLK(inodeType)) && (!LINUX_S_ISCHR(inodeType)) &&
				    (!LINUX_S_ISFIFO(inodeType)) && (!LINUX_S_ISSOCK(inodeType)))
					log_error("[Remote fs mismatch] Existing file %s/%s has unknown/unsupported type [0x%x].",  log_cwd(), dname, inodeType);
			}

			switch ( type ) {
			case 'd':
				mode |= LINUX_S_IFDIR;
				log_action(ACT_MKDIR, name, NULL, 0, 0, 0, 0, 0, 0, overWrite);

				if ( !overWrite )
					if ( !do_mkdir(name))
						log_error("[Filesystem error] cannot mkdir %s/%s", log_cwd(), name);
				break;

			case 'c':
			case 'b':
				if ( type == 'c' )
					mode |= LINUX_S_IFCHR;
				else
					mode |= LINUX_S_IFBLK;

				if ( overWrite ) {
					log_action(ACT_RM, name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
					if ( !do_rm(name))
						log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), name);
				}

				log_action(ACT_MKNOD, name, NULL, 0, 0, 0, type, major, minor, overWrite);
				if ( !do_mknod(name, type, major, minor))
					log_error("[Filesystem error] cannot mknod %s/%s", log_cwd(), name);
				break;

			case 's':
			case 'p':
				if ( type == 's' )
					mode |= LINUX_S_IFSOCK;
				else mode |= LINUX_S_IFIFO;

				if ( overWrite ) {
					log_action(ACT_RM, name, NULL, 0, 0, 0, 0, 0, 0, overWrite);
					if ( !do_rm(name))
						log_error("[Filesystem error] cannot rm %s/%s", log_cwd(), name);
				}

				log_action(ACT_MKNOD, name, NULL, 0, 0, 0, type, 0, 0, overWrite);
				if ( !do_mknod(name, type, 0, 0))
					log_error("[Filesystem error] cannot mknod %s/%s", log_cwd(), name);
				break;
			}

			log_action(ACT_CHMOD, name, NULL, rmode, 0, 0, 0, 0, 0, 0);
			if ( !do_chmod(name, rmode))
				log_error("[Filesystem error] cannot chmod %s/%s", log_cwd(), name);
			log_action(ACT_CHOWN, name, NULL, 0, uid, gid, 0, 0, 0, 0);
			if ( !do_chown(name, uid, gid))
				log_error("[Filesystem error] cannot chown %s/%s", log_cwd(), name);
			log_action(ACT_CHDIR, "/", NULL, 0, 0, 0, 0, 0, 0, 0);
			if ( !do_chdir("/"))
				log_error("[Filesystem error] cannot chdir to root");
		}

	}


	if ( line ) {
		free( line ); line = NULL;
	}
	if ( path ) {
		free( path ); path = NULL;
	}
	if ( path2 ) {
		free( path2 ); path2 = NULL;
	}

}
