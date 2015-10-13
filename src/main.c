#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"
#include "linklist.h"
#include "log.h"
#include "mod_file.h"
#include "mod_path.h"

#define MAX_SOURCES 128

int main(int argc, char **argv)
{

	const char *usage =
		"Usage: %s [options] image\r\n"
		"Manipulate disk image from directories/files\r\n\n"
		" -d <directory>   Add the given directory and contents at a particular path to root\r\n"
		" -D <file>        Add device nodes and directories from filespec\r\n"
		" -U               Squash owners making all files be owner by root:root\r\n"
		" -P               Squash permissions on all files\r\n"
		" -q               Same as \"-U -P\"\r\n"
		" -h               Display this usage guide\r\n"
		" -V               Show version\r\n"
		" -v               Be verbose\r\n"
		" -w               Be even more verbose\r\n"
		"\n";

	const char *version =
		"populatefs %s%s\n";

	int c;
	int sourcenum = 0;
	char *source[MAX_SOURCES];
	FILE *fd;
	int squash_uids = 0;
	int squash_perms = 0;
	int firstRun = 1;

	setLoggingLevel(LOG_OFF);

	while ((c = getopt(argc, argv, "d:D:UPqVvw")) != EOF) {
		switch (c) {
		case 'd':
		case 'D':
			source[sourcenum] = optarg;

			// Remove trailing slash from path name..
			if (( source[sourcenum][strlen(source[sourcenum]) - 1] == '/' ) && ( source[sourcenum][1] != 0 ))
				source[sourcenum][strlen(source[sourcenum]) - 1] = 0;

			sourcenum++;
			break;
		case 'U':
			squash_uids = 1;
			break;
		case 'P':
			squash_perms = 1;
			break;
		case 'q':
			squash_uids = squash_perms = 1;
			break;
		case 'v':
			setLoggingLevel(LOG_ON);
			break;
		case 'w':
			setLoggingLevel(LOG_VERBOSE);
			break;
		case 'V':
			printf(version, POPULATEFS_VERSION, POPULATEFS_EXTRAVERSION);
			exit(0);
		default:
			printf(usage, argv[0]);
			return 1;
		}
	}

	if ((( sourcenum == 0 ) && ( squash_uids == 0 )) || ( optind != argc - 1 )) {
		printf(usage, argv[0]);
		return 1;
	}

	if ( logLevel())
		printf(version, POPULATEFS_VERSION, POPULATEFS_EXTRAVERSION);

	if ( !fs_isClosed())
		log_error("[Filesystem error] Filesystem image %s already open.", argv[optind]);

	if ( !open_filesystem(argv[optind]))
		log_error("[Filesystem error] %s cannot be opened.", argv[optind]);

	if ( !fs_isReadWrite())
		log_error("[Filesystem error] %s cannot be opened with write priviledges.", argv[optind]);

	for (c = 0; c < sourcenum; c++) {
		struct stat st;
		stat(source[c], &st);

		if (( st.st_mode & S_IFMT ) == S_IFREG )
			printf("Populating filesystem from filespec (%s)\r\n", source[c]);
		else if (( st.st_mode & S_IFMT ) == S_IFDIR )
			printf("Populating filesystem from path (%s)\r\n", source[c]);
		else
			log_error("%s is neither a file nor existing path.", source[c]);

		log_action(ACT_CHDIR, "/", NULL, 0, 0, 0, 0, 0, 0, 0);
		do_chdir("/");
		switch (st.st_mode & S_IFMT) {
		case S_IFREG:
			if (( fd = fopen(source[c], "rb")) == NULL)
				log_error("Cannot open %s for reading.", source[c]);
			if ( firstRun ) {
				log_action(ACT_CHOWN, "/", NULL, 0, squash_uids ? 0 : st.st_uid, squash_uids ? 0 : st.st_gid, 0, 0, 0, 0);
				do_chown(".", squash_uids ? 0 : st.st_uid, squash_uids ? 0 : st.st_gid);
				firstRun = 0;
			}
			addFilespec(fd, 0, 0);
			fclose(fd);
			break;
		case S_IFDIR:
			modPath_set_pathLen(strlen(source[c]));
			addPath(source[c], squash_uids, squash_perms);
			linklist_release();
			break;
		}
	}

	if ( !close_filesystem())
		log_error("[Filesystem error] Cannot close %s properly.", argv[optind]);

	purge_warnings();
	printf("Filesystem %s populated.\r\n", argv[optind]);

	return 0;
}
