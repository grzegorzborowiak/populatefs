#ifndef __mod_path__h__
#define __mod_path__h__

#define MAX_PATHSIZE 2048

extern void modPath_set_pathLen(int pathLen);
extern void addPath(const char *path, int squash_uids, int squash_perms);

#endif
