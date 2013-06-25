#include "google-drive-on-fuse.h"
#include "str.h"

void o_load_clientid(struct o_val_t *o_val) {
    struct json_object *json, *installed;
    json = getjson_under_home_config("client_secrets.json");
    installed = json_object_object_get(json, "installed");
    o_val -> client_id = new_str(json_object_get_string(json_object_object_get(installed, "client_id")));
    o_val -> client_secret = new_str(json_object_get_string(json_object_object_get(installed, "client_secret")));
    o_val -> redirect_uri = new_str(json_object_get_string(json_object_array_get_idx(json_object_object_get(installed, "redirect_uris"), 0)));
}

struct o_val_t *o_load_token(struct o_val_t *o_val) {
    struct json_object *json = getjson_under_home_config("token.json");
    if(json == NULL) {
        return NULL;
    }
    o_val -> access_token = new_str(json_object_get_string(json_object_object_get(json, "access_token")));
    o_val -> refresh_token = new_str(json_object_get_string(json_object_object_get(json, "refresh_token")));
    o_val -> token_type = new_str(json_object_get_string(json_object_object_get(json, "token_type")));
    o_val -> expires_in = json_object_get_int(json_object_object_get(json, "expires_in"));
    o_val -> established_time = json_object_get_int(json_object_object_get(json, "established_time"));
    return o_val;
}

void o_establish_getcode(struct o_val_t *o_val) {
    struct http_map getmap[4];
    char code[128];
    int i;
    getmap[0].key = new_str("response_type");
    getmap[0].val = o_val -> response_type;
    getmap[1].key = new_str("redirect_uri");
    getmap[1].val = o_val -> redirect_uri;
    getmap[2].key = new_str("client_id");
    getmap[2].val = o_val -> client_id;
    getmap[3].key = new_str("scope");
    getmap[3].val = new_str(o_val -> scope[0]);
    for(i = 1; i < arrsize(o_val -> scope); ++i) {
        getmap[3].val = str_cat(getmap[3].val, " ");
        getmap[3].val = str_cat(getmap[3].val, o_val -> scope[i]);
    }
    fprintf(stdout, "Get and input the code from %s .\n", build_url("https://accounts.google.com/o/oauth2/auth", getmap, arrsize(getmap)));
    scanf("%127s", code);
    o_val -> code = new_str(code);
    time(&(o_val -> established_time));
}

void o_establish_gettoken(struct o_val_t *o_val) {
    struct json_object *response;
    struct http_map post[5];
    char *content;
    int i;
    post[0].key = new_str("code");
    post[0].val = new_str(o_val -> code);
    post[1].key = new_str("client_id");
    post[1].val = new_str(o_val -> client_id);
    post[2].key = new_str("client_secret");
    post[2].val = new_str(o_val -> client_secret);
    post[3].key = new_str("grant_type");
    post[3].val = new_str(o_val -> grant_type);
    post[4].key = new_str("redirect_uri");
    post[4].val = new_str(o_val -> redirect_uri);
    content = build_url("", post, arrsize(post));
    ++content;
    response = getjson(POST, "https://accounts.google.com/o/oauth2/token", NULL, NULL, content, 0, 0, -1);
    if(json_object_get_string(json_object_object_get(response, "error")) != NULL) {
        fprintf(stderr, "[Login][Error] %s\n", json_object_get_string(json_object_object_get(response, "error")));
    }
    o_val -> access_token = new_str(json_object_get_string(json_object_object_get(response, "access_token")));
    o_val -> expires_in = json_object_get_int(json_object_object_get(response, "expires_in"));
    o_val -> token_type = new_str(json_object_get_string(json_object_object_get(response, "token_type")));
    o_val -> refresh_token = new_str(json_object_get_string(json_object_object_get(response, "refresh_token")));
}

void o_save_token(struct o_val_t *o_val) {
    struct json_object *json = json_object_new_object();
    json_object_object_add(json, "access_token", json_object_new_string(o_val -> access_token));
    json_object_object_add(json, "refresh_token", json_object_new_string(o_val -> refresh_token));
    json_object_object_add(json, "token_type", json_object_new_string(o_val -> token_type));
    json_object_object_add(json, "established_time", json_object_new_int(o_val -> established_time));
    json_object_object_add(json, "expires_in", json_object_new_int(o_val -> expires_in));
    setjson_under_home_config("token.json", json);
}

size_t o_refresh(struct o_val_t *o_val) {
    struct json_object *response;
    struct http_map post[4];
    char *content;
    int i;
    time_t now;
    time(&now);
    if(now - (o_val -> established_time) < o_val -> expires_in - DELTA_TIME) {
        return sizeof(o_val);
    }
    fprintf(stderr, "[OAuth][Info] Refresh Token...\n");
    post[0].key = new_str("refresh_token");
    post[0].val = new_str(o_val -> refresh_token);
    post[1].key = new_str("client_id");
    post[1].val = new_str(o_val -> client_id);
    post[2].key = new_str("client_secret");
    post[2].val = o_val -> client_secret;
    post[3].key = new_str("grant_type");
    post[3].val = "refresh_token";
    content = build_url("", post, arrsize(post));
    ++content;
    time(&(o_val -> established_time));
    response = getjson(POST, "https://accounts.google.com/o/oauth2/token", NULL, NULL, content, 0, 0, -1);
    o_val -> access_token = json_object_get_string(json_object_object_get(response, "access_token"));
    o_val -> expires_in = json_object_get_int(json_object_object_get(response, "expires_in"));
    o_val -> token_type = json_object_get_string(json_object_object_get(response, "token_type"));
    o_save_token(o_val);
    return sizeof(o_val);
}

struct o_val_t o_establish(struct o_val_t *o_val) {
    o_val -> response_type = new_str("code");
    o_val -> scope = (char **)malloc(sizeof(void *));
    o_val -> scope[0] = new_str("https://www.googleapis.com/auth/drive");
    o_val -> grant_type = new_str("authorization_code");
    o_load_clientid(o_val);
    if(o_load_token(o_val) == NULL) {
        o_establish_getcode(o_val);
        o_establish_gettoken(o_val);
        o_save_token(o_val);
    }
    return *o_val;
}
