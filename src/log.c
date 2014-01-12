#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include "log.h"

#define LOG_MSG_MAXLEN 512

typedef struct warning_node {
	char                    *str;
	struct warning_node     *next;
} warning_nodeT;

static int logging_level = 0;
static char *logPath = NULL;
static char logPathRoot[1] = "\0";
warning_nodeT *warning_list = NULL;

int logLevel(void)
{
	return logging_level;
}

void setLoggingLevel(int state)
{
	logging_level = state < 1 ? 0 : ( state == 1 ? 1 : 2 );
}

void log_error(const char *s, ...)
{
	va_list p;

	va_start(p, s);

	if ( warning_list ) {
		printf("\r\n\n");
		purge_warnings();
		printf("\r\n");
	}
	printf("Error: ");
	vprintf(s, p);
	printf("\r\n");
	fflush(stdout);
	exit(-1);
}

void internal_log_chdir(char *path)
{
	size_t pathLen;

	if ( path[0] == '/' ) { // chdir to path
		if ( logPath )
			free( logPath );

		logPath = NULL;

		if ( strcmp(path, "/") == 0 ) // Go to root?
			return;

		pathLen = strlen(path) - ( path[strlen(path) - 1] == '/' ? 1 : 0 );

		logPath = (char*)malloc(pathLen + 1);

		if ( !logPath )
			log_error("[log/chdir] Memory allocation error (logPath --> %s)", path);

		memset(logPath, 0, pathLen + 1);
		snprintf(logPath, pathLen + 1, path);
	} else { // chdir to directory in current path

		char *tmpPath = NULL;

		if (( strlen(path) >= 2 ) && ( strncmp(path, "..", 2) == 0 )) { // Go to parent direcotry
			if ( !logPath )                                         // Cannot go to parent directory
				return;                                         // because we are already in root

			pathLen = strlen(logPath) - 1;
			while (( pathLen > 0 ) && ( logPath[pathLen] != '/' ))
				pathLen--;

			if ( pathLen == 0 ) { // Parent directory = root directory
				free(logPath);
				logPath = NULL;
				return;
			}

			tmpPath = (char*)malloc(pathLen + 2);

			if ( !tmpPath )
				log_error("[log/chdir] Memory allocation error (tmpPath --> %s)", path);

			memset(tmpPath, 0, pathLen + 2);
			snprintf(tmpPath, pathLen + 1, logPath);
			free( logPath );
			logPath = NULL;
			logPath = (char*)malloc(strlen(tmpPath) + 1);
			memset(logPath, 0, strlen(tmpPath) + 1);
			snprintf(logPath, pathLen + 1, tmpPath);
			free(tmpPath );
			tmpPath = NULL;

			if (( strlen(path) > 3 ) && ( path[2] == '/' )) // Continue to another directory that's in the parent
				internal_log_chdir(path + 3);           // that we just entered

		} else {                                                // Enter directory

			if ( logPath ) {

				tmpPath = (char*)malloc( strlen(logPath) + strlen(path) + 2 );

				if ( !tmpPath )
					log_error("[log/chdir] Memory allocation error (tmpPath --> %s)", path);

				sprintf(tmpPath, "%s/%s", logPath, path);
				free( logPath );
				logPath = NULL;

				pathLen = strlen(tmpPath) - ( path[strlen(path) - 1] == '/' ? 1 : 0 );
				logPath = (char*)malloc(pathLen + 1);
				if ( !logPath )
					log_error("[log/chdir] Memory allocation error (logPath --> %s)", path);

				memset(logPath, 0, pathLen + 1);
				snprintf(logPath, pathLen + 1, tmpPath);
				free( tmpPath );
				tmpPath = NULL;
			} else {

				tmpPath = (char*)malloc( strlen(path) + 2 );

				if ( !tmpPath )
					log_error("[log/chdir] Memory allocation error (tmpPath)");

				sprintf(tmpPath, "/%s", path);
				pathLen = strlen(tmpPath) - ( path[strlen(path) - 1] == '/' ? 1 : 0);
				logPath = (char*)malloc(pathLen + 1);
				if ( !logPath )
					log_error("[log/chdir] Memory allocation error (logPath --> %s)", path);

				memset(logPath, 0, pathLen + 1);
				snprintf(logPath, pathLen + 1, tmpPath);
				free( tmpPath );
				tmpPath = NULL;
			}
		}
	}
}

