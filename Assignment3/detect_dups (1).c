#include "detect_dups.h"

FileGroup *file_map = NULL;

char *compute_md5(const char *path) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_md5();
    FILE *file = fopen(path, "rb");

    if (!file) return NULL;

    EVP_DigestInit_ex(ctx, md, NULL);

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
        EVP_DigestUpdate(ctx, buffer, bytes);

    EVP_DigestFinal_ex(ctx, hash, &length);
    EVP_MD_CTX_free(ctx);
    fclose(file);

    char *md5_string = malloc(33);
    for (int i = 0; i < 16; i++)
        sprintf(&md5_string[i * 2], "%02x", hash[i]);

    return md5_string;
}

void add_path(InodeGroup *ig, const char *path) {
    PathNode *pn = malloc(sizeof(PathNode));
    pn->path = strdup(path);
    pn->next = ig->paths;
    ig->paths = pn;
}

static int render_file_info(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag != FTW_F && tflag != FTW_SL) return 0;

    char *hash = compute_md5(fpath);
    if (!hash) return 0;

    FileGroup *fg;
    HASH_FIND_STR(file_map, hash, fg);

    if (!fg) {
        fg = malloc(sizeof(FileGroup));
        strcpy(fg->md5, hash);
        fg->hard_links = NULL;
        HASH_ADD_STR(file_map, md5, fg);
    }

    InodeGroup *ig;
    HASH_FIND_INT(fg->hard_links, &sb->st_ino, ig);

    if (!ig) {
        ig = malloc(sizeof(InodeGroup));
        ig->inode = sb->st_ino;
        ig->ref_count = sb->st_nlink;
        ig->paths = NULL;
        HASH_ADD_INT(fg->hard_links, inode, ig);
    }

    add_path(ig, fpath);
    free(hash);
    return 0;
}

void print_output() {
    int file_num = 1;
    FileGroup *fg, *tmp_fg;

    HASH_ITER(hh, file_map, fg, tmp_fg) {
        printf("File %d:\n", file_num++);
        printf("\tMD5 Hash: %s\n", fg->md5);

        InodeGroup *ig, *tmp_ig;
        HASH_ITER(hh, fg->hard_links, ig, tmp_ig) {
            printf("\t\tHard Link (%d): %lu\n", ig->ref_count, (unsigned long)ig->inode);
            printf("\t\t\tPaths:\n");

            PathNode *pn = ig->paths;
            while (pn) {
                printf("\t\t\t\t%s\n", pn->path);
                pn = pn->next;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./detect_dups <directory>\n");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (stat(argv[1], &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "Error %d: %s is not a valid directory\n", errno, argv[1]);
        exit(EXIT_FAILURE);
    }

    nftw(argv[1], render_file_info, 20, FTW_PHYS);
    print_output();
    return 0;
}
