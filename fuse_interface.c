#include "google-drive-on-fuse.h"
#include "str.h"

int fuse_gdrive_getattr(const char *path, struct stat *stbuf) {
    struct fuse_context *fc = fuse_get_context();
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    const char dirMimeType[] = "application/vnd.google-apps.folder";
    if(strcmp(path, "/") == 0) {
        stbuf -> st_mode = S_IFDIR | 0755;
        stbuf -> st_nlink = 2;
    } else {
        struct filetree *currentTree;
        memset(stbuf, 0, sizeof(struct stat));
        currentTree = filetree_get(path, tree);
        if(currentTree == NULL) {
            return -ENOENT;
        } if(currentTree->mimeType!=NULL?strcmp(currentTree -> mimeType, dirMimeType)==0:0) {
            stbuf -> st_mode = S_IFDIR | 0755;
            stbuf -> st_nlink = 2;
        } else {
            stbuf -> st_size = currentTree -> fileSize;
            stbuf -> st_mode = S_IFREG | 0664;
            stbuf -> st_nlink = 1;
        }
    }
    stbuf -> st_uid = fc -> uid;
    stbuf -> st_gid = fc -> gid;
    return 0;
}

int fuse_gdrive_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileinfo) {
    int i;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    for(i = 0; i < currentTree -> numberOfChildren; ++i) {
        filler(buf, currentTree -> children[i] -> title, NULL, 0);
    }
    return 0;
}

int fuse_gdrive_mkdir(const char *path, mode_t mode) {
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct o_val_t *o_val = ((struct gdrive_data *)fuse_get_context() -> private_data) -> o_val;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree != NULL) {
        return -EEXIST;
    }
    currentTree = new_filetree(path, tree, "application/vnd.google-apps.folder", o_val);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    return 0;
}

int fuse_gdrive_create(const char *path, mode_t mode, struct fuse_file_info *file_info) {
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct o_val_t *o_val = ((struct gdrive_data *)fuse_get_context() -> private_data) -> o_val;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree != NULL) {
        return -EEXIST;
    }
    currentTree = new_filetree(path, tree, NULL, o_val);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    return 0;
}

int fuse_gdrive_open(const char *path, struct fuse_file_info *file_info) {
    int i;
    struct http_map map[1];
    char *authorization;
    const char blockMimeType[][100] = {"application/vnd.google-apps.document",
                                    "application/vnd.google-apps.drawing",
                                    "application/vnd.google-apps.form",
                                    "application/vnd.google-apps.fusiontable",
                                    "application/vnd.google-apps.kix",
                                    "application/vnd.google-apps.presentation",
                                    "application/vnd.google-apps.script",
                                    "application/vnd.google-apps.sites",
                                    "application/vnd.google-apps.spreadsheet"};
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct o_val_t *o_val = ((struct gdrive_data *)fuse_get_context() -> private_data) -> o_val;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL && (file_info -> flags & 3) == O_RDONLY) {
        return -ENOENT;
    }
    if(currentTree -> mimeType != NULL) {
        for(i = 0; i < arrsize(blockMimeType); ++i) {
            if(strcmp(currentTree -> mimeType, blockMimeType[i]) == 0) {
                return -EACCES;
            }
        }
    }
    currentTree -> isChanged = 0;
    if((currentTree -> semaphore)++ == 0) {
        authorization = str_cat(o_val -> token_type, " ");
        authorization = str_cat(authorization, o_val -> access_token);
        map[0].key = new_str("Authorization");
        map[0].val = authorization; 
        fprintf(stderr, "[Download][Info] Donwloading : %s\n", currentTree -> downloadUrl);
        o_refresh(o_val);
        currentTree -> content = http_get_data(GET, currentTree -> downloadUrl, NULL, map, NULL, 0, 1, 0).body;
    }
    return 0;
}