void log_action(int actiontype, char *fname, char *target,
		mode_t mode, unsigned long uid, unsigned long gid,
		int type, unsigned long major, unsigned long minor,
		int overwrite )
{

	if ( actiontype == ACT_CHDIR )
		internal_log_chdir(fname);

	if ( logging_level < 1 )
		return;

	int type2;
	switch (type) {
	case EXT2_FT_REG_FILE: type2 = 'f'; break;
	case EXT2_FT_FIFO: type2 = 'p'; break;
	case EXT2_FT_SOCK: type2 = 's'; break;
	case EXT2_FT_CHRDEV: type2 = 'c'; break;
	case EXT2_FT_BLKDEV: type2 = 'b'; break;
	default: type2 = type;
	}

	switch ( actiontype ) {
	case ACT_WRITE:
		if ((( logging_level == 1 ) && ( overwrite )) || ( !overwrite ))
			printf("[%c][write]     %s/%s\r\n", overwrite ? '!' : 'x', log_cwd(), fname);
		break;
	case ACT_MKNOD:
		if ((( logging_level == 1 ) && ( overwrite )) || ( !overwrite )) {
			if ( type2 == 'f' )
				printf("[x][touch]    %s/%s\r\n", log_cwd(), fname);
			else
				printf("[%c][mknod]    %s/%s %c:%ld,%ld\r\n", overwrite ? '!' : 'x', log_cwd(), fname, type2, major, minor);
		}
		break;
	case ACT_MKDIR:
		printf("[%c][mkdir]    %s/%s\r\n", overwrite ? '-' : 'd', log_cwd(), fname);
		break;
	case ACT_TOUCH:
		if ((( logging_level == 1 ) && ( overwrite )) || ( !overwrite ))
			printf("[%c][touch]    %s/%s\r\n", overwrite ? '!' : 'x', log_cwd(), fname);
		break;
	case ACT_RM:
		if (( logging_level == 1 ) && ( !overwrite ))
			printf("[x][rm]       %s/%s\r\n", log_cwd(), fname);
		break;
	case ACT_SYMLINK:
		if ((( logging_level == 1 ) && ( overwrite )) || ( !overwrite ))
			printf("[%c][symlink]  %s/%s --> %s\r\n", overwrite ? '!' : 'x', log_cwd(), fname, target);
		break;
	case ACT_HARDLINK:
		if ((( logging_level == 1 ) && ( overwrite )) || ( !overwrite ))
			printf("[%c][hardlink] %s/%s --> %s\r\n", overwrite ? '!' : 'x', log_cwd(), fname, target);
		break;
	}

	if ( logging_level < 2 )
		return;

	switch ( actiontype ) {
	case ACT_WRITE:
		if ( overwrite )
			printf("[x][write]     %s/%s\r\n", log_cwd(), fname);
		break;
	case ACT_MKNOD:
		if ( overwrite ) {
			if ( type2 == 'f' )
				printf("[x][touch]    %s/%s\r\n", log_cwd(), fname);
			else
				printf("[x][mknod]    %s/%s %c:%ld,%ld\r\n", log_cwd(), fname, type2, major, minor);
		}
		break;
	case ACT_TOUCH:
		if ( overwrite )
			printf("[x][touch]    %s/%s\r\n", log_cwd(), fname);
		break;
	case ACT_CHMOD:
		printf("[p][chmod]    %s/%s %d\r\n", log_cwd(), fname, mode);
		break;
	case ACT_CHOWN:
		printf("[o][chown]    %s/%s %ld:%ld\r\n", log_cwd(), fname, uid, gid);
		break;
	case ACT_CHMODE:
		printf("[t][filetype] %s/%s: ", log_cwd(), fname);
		switch (( mode ) & LINUX_S_IFMT ) {
		case LINUX_S_IFLNK:  printf("SYMBOLIC LINK\r\n"); break;
		case LINUX_S_IFREG:  printf("REGULAR FILE\r\n"); break;
		case LINUX_S_IFDIR:  printf("DIRECTORY\r\n"); break;
		case LINUX_S_IFCHR:  printf("CHARACTER DEVICE NODE\r\n"); break;
		case LINUX_S_IFBLK:  printf("BLOCK DEVICE NODE\r\n"); break;
		case LINUX_S_IFIFO:  printf("FIFO\r\n"); break;
		case LINUX_S_IFSOCK: printf("SOCKET\r\n"); break;
		default: printf("UNKNOWN FILETYPE\r\n");
		}
		printf("[p][chmod]    %s/%s %04o\r\n", log_cwd(), fname, mode & 0777);
		break;
	case ACT_CHDIR:
		printf("[-][chdir]    %s\r\n", logPath ? logPath : "[fs root]");
		break;
	case ACT_RM:
		printf("[x][rm]       %s/%s\r\n", log_cwd(), fname);
		break;
	case ACT_SYMLINK:
		if ( overwrite )
			printf("[x][symlink]  %s/%s --> %s\r\n", log_cwd(), fname, target);
		break;
	case ACT_HARDLINK:
		if ( overwrite )
			printf("[x][hardlink]  %s/%s --> %s\r\n", log_cwd(), fname, target);
		break;
	}
}

