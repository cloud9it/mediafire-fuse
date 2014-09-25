/*
 * Copyright (C) 2013 Bryan Christ <bryan.christ@mediafire.com>
 *               2014 Johannes Schauer <j.schauer@email.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#define _XOPEN_SOURCE           // for strptime
#include <time.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../utils/http.h"
#include "../../utils/json.h"
#include "../../utils/strings.h"
#include "../folder.h"
#include "../mfconn.h"
#include "../apicalls.h"        // IWYU pragma: keep

static int      _decode_folder_get_content_folders(mfhttp * conn, void *data);

static int      _decode_folder_get_content_files(mfhttp * conn, void *data);

/*
 * the helper functions will do a realloc on the values pointed to by
 * mffolder_result and mffile_result so make sure that those are either NULL
 * or values of already malloc'ed regions. They must not be uninitialized
 * values.
 */
long
mfconn_api_folder_get_content(mfconn * conn, int mode, mffolder * folder_curr, mffolder ***mffolder_result, mffile ***mffile_result)
{
    const char     *api_call;
    int             retval;
    char           *content_type;
    mfhttp         *http;
    const char     *folderkey;

    if (conn == NULL)
        return -1;

    if (mode == 0)
        content_type = "folders";
    else
        content_type = "files";

    folderkey = folder_get_key(folder_curr);
    if (folderkey == NULL) {
        api_call = mfconn_create_signed_get(conn, 0, "folder/get_content.php",
                                            "?content_type=%s"
                                            "&response_format=json",
                                            content_type);
    } else {
        api_call = mfconn_create_signed_get(conn, 0, "folder/get_content.php",
                                            "?folder_key=%s"
                                            "&content_type=%s"
                                            "&response_format=json",
                                            folderkey, content_type);
    }

    http = http_create();
    if (mode == 0)
        retval =
            http_get_buf(http, api_call,
                         _decode_folder_get_content_folders, (void *)mffolder_result);
    else
        retval =
            http_get_buf(http, api_call,
                         _decode_folder_get_content_files, (void *)mffile_result);
    http_destroy(http);

    free((void *)api_call);

    return retval;
}

static int _decode_folder_get_content_folders(mfhttp * conn, void *user_ptr)
{
    json_error_t    error;
    json_t         *root;
    json_t         *node;
    json_t         *data;

    json_t         *folders_array;
    json_t         *folderkey;
    json_t         *folder_name;
    json_t         *j_obj;

    char           *ret;
    struct tm       tm;

    int             array_sz;
    int             i = 0;

    mffolder     ***mffolder_result;
    mffolder       *tmp_folder;
    size_t          len_mffolder_result;

    mffolder_result = (mffolder ***)user_ptr;
    if (mffolder_result == NULL)
        return -1;

    root = http_parse_buf_json(conn, 0, &error);

    /*json_t *result = json_object_by_path(root, "response/action");
       fprintf(stderr, "response/action: %s\n", (char*)json_string_value(result)); */

    node = json_object_by_path(root, "response/folder_content");

    folders_array = json_object_get(node, "folders");
    if (!json_is_array(folders_array)) {
        json_decref(root);
        return -1;
    }

    len_mffolder_result = 0;
    array_sz = json_array_size(folders_array);
    for (i = 0; i < array_sz; i++) {
        data = json_array_get(folders_array, i);

        if (json_is_object(data)) {
            folderkey = json_object_get(data, "folderkey");

            folder_name = json_object_get(data, "name");

            if (folderkey != NULL && folder_name != NULL) {
                tmp_folder = folder_alloc();

                folder_set_key(tmp_folder, json_string_value(folderkey));
                folder_set_name(tmp_folder, json_string_value(folder_name));

                j_obj = json_object_get(data, "revision");
                if (j_obj != NULL) {
                    folder_set_revision(tmp_folder, atoll(json_string_value(j_obj)));
                }

                j_obj = json_object_get(data, "parent");
                if (j_obj != NULL) {
                    folder_set_parent(tmp_folder, json_string_value(j_obj));
                }

                j_obj = json_object_get(data, "created");
                if (j_obj != NULL) {
                    memset(&tm, 0, sizeof(struct tm));
                    ret = strptime(json_string_value(j_obj), "%F %T", &tm);
                    if (ret[0] == '\0') {
                        folder_set_created(tmp_folder, mktime(&tm));
                    }
                }

                len_mffolder_result++;
                *mffolder_result =
                    (mffolder**)realloc(*mffolder_result,
                                        len_mffolder_result*sizeof(mffolder*));
                (*mffolder_result)[len_mffolder_result - 1] = tmp_folder;
            }
        }
    }

    // append an terminating empty mffolder
    len_mffolder_result++;
    *mffolder_result = (mffolder**)realloc(*mffolder_result, len_mffolder_result*sizeof(mffolder*));
    // write an empty last element
    (*mffolder_result)[len_mffolder_result-1] = NULL;

    if (root != NULL)
        json_decref(root);

    return 0;
}

