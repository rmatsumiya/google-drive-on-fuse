#include "google-drive-on-fuse.h"
#include "str.h"

size_t http_buffering(char *buf, size_t size, size_t nmemb, void *userdata) {
    int i;
    char *tmp, *old_buf;
    struct http_buf_data *dst = (struct http_buf_data *)userdata;
    old_buf = dst -> buf;
    tmp = malloc(dst -> len + size * nmemb + 1);
    memset(tmp, 0, dst -> len + size * nmemb + 1);
    memcpy(tmp, dst -> buf, dst -> len);
    memcpy(tmp + (dst -> len), buf, size * nmemb);
    dst -> buf = tmp;
    dst -> len += size * nmemb;
    userdata = (void *)dst;
    free(old_buf);
    return size * nmemb;
}

char *http_get_header_val(const struct http_buf_data raw, const char *key) {
    int current;
    char statusCode[5];
    char statusMsg[20];
    struct http_map *res, *tmp;
    sscanf(raw.buf, "HTTP/1.1 %s %s\r\n", statusCode, statusMsg);
    current = strlen(statusCode) + strlen(statusMsg) + 12;
    if(key == NULL) {
        return new_str(statusCode);
    }
    while(current <= raw.len) {
        char tmp_key[256];
        char tmp_val[256];
        sscanf(raw.buf + current, "%s", tmp_key);
        current += strlen(tmp_key) + 1;
        sscanf(raw.buf + current, "%[^\r]", tmp_val);
        if(strcmp(tmp_key, str_cat(key, ":")) == 0) {
            return new_str(tmp_val);
        }
        current += strlen(tmp_val) + 2;
    }
    return NULL;
}

struct http_recv_data http_get_data(
    const enum http_mode mode,
    const char *url,
    const struct http_map *http_get,
    const struct http_map *http_header,
    const char *http_content,
    const size_t http_get_size,
    const size_t http_header_size,
    const size_t http_content_size
) {
    int i;
    int firstread = 1;
    CURLcode res;
    char *head;
    char *tmp;
    struct curl_slist *header = NULL;
    CURL *curl = curl_easy_init();
    struct http_recv_data data;
    data.body.len = 0;
    data.body.buf = new_str("");
    data.header.len = 0;
    data.header.buf = new_str("");
    curl_easy_setopt(curl, CURL_MAX_WRITE_SIZE, 100 * 1024 * 1024);
    curl_easy_setopt(curl, CURLOPT_URL, build_url(url, http_get, http_get_size));
    if(http_header != NULL) {
        for(i = 0; i < http_header_size; ++i) {
            head = str_cat(http_header[i].key, ": ");
            head = str_cat(head, http_header[i].val);
            header = curl_slist_append(header, head);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_buffering);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &(data.body));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_buffering);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &(data.header));
    switch(mode) {
        case PUT:
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, http_content_size);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, http_content);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            break;
        case PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            break;
        case DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case POST:
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, http_content_size);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, http_content);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            break;
    }
    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        fprintf(stderr, "[CURL][ERROR] %s\n", curl_easy_strerror(res));
    }
    if(header != NULL) {
        curl_slist_free_all(header);
    }
    curl_easy_cleanup(curl);
    return data;
}

char *build_url(const char *url, const struct http_map map[], size_t mapsize) {
    char *get_url;
    char *ret;
    int i;
    if(map != NULL) {
        get_url = new_str("?");
        for(i = 0; i < mapsize; ++i) {
            get_url = str_cat(get_url, map[i].key); 
            get_url = str_cat(get_url, "=");
            get_url = str_cat(get_url, g_uri_escape_string(map[i].val, NULL, TRUE));
            if(i + 1 != mapsize) {
                get_url = str_cat(get_url, "&");
            }
        }
        ret = str_cat(url, get_url);
    } else {
        ret = new_str(url);
    }
    return ret;
}

char *getMimeType(const char *data, size_t len) {
    magic_t m;
    char *res;
    m = magic_open(MAGIC_MIME);
    magic_load(m, NULL);
    res = new_str(magic_buffer(m, data, len));
    magic_close(m);
    return res;
}