char *log_cwd(void)
{
	return logPath ? logPath : logPathRoot;
}

int log_inRoot(void)
{
	return logPath ? 1 : 0;
}

void log_warning(const char *s, ...)
{
	char *tmpS = NULL;
	va_list p;
	warning_nodeT *node;
	size_t len;

	tmpS = (char*)malloc(LOG_MSG_MAXLEN + 2);
	if ( !tmpS )
		log_error("[log/log_warning] Memory allocation error (tmpS)");

	va_start(p, s);
	vsnprintf(tmpS, LOG_MSG_MAXLEN, s, p);

	if ( !warning_list ) {
		warning_list = (warning_nodeT*)malloc(sizeof(warning_nodeT));
		node = warning_list;
	} else {
		node = warning_list;
		while ( node->next != NULL )
			node = node->next;
		node->next = (warning_nodeT*)malloc(sizeof(warning_nodeT));
		node = node->next;
	}

	len = strlen(tmpS);
	node->str = (char*)malloc(len + 1);
	memset(node->str, 0, len + 1);
	snprintf(node->str, len + 1, "%s", tmpS);
	node->next = NULL;
	free( tmpS );
	tmpS = NULL;
}

char *get_single_warning(void)
{
	if ( !warning_list )
		return NULL;

	return warning_list->str;
}

void clear_single_warning(void)
{
	if ( !warning_list )
		return;

	warning_nodeT *node = warning_list;
	if ( node->next == NULL )
		warning_list = NULL;
	else
		warning_list = node->next;
	node->next = NULL;
	free( node->str );
	node->str = NULL;
	free( node );
}

void purge_warnings(void)
{
	while ( warning_list ) {
		warning_nodeT *node = warning_list;
		warning_list = node->next;
		printf("Warning: %s\r\n", node->str);
		node->next = NULL;
		free( node->str );
		node->str = NULL;
		free( node );
		node = NULL;
	}
	warning_list = NULL;
}

void clear_warnings(void)
{
	while ( warning_list ) {
		warning_nodeT *node = warning_list;
		warning_list = node->next;
		node->next = NULL;
		free( node->str );
		node->str = NULL;
		free( node );
		node = NULL;
	}
	warning_list = NULL;
}

void release_log(void)
{
	if ( logPath ) {
		free(logPath);
		logPath = NULL;
	}
	clear_warnings();
}
