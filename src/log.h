#ifndef __log__h__
#define __log__h__

#include <sys/types.h>

#define ACT_WRITE 1
#define ACT_MKNOD 2
#define ACT_MKDIR 3
#define ACT_TOUCH 4
#define ACT_CHMOD 5
#define ACT_CHOWN 6
#define ACT_CHMODE 7
#define ACT_CHDIR 8
#define ACT_RM 9
#define ACT_SYMLINK 10
#define ACT_HARDLINK 11

#define LOG_OFF 0
#define LOG_ON 1
#define LOG_VERBOSE 2

extern int logLevel(void);
extern void setLoggingLevel(int state);
extern void log_error(const char *s, ...);
extern void log_action(int actiontype, char *fname, char *target,
		       mode_t mode, unsigned long uid, unsigned long gid,
		       int type, unsigned long major, unsigned long minor,
		       int overwrite );
extern char *log_cwd(void);
extern int log_inRoot(void);

extern void log_warning(const char *s, ...);
extern char *get_single_warning(void);
extern void clear_single_warning(void);
extern void purge_warnings(void);
extern void clear_warnings(void);

extern void release_log(void);

#endif
