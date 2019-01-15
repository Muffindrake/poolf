#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <jansson.h>

#define POOLF_USERAGENT \
"poolf/0 (by Muffindrake on e621) @ https://github.com/Muffindrake/poolf"

#define POOLF_USAGE \
"usage: poolf [pool id]\n"

enum poolf_err {
        POOLF_ERR_NONE,
        POOLF_ERR_MEM,
        POOLF_ERR_TIMEOUT,
        POOLF_ERR_PARSE,
        POOLF_ERR_INVALID,
        POOLF_ERR_URLMALF,
        POOLF_ERR_BADARG,
        POOLF_ERR_BADREQ,
        POOLF_ERR_ARG_EMPTY
};

struct curl_cb_data {
        char *string;
        size_t len;
};

const char *poolf_error_string[] = {
[POOLF_ERR_NONE]        = "seeing this is an error!",
[POOLF_ERR_MEM]         = "out of memory",
[POOLF_ERR_TIMEOUT]     = "connection timeout",
[POOLF_ERR_PARSE]       = "json could not be parsed",
[POOLF_ERR_INVALID]     = "json entries are invalid",
[POOLF_ERR_URLMALF]     = "malformed URL returned by API for direct file link",
[POOLF_ERR_BADARG]      = "bad command line argument",
[POOLF_ERR_BADREQ]      = "GET request could not be performed",
[POOLF_ERR_ARG_EMPTY]   = "command line argument empty"
};

/* printf to allocated space */
/* returned pointer must be freed when no longer used */
/* returns 0 on allocation failure or failure in vsnprintf */
char *
memfmt(const char fmt[static 1], ...)
{
        va_list ap;
        char *ret;
        int len;

        va_start(ap, fmt);
        len = vsnprintf(0, 0, fmt, ap);
        va_end(ap);
        if (len < 0)
                return 0;
        if (len != INT_MAX)
                len++;
        ret = malloc(len);
        if (!ret)
                return 0;
        va_start(ap, fmt);
        len = vsnprintf(ret, len, fmt, ap);
        va_end(ap);
        if (len < 0) {
                free(ret);
                return 0;
        }
        return ret;
}

size_t
poolf_curl_callback_mem(void *data_p, size_t sz, size_t n, void *user_p)
{
        struct curl_cb_data *data = user_p;
        size_t rsz = sz * n;

        data->string = realloc(data->string, data->len + rsz + 1);
        if (!data->string)
                return 0;
        memcpy(data->string + data->len, data_p, rsz);
        data->len += rsz;
        data->string[data->len] = 0;
        return rsz;
}

/* for a given url, returns allocated space containing the entire HTTP GET
 * response */
/* returns 0 on allocation failure or failure during request */
char *
poolf_request_string_sync(const char url[static 1])
{
        struct curl_cb_data buf_ret;
        CURL *crl;
        CURLcode err;

        buf_ret = (struct curl_cb_data) {0};
        crl = curl_easy_init();
        if (!crl) {
                fputs("failure initializing curl_easy\n", stderr);
                return 0;
        }
        curl_easy_setopt(crl, CURLOPT_URL, url);
        curl_easy_setopt(crl, CURLOPT_USERAGENT, POOLF_USERAGENT);
        curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, poolf_curl_callback_mem);
        curl_easy_setopt(crl, CURLOPT_WRITEDATA, &buf_ret);
        err = curl_easy_perform(crl);
        curl_easy_cleanup(crl);
        if (err != CURLE_OK) {
                fprintf(stderr, "failure performing request: %s\n",
                                curl_easy_strerror(err));
                free(buf_ret.string);
                return 0;
        }
        return buf_ret.string;
}

/* returns allocated space containing a valid URL for fetching e621 pool data */
/* returns 0 on allocation failure */
char *
poolf_url(uint_least64_t pool, uint_least64_t page)
{
        return memfmt("https://e621.net/pool/show.json"
                      "?id=%"PRIuLEAST64
                      "&page=%"PRIuLEAST64,
                        pool, page);
}

enum poolf_err
poolf_url_validate(const char url[static 1])
{
        CURLU *h;
        CURLUcode err;

        if (strstr(url, "https://") != url)
                return POOLF_ERR_URLMALF;
        h = curl_url();
        err = curl_url_set(h, CURLUPART_URL, url, 0);
        curl_url_cleanup(h);
        if (err != CURLUE_OK)
                return POOLF_ERR_URLMALF;
        return POOLF_ERR_NONE;
}

enum poolf_err
poolf_arg_parse(const char arg[static 1], uint_least64_t pool[static 1])
{
        size_t len;
        const char *start;

        start = arg;
        len = strlen(arg);
        if (!len)
                return POOLF_ERR_ARG_EMPTY;
        arg += len - 1;
        while (arg != start) {
                if (isdigit((unsigned char) *arg)) {
                        arg--;
                } else if (*arg == '/') {
                        arg++;
                        break;
                } else {
                        break;
                }
        }
        if (start + len == arg)
                return POOLF_ERR_BADARG;
        errno = 0;
        *pool = strtoull(arg, 0, 10);
        if (errno != 0)
                return POOLF_ERR_BADARG;
        return POOLF_ERR_NONE;
}

