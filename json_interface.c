#include "str.h"
#include "google-drive-on-fuse.h"

const struct json_object *getjson(
    const enum http_mode mode,
    const char *url,
    const struct http_map *http_get,
    const struct http_map *http_header,
    const char *http_content,
    const size_t http_get_size,
    const size_t http_header_size,
    const size_t http_content_size
) {
    struct http_recv_data data;
    struct json_object *json;
    data = http_get_data(mode, url, http_get, http_header, http_content, http_get_size, http_header_size, http_content_size);
    if(strncmp(http_get_header_val(data.header, NULL), "4", 1) == 0 || strncmp(http_get_header_val(data.header, NULL), "5", 1) == 0) {
        fprintf(stderr, "[JSON][Error] Detected getjson() Error!\n");
        fprintf(stderr, data.body.buf);
    }
    json = json_tokener_parse(data.body.buf);
    free(data.body.buf);
    return json;
}

const struct json_object *getjson_local(const char *path) {
    char *json = NULL;
    char buf[1024] = "";
    FILE *fp = fopen(path, "r");
    if(fp == NULL) {
        return NULL;
    }
    while(feof(fp) == 0) {
        fread(buf, sizeof(buf) - 1, 1, fp);
        json = json==NULL?new_str(buf):str_cat(json, buf);
    }
    fclose(fp);
    return json_tokener_parse(json);
}

const struct json_object *getjson_under_home_config(const char *filename) {
    char *path;
    const char *homedir = getenv("HOME");
    path = str_cat(homedir, configdirpath);
    path = str_cat(path, filename);
    return getjson_local(path);
}

void setjson_local(const char *path, struct json_object *json) {
    FILE *fp = fopen(path, "w");
    fprintf(fp, json_object_to_json_string(json));
    fclose(fp);
}

void setjson_under_home_config(const char *filename, struct json_object *json) {
    char *path;
    const char *homedir = getenv("HOME");
    path = str_cat(homedir, configdirpath);
    path = str_cat(path, filename);
    setjson_local(path, json);
}