int fuse_gdrive_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *file_info) {
    int res;
    struct http_buf_data content;
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    content = currentTree -> content;
    if(content.len < offset) {
        return 0;
    }
    res = offset+size<=content.len?size:content.len-offset;
    memcpy(buf, content.buf + offset, res);
    return res;
}

int fuse_gdrive_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info) {
    struct http_buf_data tmp;
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    tmp.len = offset+size>(currentTree -> content.len)?offset+size:currentTree->content.len;
    tmp.buf = malloc(tmp.len + 1);
    memset(tmp.buf, 0, tmp.len + 1);
    memcpy(tmp.buf, currentTree -> content.buf, currentTree -> content.len);
    memcpy(tmp.buf + offset, buf, size);
    if(currentTree -> content.buf != NULL) {
        free(currentTree -> content.buf);
    }
    currentTree -> content.buf = tmp.buf;
    currentTree -> content.len = tmp.len;
    currentTree -> isChanged = 1;
    return size;
}

int fuse_gdrive_release(const char *path, struct fuse_file_info *file_info) {
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    if(((file_info -> flags & 3) == O_WRONLY) || ((file_info -> flags & 3) == O_RDWR)) {
        if(currentTree -> isChanged != 0) {
            struct http_map map[4];
            struct http_map getmap[2];
            char *authorization, *uploadUrl;
            struct http_recv_data recvdata;
            struct o_val_t *o_val = ((struct gdrive_data *)fuse_get_context() -> private_data) -> o_val;
            currentTree -> fileSize = currentTree -> content.len;
            authorization = str_cat(o_val -> token_type, " ");
            authorization = str_cat(authorization, o_val -> access_token);
            map[0].key = new_str("Authorization");
            map[0].val = authorization; 
            map[1].key = new_str("X-Upload-Content-Type");
            map[1].val = getMimeType(currentTree -> content.buf, currentTree -> content.len);
            map[2].key = new_str("X-Upload-Content-Length");
            map[2].val = (char *)malloc(100);
            sprintf(map[2].val, "%d", currentTree -> content.len);
            map[3].key = new_str("Content-Length");
            map[3].val = new_str("0");
            getmap[0].key = new_str("uploadType");
            getmap[0].val = new_str("resumable");
            o_refresh(o_val);
            recvdata = http_get_data(PUT, str_cat("https://www.googleapis.com/upload/drive/v2/files/", currentTree -> id), getmap, map, "", 1, 4, 0);
            fprintf(stderr, "[Upload][Info] Uploading...\n");
            if(strncmp(http_get_header_val(recvdata.header, NULL), "400", 3) == 0) {
                getmap[1].key = new_str("convert");
                getmap[1].val = new_str("true");
                recvdata = http_get_data(PUT, str_cat("https://www.googleapis.com/upload/drive/v2/files/", currentTree -> id), getmap, map, "", 2, 4, 0);
            }
            if(http_get_header_val(recvdata.header, "Location") == NULL) {
                fprintf(stderr, "[Upload][Error] Error: Location NULL.\n");
            }
            uploadUrl = http_get_header_val(recvdata.header, "Location");
            map[1].key = new_str("Content-Type");
            map[2].key = new_str("Content-Length");
            recvdata = http_get_data(PUT, uploadUrl, NULL, map, currentTree -> content.buf, 0, 3, currentTree -> content.len);
            fprintf(stderr, "[Upload][Info] Status: %s\n", http_get_header_val(recvdata.header, NULL));
            char *fsize = map[2].val;
            while(strncmp(http_get_header_val(recvdata.header, NULL), "503", 3) == 0) {
                char range[100] = "";
                char bytes[512] = "";
                map[1].key = new_str("Content-Range");
                map[1].val = str_cat("bytes */", fsize);
                map[2].val = "0";
                recvdata = http_get_data(PUT, uploadUrl, NULL, map, NULL, 0, 3, 0);
                sscanf(http_get_header_val(recvdata.header, "Range"), "0-%s", range);
                sprintf(map[2].val, "%d", atoi(fsize) - atoi(range));
                sprintf(bytes, "bytes %d-%d/%s", atoi(range) + 1, atoi(fsize) - 1, fsize);
                map[1].val = new_str(bytes);
                recvdata = http_get_data(PUT, uploadUrl, NULL, map, currentTree -> content.buf + atoi(range) + 1, 0, 3, currentTree -> content.len - atoi(range));
            }
        }
    }
    if(--(currentTree -> semaphore) == 0) {
        free(currentTree -> content.buf);
    }
    return 0;
}

