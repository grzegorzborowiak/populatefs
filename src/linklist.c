#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linklist.h"

typedef struct linklist_node {
    dev_t dev_id;
    ino_t inode_num;
    char                    *name;
    struct linklist_node    *next;
} linklistT;

static linklistT *linklist = NULL;

char *linklist_add(dev_t dev_id, ino_t inode_num, char *name)
{
    linklistT *node;

    if ( linklist != NULL ) {
        node = linklist;
        while ( node->next != NULL )
            if (( node->inode_num == inode_num ) && ( node->dev_id == dev_id ))
                return node->name;
            else node = node->next;

        node->next = (linklistT*)malloc(sizeof(linklistT));
        node = node->next;
    } else {
        linklist = (linklistT*)malloc(sizeof(linklistT));
        node = linklist;
    }

    node->dev_id = dev_id;
    node->inode_num = inode_num;
    node->name = strdup(name);
    node->next = NULL;

    return NULL;
}

void linklist_release(void)
{
    while ( linklist != NULL ) {
        linklistT *node = linklist;
        linklist = node->next;
        node->next = NULL;
        free( node->name);
        node->name = NULL;
        free( node );
        node = NULL;
    }
}
