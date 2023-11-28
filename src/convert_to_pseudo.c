#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"

char * root = NULL;
char * path_in = NULL;
char * path_out = NULL;

int main(int argc, char *argv[])
{
    if ( argc >= 2 ) {
        root = argv[1];
    }
    if ( argc >= 3 ) {
        path_in = argv[2];
    }
    if ( argc >= 4 ) {
        path_out = argv[3];
    }
    
    if ( NULL == root ) {
        fprintf(stderr, "root directory must be specified as first argument\n");
        exit(1);
    }

    if ( path_in ) {
        if ( NULL == freopen(path_in, "r", stdin ) ) {
            fprintf(stderr, "\"%s\": %m\n", path_in);
            exit(1);
        }
    }
    if ( path_out ) {
        if ( NULL == freopen(path_out, "w", stdout ) ) {
            fprintf(stderr, "\"%s\": %m\n", path_out);
            exit(1);
        }
    }

    int rootdirfd = open(root, O_RDONLY);

    if ( -1 == rootdirfd ) {
        fprintf(stderr, "\"%s\": %m\n", root);
        exit(1);
    }

    size_t llen = 0;
    char * line = NULL;

    size_t plen = 1000;
    char * path = (char*)malloc(plen);

    while ( getline(&line, &llen, stdin) >= 0 ) {
        
        if ( plen < llen ) {
            plen = llen;
            path = (char*)realloc(path, plen);
        }

        char * type = nextToken(line, path);

        while ( *type && isspace(*type) ) {
            ++type;
        }
        if ( *type ) {
            char * rpath = path;
            while ( '/' == *rpath ) {
                ++rpath;
            }
            if ( 0 == faccessat(rootdirfd, rpath, F_OK, AT_SYMLINK_NOFOLLOW) ) {
                *type = 'm';
            }
        }

        printf("%s", line);
    }

    if ( line ) {
        free(line);
        line = NULL;
    }

    if ( path ) {
        free(path);
        path = NULL;
    }

    close(rootdirfd);

    return 0;
}
