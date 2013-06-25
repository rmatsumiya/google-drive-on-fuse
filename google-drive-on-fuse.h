#include <fuse.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json.h>
#include <magic.h>

#define configdirpath "/.config/fuse-google-drive/"
#define arrsize(arr) (sizeof(arr)/sizeof(arr[0]))
#define DELTA_TIME 5

enum http_mode {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
} HTTPMODE;

struct o_val_t {
    char *response_type;
    char *client_id;
    char *client_secret;
    char *redirect_uri;
    char **scope;
    char *code;
    char *grant_type;
    char *access_token;
    char *refresh_token;
    int expires_in;
    char *token_type;
    time_t established_time;
};

struct http_buf_data {
    size_t len;
    char *buf;
};

struct filetree {
    char *id;
    char *title;
    char *mimeType;
    char *downloadUrl;
    char *parent_id;
    int fileSize;
    int numberOfChildren;
    int semaphore;
    int isChanged;
    struct filetree *parent;
    struct filetree **children;
    struct http_buf_data content;
};

struct http_map {
    char *key;
    char *val;
};

struct gdrive_data {
    struct filetree *tree;
    struct o_val_t *o_val;
};

struct http_recv_data {
    struct http_buf_data header, body;
};

const struct json_object *getjson(
    const enum http_mode mode,
    const char *url,
    const struct http_map *http_get,
    const struct http_map *http_header,
    const char *http_content,
    const size_t http_get_size,
    const size_t http_header_size,
    const size_t http_content_size
);

struct http_recv_data http_get_data(
    const enum http_mode mode,
    const char *url,
    const struct http_map *http_get,
    const struct http_map *http_header,
    const char *http_content,
    const size_t http_get_size,
    const size_t http_header_size,
    const size_t http_content_size
);

char *http_get_header_val(const struct http_buf_data raw, const char *key);

int fuse_gdrive_getattr(const char *path, struct stat *status);
int fuse_gdrive_readdir(const char *path, void *buf, fuse_fill_dir_t filter, off_t offset, struct fuse_file_info *file_info);
int fuse_gdrive_open(const char *path, struct fuse_file_info *file_info);
int fuse_gdrive_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

size_t o_refresh(struct o_val_t *o_val);
struct o_val_t o_establish(struct o_val_t *o_val);

void append_child(struct filetree *parent, struct filetree *child);
int remove_child(struct filetree *tree);
struct filetree *filetree_init(struct o_val_t *o_val);
struct filetree *filetree_get(const char *path, const struct filetree const *tree);
struct filetree *new_filetree(const char *path, const struct filetree *root, const char *mimeType, struct o_val_t *o_val);
char *build_url(const char *url, const struct http_map *map, size_t mapsize);
char *getMimeType(const char *data, size_t len);

const struct json_object *getjson_local(const char *path);
const struct json_object *getjson_under_home_config(const char *filename);
void setjson_under_home_config(const char *filename, struct json_object *json);
