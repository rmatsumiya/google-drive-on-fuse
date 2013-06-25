#include "google-drive-on-fuse.h"
#include "str.h"

void append_child(struct filetree *parent, struct filetree *child) {
    struct filetree **newChildren;
    child -> parent = parent;
    newChildren = (struct filetree **)malloc(sizeof(struct filetree *) + (parent -> numberOfChildren) * sizeof(struct filetree *));
    if(parent -> children != NULL) {
        memcpy(newChildren, parent -> children, (parent -> numberOfChildren) * sizeof(struct filetree *));
    }
    newChildren[(parent -> numberOfChildren)++] = child;
    parent -> children = newChildren;
}

int remove_child(struct filetree *tree) {
    struct filetree *parent, **newChildren;
    int pos;
    if(tree == NULL) {
        return -ENOENT;
    }
    if(tree -> numberOfChildren > 0) {
        return -ENOTEMPTY;
    }
    parent = tree -> parent;
    if(parent == NULL) {
        return -EACCES;
    }
    if(parent -> numberOfChildren > 1) {
        for(pos = 0; pos < (parent -> numberOfChildren); ++pos) {
            if(parent -> children[pos] == tree) {
                break;
            }
        }
        newChildren = (struct filetree **)malloc(sizeof(struct filetree *) * (parent -> numberOfChildren - 1));
        if(pos > 0) {
            memcpy(newChildren, parent -> children, sizeof(struct filetree *) * pos);
        }
        if(pos < parent -> numberOfChildren - 1) {
            memcpy(newChildren + pos, parent -> children + pos + 1, sizeof(struct filetree *) * (parent -> numberOfChildren - pos - 1));
        }
    } else {
        newChildren = NULL;
    }
    free(parent -> children);
    parent -> children = newChildren;
    --(parent -> numberOfChildren);
    return 0;
}

struct filetree *filetree_init(struct o_val_t *o_val) {
    char *authorization;
    struct http_map map[1], *get;
    struct json_object *json, *items;
    struct filetree *tree, *root, *newTree;
    int items_size, i, j, topidx;
    
    char *nextPageToken = NULL;
    items_size = 0;
    i = 0;
    
    root = (struct filetree *)malloc(sizeof(struct filetree));
    root -> numberOfChildren = 0;
    root -> children = NULL;
    root -> title = "";
    root -> id = NULL;
    root -> parent = NULL;

    authorization = str_cat(o_val -> token_type, " ");
    authorization = str_cat(authorization, o_val -> access_token);
    map[0].key = "Authorization";
    map[0].val = authorization; 

    do {
        if(nextPageToken == NULL) {
            get = (struct http_map *)malloc(sizeof(struct http_map) * 2);
        } else {
            get = (struct http_map *)malloc(sizeof(struct http_map) * 3);
            get[2].key = new_str("pageToken");
            get[2].val = nextPageToken;
        }
        get[0].key = new_str("maxResults");
        get[0].val = new_str("1000");
        get[1].key = new_str("q");
        get[1].val = new_str("trashed = false");

        o_refresh(o_val);
        json = getjson(GET, "https://www.googleapis.com/drive/v2/files", get, map, "", nextPageToken==NULL?2:3, arrsize(map), 0);
        items = json_object_object_get(json, "items");
        items_size += json_object_array_length(items);
        free(get);

        newTree = (struct filetree *)malloc(sizeof(struct filetree) * items_size);
        memcpy(newTree, tree, i * sizeof(struct filetree));
        if(i != 0) {
            free(tree);
        }
        tree = newTree;

        nextPageToken = NULL;
        if(json_object_object_get(json, "nextPageToken") != NULL) {
            nextPageToken = new_str(json_object_get_string(json_object_object_get(json, "nextPageToken")));
        }
        for(j = 0; i < items_size; ++i, ++j) {
            struct json_object *current_json = json_object_array_get_idx(items, j);
            tree[i].id = new_str(json_object_get_string(json_object_object_get(current_json, "id")));
            tree[i].title = new_str(json_object_get_string(json_object_object_get(current_json, "title")));
            tree[i].mimeType = new_str(json_object_get_string(json_object_object_get(current_json, "mimeType")));
            tree[i].fileSize = json_object_get_int(json_object_object_get(current_json, "fileSize"));
            tree[i].parent = NULL;
            tree[i].parent_id = NULL;
            tree[i].children = NULL;
            tree[i].numberOfChildren = 0;
            tree[i].content.len = 0;
            tree[i].content.buf = NULL;
            tree[i].semaphore = 0;
            if(json_object_get_string(json_object_object_get(current_json, "downloadUrl")) != NULL) {
                tree[i].downloadUrl = new_str(json_object_get_string(json_object_object_get(current_json, "downloadUrl")));
            } else {
                tree[i].downloadUrl = NULL;
            }
            if(json_object_object_get(current_json, "parents") != NULL) {
                int isRoot = json_object_get_boolean(json_object_object_get(json_object_array_get_idx(json_object_object_get(current_json, "parents"), 0), "isRoot"));
                if(isRoot) {
                    root -> id = new_str(json_object_get_string(json_object_object_get(json_object_array_get_idx(json_object_object_get(current_json, "parents"), 0), "id")));
                }
                if(json_object_object_get(json_object_array_get_idx(json_object_object_get(current_json, "parents"), 0), "id") != NULL) {
                    tree[i].parent_id = new_str(json_object_get_string(json_object_object_get(json_object_array_get_idx(json_object_object_get(current_json, "parents"), 0), "id")));
                }
            }
        }
    } while(nextPageToken != NULL);

