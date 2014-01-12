#ifndef __linklist__h__
#define __linklist__h__
#include <sys/types.h>

extern char *linklist_add(dev_t dev_id, ino_t inode_num, char *name);
extern void linklist_release(void);

#endif
