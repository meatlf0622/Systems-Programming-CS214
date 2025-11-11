#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "uthash.h"

typedef struct PathNode {
    char *path;
    struct PathNode *next;
} PathNode;

typedef struct InodeGroup {
    ino_t inode;
    int ref_count;
    PathNode *paths;
    UT_hash_handle hh;
} InodeGroup;

typedef struct FileGroup {
    char md5[33]; // 32 + null terminator
    InodeGroup *hard_links;
    UT_hash_handle hh;
} FileGroup;

char *compute_md5(const char *path);
static int render_file_info(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);
void print_output();