int fuse_gdrive_unlink(const char *path) {
    int res;
    char *authorization;
    struct http_map map[1];
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct o_val_t *o_val = ((struct gdrive_data *)fuse_get_context() -> private_data) -> o_val;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    res = remove_child(currentTree);
    if(res != 0) {
        return res;
    }
    authorization = str_cat(o_val -> token_type, " ");
    authorization = str_cat(authorization, o_val -> access_token);
    map[0].key = new_str("Authorization");
    map[0].val = authorization; 
    http_get_data(DELETE, str_cat("https://www.googleapis.com/drive/v2/files/", currentTree -> id), NULL, map, NULL, 0, 1, 0);
    return 0;
}

int fuse_gdrive_chmod(const char *path, mode_t mode) {
    return -ENOTSUP;
}

int fuse_gdrive_chown(const char *path, uid_t uid, gid_t gid) {
    return -ENOTSUP;
}

int fuse_gdrive_truncate(const char *path, off_t size) {
    struct filetree *tree = ((struct gdrive_data *)fuse_get_context() -> private_data) -> tree;
    struct filetree *currentTree = filetree_get(path, tree);
    if(currentTree == NULL) {
        return -ENOENT;
    }
    if(currentTree -> fileSize < size) {
        char *tmp;
        tmp = (char *)malloc(size + 1);
        memset(tmp, 0, size + 1);
        memcpy(tmp, currentTree -> content.buf, currentTree -> content.len);
        if(currentTree -> content.buf != NULL) {
            free(currentTree -> content.buf);
        }
        currentTree -> content.buf = tmp;
        currentTree -> content.len = size;
        currentTree -> isChanged = 1;
    } else if(currentTree -> fileSize > size) {
        free(currentTree -> content.buf);
        currentTree -> content.buf = malloc(size + 1);
        memset(currentTree -> content.buf, 0, size + 1);
        currentTree -> content.len = size;
        currentTree -> isChanged = 1;
    }
    return 0;
}

int fuse_gdrive_utimens(const char *path, const struct timespec tv[2]) {
    return 0;
}

struct fuse_operations fuse_gdrive_operate = {
    .getattr = fuse_gdrive_getattr,
    .readdir = fuse_gdrive_readdir,
    .open = fuse_gdrive_open,
    .read = fuse_gdrive_read,
    .release = fuse_gdrive_release,
    .create = fuse_gdrive_create,
    .mkdir = fuse_gdrive_mkdir,
    .write = fuse_gdrive_write,
    .unlink = fuse_gdrive_unlink,
    .rmdir = fuse_gdrive_unlink,
    .chmod = fuse_gdrive_chmod,
    .chown = fuse_gdrive_chown,
    .truncate = fuse_gdrive_truncate,
    .utimens = fuse_gdrive_utimens
};

int main(int argc, char *argv[]) {
    int ret;
    struct o_val_t *o_val;
    struct filetree *tree;
    struct gdrive_data *pdata = (struct gdrive_data *)malloc(sizeof(struct gdrive_data));
    o_val = (struct o_val_t *) malloc(sizeof(struct o_val_t));
    o_establish(o_val);
    tree = filetree_init(o_val);
    pdata -> tree = tree;
    pdata -> o_val = o_val;
    ret = fuse_main(argc, argv, &fuse_gdrive_operate, pdata);
    free(pdata -> o_val);
    free(pdata -> tree);
    return ret;
}