enum poolf_err
poolf_json_fetch(const char url[static 1], json_t *root[static 1])
{
        enum poolf_err ret;
        char *json;

        ret = POOLF_ERR_NONE;
        json = poolf_request_string_sync(url);
        if (!json) {
                ret = POOLF_ERR_BADREQ;
                goto cleanup;
        }
        *root = json_loads(json, 0, 0);
        if (!*root) {
                ret = POOLF_ERR_PARSE;
                goto cleanup;
        }
cleanup:
        free(json);
        return ret;
}

enum poolf_err
poolf_page_fetch_parse_print(uint_least64_t pool, uint_least64_t page,
        uint_least64_t posts_size[static 1])
{
        char *url;
        enum poolf_err ret;
        enum poolf_err err;
        uint_least64_t i;
        json_t *root;
        json_t *root_posts;
        json_t *root_posts_elem;
        json_t *root_posts_elem_fileurl;
        const char *fileurl_string;

        ret = POOLF_ERR_NONE;
        url = poolf_url(pool, page);
        if (!url)
                return POOLF_ERR_MEM;
        err = poolf_json_fetch(url, &root);
        free(url);
        url = 0;
        if (err != POOLF_ERR_NONE) {
                ret = err;
                goto cleanup;
        }
        root_posts = json_object_get(root, "posts");
        if (!root_posts || json_typeof(root_posts) != JSON_ARRAY) {
                ret = POOLF_ERR_INVALID;
                goto cleanup;
        }
        if (!(*posts_size = json_array_size(root_posts)))
                goto cleanup;
#define POOLF_FURL_PARSE_ERR(pooln, pagen, index, errt)                 \
        fprintf(stderr, "error in pool %"PRIuLEAST64                    \
                        " page %"PRIuLEAST64                            \
                        " entry %zu: %s\n",                             \
                        pooln, pagen, index + 1, poolf_error_string[errt])
        for (i = 0; i < *posts_size; i++) {
                root_posts_elem = json_array_get(root_posts, i);
                if (!root_posts_elem) {
                        ret = POOLF_ERR_INVALID;
                        goto cleanup;
                }
                root_posts_elem_fileurl = json_object_get(root_posts_elem,
                                "file_url");
                if (!root_posts_elem_fileurl
                        || json_typeof(root_posts_elem_fileurl) != JSON_STRING
                        ) {
                        POOLF_FURL_PARSE_ERR(pool, page, i, POOLF_ERR_INVALID);
                        puts("");
                        continue;
                }
                fileurl_string = json_string_value(root_posts_elem_fileurl);
                if (!fileurl_string) {
                        POOLF_FURL_PARSE_ERR(pool, page, i, POOLF_ERR_INVALID);
                        puts("");
                        continue;
                }
                err = poolf_url_validate(fileurl_string);
                if (err != POOLF_ERR_NONE) {
                        POOLF_FURL_PARSE_ERR(pool, page, i, err);
                        puts("");
                } else {
                        puts(fileurl_string);
                }
        }
#undef POOLF_FURL_PARSE_ERR
cleanup:
        json_decref(root);
        return ret;
}

void
poolf_sleep_at_least(struct timespec t)
{
        struct timespec r;
        int intr;
inf:
        errno = 0;
        intr = nanosleep(&t, &r);
        if (intr == -1 && errno == EINTR)
                t = r;
        else
                return;
        goto inf;
}

int
main(int argc, char **argv)
{
        static uint_least64_t pool_id;
        static uint_least64_t post_count;
        static uint_least64_t page_index;
        static uint_least64_t page_entry_count;
        static enum poolf_err err;
        static char *url;
        static json_t *root;
        static json_t *root_postcount;
        static struct timespec one_second = {
                .tv_sec = 1,
                .tv_nsec = 0
        };

        setlocale(LC_ALL, "");
        if (argc == 1 || argc != 2) {
                fputs(POOLF_USAGE, stderr);
                return EXIT_FAILURE;
        }
        err = poolf_arg_parse(argv[1], &pool_id);
        if (err != POOLF_ERR_NONE) {
                fprintf(stderr, "%s\n", poolf_error_string[err]);
                return EXIT_FAILURE;
        }
        url = poolf_url(pool_id, 1);
        if (err != POOLF_ERR_NONE) {
                fprintf(stderr, "%s\n", poolf_error_string[err]);
                return EXIT_FAILURE;
        }
        err = poolf_json_fetch(url, &root);
        free(url);
        if (err != POOLF_ERR_NONE) {
                fprintf(stderr, "%s\n", poolf_error_string[err]);
                return EXIT_FAILURE;
        }
        root_postcount = json_object_get(root, "post_count");
        if (!root_postcount || json_typeof(root_postcount) != JSON_INTEGER) {
                json_decref(root);
                fprintf(stderr, "%s\n", poolf_error_string[POOLF_ERR_INVALID]);
                return EXIT_FAILURE;
        }
        post_count = (uint_least64_t) json_integer_value(root_postcount);
        json_decref(root);
        page_index = 1;
        while (post_count) {
                poolf_sleep_at_least(one_second);
                err = poolf_page_fetch_parse_print(pool_id, page_index,
                                &page_entry_count);
                if (err != POOLF_ERR_NONE) {
                        fprintf(stderr, "critical error on page %"PRIuLEAST64
                                        ": %s\n",
                                        page_index, poolf_error_string[err]);
                        return EXIT_FAILURE;
                }
                page_index++;
                if (page_entry_count <= post_count)
                        post_count -= page_entry_count;
                else
                        post_count = 0;
        }
        return EXIT_SUCCESS;
}