static int _decode_folder_get_content_files(mfhttp * conn, void *user_ptr)
{
    json_error_t    error;
    json_t         *root;
    json_t         *node;
    json_t         *data;

    json_t         *files_array;
    json_t         *quickkey;
    json_t         *file_name;
    json_t         *j_obj;

    char           *ret;
    struct tm       tm;

    int             array_sz;
    int             i = 0;

    mffile       ***mffile_result;
    mffile         *tmp_file;
    size_t          len_mffile_result;

    mffile_result = (mffile ***)user_ptr;
    if (mffile_result == NULL)
        return -1;

    root = http_parse_buf_json(conn, 0, &error);

    node = json_object_by_path(root, "response/folder_content");

    files_array = json_object_get(node, "files");
    if (!json_is_array(files_array)) {
        json_decref(root);
        return -1;
    }

    len_mffile_result = 0;
    array_sz = json_array_size(files_array);
    for (i = 0; i < array_sz; i++) {
        data = json_array_get(files_array, i);

        if (json_is_object(data)) {
            quickkey = json_object_get(data, "quickkey");

            file_name = json_object_get(data, "filename");

            if (quickkey != NULL && file_name != NULL) {
                tmp_file = file_alloc();

                file_set_key(tmp_file, json_string_value(quickkey));
                file_set_name(tmp_file, json_string_value(file_name));

                j_obj = json_object_get(data, "size");
                if (j_obj != NULL) {
                    file_set_size(tmp_file, atoll(json_string_value(j_obj)));
                }

                j_obj = json_object_get(data, "created");
                if (j_obj != NULL ) {
                    memset(&tm, 0, sizeof(struct tm));
                    ret = strptime(json_string_value(j_obj), "%F %T", &tm);
                    if (ret[0] == '\0') {
                        file_set_created(tmp_file, mktime(&tm));
                    }
                }

                j_obj = json_object_get(data, "revision");
                if (j_obj != NULL) {
                    file_set_revision(tmp_file, atoll(json_string_value(j_obj)));
                }

                // FIXME don't save hex ascii string but binary chars instead
                j_obj = json_object_get(data, "hash");
                if (j_obj != NULL) {
                    file_set_hash(tmp_file, json_string_value(j_obj));
                }

                len_mffile_result++;
                *mffile_result =
                    (mffile**)realloc(*mffile_result,
                                      len_mffile_result*sizeof(mffile*));
                (*mffile_result)[len_mffile_result - 1] = tmp_file;
            }
        }
    }

    // append a terminating empty file
    len_mffile_result++;
    *mffile_result = (mffile**)realloc(*mffile_result, len_mffile_result*sizeof(mffile*));
    // write an empty last element
    (*mffile_result)[len_mffile_result - 1] = NULL;

    if (root != NULL)
        json_decref(root);

    return 0;
}