    for(i = 0; i < items_size; ++i) {
        int j;
        struct json_object *current_json = json_object_array_get_idx(items, i);
        if(tree[i].parent_id == NULL) {
            continue;
        }
        if(strcmp(tree[i].parent_id, root -> id) == 0) {
            append_child(root, tree + i);
            continue;
        }
        for(j = 0; j < items_size; ++j) {
            char *current_id;
            if(strcmp(tree[j].id, tree[i].parent_id) == 0) {
                append_child(tree + j, tree + i);
                break;
            }
        }
    }

    return root;
}

struct filetree *filetree_get(const char *path, const struct filetree const *root) {
    int start;
    char *filename = str_split(path, '/', &start);
    int i;
    if(strcmp(filename, "") == 0) {
        return root;
    }
    for(i = 0; i < root -> numberOfChildren; ++i) {
        if(strcmp(root -> children[i] -> title, filename) == 0 && root -> children[i] -> parent == root) {
            if(root->children[i]->mimeType!=NULL?strcmp(root->children[i]->mimeType, "application/vnd.google-apps.folder")==0:0) {
                return filetree_get(path + start + strlen(filename), root -> children[i]);
            } else if(strlen(root -> children[i] -> title) == strlen(path + start)) {
                return root -> children[i];
            } else {
                return NULL;
            }
        }
    }
    return NULL;
}

struct filetree *new_filetree(const char *path, const struct filetree *root, const char *mimeType, struct o_val_t *o_val) {
    struct filetree *newTree;
    struct filetree *parentTree;
    char *parentname, *filename, *authorization, *sentJSON_buf, sentJSON_buf_len[100];
    struct http_map map[3];
    int pos;
    struct json_object *recvJSON, *ids;
    struct json_object *sentJSON = json_object_new_object();
    struct json_object *parents = json_object_new_array();
    parentname = str_split_rev(path, '/', &pos);
    parentTree = filetree_get(parentname, root);
    if(parentTree == NULL) {
        return NULL;
    }
    filename = new_str(path + pos);
    newTree = (struct filetree *)malloc(sizeof(struct filetree));
    newTree -> title = filename;
    newTree -> fileSize = 0;
    newTree -> semaphore = 0;
    newTree -> parent = NULL;
    newTree -> children = NULL;
    newTree -> numberOfChildren = 0;
    newTree -> parent_id = parentTree -> id;
    newTree -> content.buf = NULL;
    newTree -> content.len = 0;
    newTree -> isChanged = 0;
    if(mimeType != NULL) {
        newTree -> mimeType = new_str(mimeType);
        json_object_object_add(sentJSON, "mimeType", json_object_new_string(newTree -> mimeType));
    }
    append_child(parentTree, newTree);
    json_object_object_add(sentJSON, "title", json_object_new_string(newTree -> title));
    ids = json_object_new_object();
    parents = json_object_new_array();
    json_object_object_add(ids, "id", json_object_new_string(newTree -> parent_id));
    json_object_array_add(parents, ids);
    json_object_object_add(sentJSON, "parents", parents);
    sentJSON_buf = new_str(json_object_to_json_string(sentJSON));
    authorization = str_cat(o_val -> token_type, " ");
    authorization = str_cat(authorization, o_val -> access_token);
    map[0].key = new_str("Authorization");
    map[0].val = authorization; 
    map[1].key = new_str("Content-Type");
    map[1].val = new_str("application/json");
    map[2].key = new_str("Content-Length");
    sprintf(sentJSON_buf_len, "%d", strlen(sentJSON_buf));
    map[2].val = sentJSON_buf_len;
    o_refresh(o_val);
    recvJSON = getjson(POST, "https://www.googleapis.com/drive/v2/files", NULL, map, sentJSON_buf, 0, 2, -1);
    newTree -> id = new_str(json_object_get_string(json_object_object_get(recvJSON, "id")));
    if(json_object_get_string(json_object_object_get(recvJSON, "downloadUrl")) != NULL) {
        newTree -> downloadUrl = new_str(json_object_get_string(json_object_object_get(recvJSON, "downloadUrl")));
    }
    newTree -> mimeType = new_str(json_object_get_string(json_object_object_get(recvJSON, "mimeType")));
    free(sentJSON_buf);
    return newTree;
}
