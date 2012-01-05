#include "../../uwsgi.h"

extern struct uwsgi_server uwsgi;

struct uwsgi_cgi {
	struct uwsgi_dyn_dict *mountpoint;
	struct uwsgi_dyn_dict *helpers;
	int buffer_size;
	int timeout;
	struct uwsgi_string_list *index;
	struct uwsgi_string_list *allowed_ext;
	struct uwsgi_string_list *unset;
	struct uwsgi_string_list *loadlib;
	int optimize;
	int has_mountpoints;
	struct uwsgi_dyn_dict *default_cgi;
	int path_info;
} uc ;

#define LONG_ARGS_CGI_BASE		17000 + ((9 + 1) * 1000)
#define LONG_ARGS_CGI			LONG_ARGS_CGI_BASE + 1
#define LONG_ARGS_CGI_MAP_HELPER	LONG_ARGS_CGI_BASE + 2
#define LONG_ARGS_CGI_BUFFER_SIZE	LONG_ARGS_CGI_BASE + 3
#define LONG_ARGS_CGI_TIMEOUT		LONG_ARGS_CGI_BASE + 4
#define LONG_ARGS_CGI_INDEX		LONG_ARGS_CGI_BASE + 5
#define LONG_ARGS_CGI_ALLOWED_EXT	LONG_ARGS_CGI_BASE + 6
#define LONG_ARGS_CGI_UNSET		LONG_ARGS_CGI_BASE + 7
#define LONG_ARGS_CGI_LOADLIB		LONG_ARGS_CGI_BASE + 8

struct option uwsgi_cgi_options[] = {

        {"cgi", required_argument, 0, LONG_ARGS_CGI},
        {"cgi-map-helper", required_argument, 0, LONG_ARGS_CGI_MAP_HELPER},
        {"cgi-helper", required_argument, 0, LONG_ARGS_CGI_MAP_HELPER},
        {"cgi-buffer-size", required_argument, 0, LONG_ARGS_CGI_BUFFER_SIZE},
        {"cgi-timeout", required_argument, 0, LONG_ARGS_CGI_TIMEOUT},
        {"cgi-index", required_argument, 0, LONG_ARGS_CGI_INDEX},
        {"cgi-allowed-ext", required_argument, 0, LONG_ARGS_CGI_ALLOWED_EXT},
        {"cgi-unset", required_argument, 0, LONG_ARGS_CGI_UNSET},
        {"cgi-loadlib", required_argument, 0, LONG_ARGS_CGI_LOADLIB},
        {"cgi-optimize", no_argument, &uc.optimize, 1},
        {"cgi-optimized", no_argument, &uc.optimize, 1},
        {"cgi-path-info", no_argument, &uc.path_info, 1},
        {0, 0, 0, 0},

};

void uwsgi_cgi_404(struct wsgi_request *wsgi_req) {

	wsgi_req->status = 404;
	wsgi_req->headers_size += wsgi_req->socket->proto_write(wsgi_req, "HTTP/1.0 404 Not Found\r\n\r\nNot Found", 35);
}

void uwsgi_cgi_403(struct wsgi_request *wsgi_req) {

	wsgi_req->status = 403;
	wsgi_req->headers_size += wsgi_req->socket->proto_write(wsgi_req, "HTTP/1.0 403 Forbidden\r\n\r\nForbidden", 35);

}

void uwsgi_cgi_redirect_to_slash(struct wsgi_request *wsgi_req) {
	
	struct iovec iov[6];

	wsgi_req->status = 301;
	iov[0].iov_base = wsgi_req->protocol;
	iov[0].iov_len = wsgi_req->protocol_len;
	iov[1].iov_base = " 301 Moved Permanently\r\n";
	iov[1].iov_len = 24;
	wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 2);

	iov[0].iov_base = "Location: ";
	iov[0].iov_len = 10;
	iov[1].iov_base = wsgi_req->path_info;
	iov[1].iov_len = wsgi_req->path_info_len;
	iov[2].iov_base = "/";
	iov[2].iov_len = 1;

	if (wsgi_req->query_string_len > 0) {
		iov[3].iov_base = "?";
		iov[3].iov_len = 1;
		iov[4].iov_base = wsgi_req->query_string;
		iov[4].iov_len = wsgi_req->query_string_len;
		iov[5].iov_base = "\r\n\r\n";
		iov[5].iov_len = 4;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 6);
		wsgi_req->header_cnt++;
	}
	else {
		iov[3].iov_base = "\r\n\r\n";
		iov[3].iov_len = 4;
		wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 4);
		wsgi_req->header_cnt++;
	}
}

void uwsgi_cgi_apps() {

	struct uwsgi_dyn_dict *udd = uc.mountpoint;
	struct stat st;

	while(udd) {
		if (udd->vallen) {
			if (uc.optimize) {
				udd->value = realpath(udd->value, NULL);	
				if (!udd->value) {
					uwsgi_log("unable to find CGI path %.*s\n", udd->vallen, udd->value);
					exit(1);
				}
				udd->vallen = strlen(udd->value);
				udd->status = 1;
				if (stat(udd->value, &st)) {
					uwsgi_error("stat()");
					uwsgi_log("something horrible happened during CGI initialization\n");
					exit(1);
				}

				if (!S_ISDIR(st.st_mode)) {
					udd->status = 2;
				}
			}
			uc.has_mountpoints = 1;
			uwsgi_log("initialized CGI mountpoint: %.*s = %.*s\n", udd->keylen, udd->key, udd->vallen, udd->value);
		}
		else {
			if (uc.optimize) {
				udd->key = realpath(udd->key, NULL);
				if (!udd->key) {
                                        uwsgi_log("unable to find CGI path %.*s\n", udd->keylen, udd->key);
                                        exit(1);
                                }
                                udd->keylen = strlen(udd->key);
				udd->status = 1;

				if (stat(udd->key, &st)) {
                                        uwsgi_error("stat()");  
                                        uwsgi_log("something horrible happened during CGI initialization\n");
                                        exit(1);
                                }

                                if (!S_ISDIR(st.st_mode)) {
                                        udd->status = 2;
                                }

			}
			uwsgi_log("initialized CGI path: %.*s\n", udd->keylen, udd->key);
			uc.default_cgi = udd;
		}
		udd = udd->next;
	}

}

int uwsgi_cgi_init(){

	void (*cgi_sym)(void);

	if (!uc.buffer_size) uc.buffer_size = 65536;
	if (!uc.timeout) uc.timeout = 60;

	struct uwsgi_string_list *ll = uc.loadlib;
	while(ll) {
		char *colon = strchr(ll->value, ':');
		if (!colon) {
			uwsgi_log("invalid cgi-loadlib syntax, must be in the form lib:func\n");
			exit(1);
		}
		*colon = 0;
		void *cgi_lib = dlopen(ll->value, RTLD_NOW | RTLD_GLOBAL);
		if (!cgi_lib) {
			uwsgi_log( "cgi-loadlib: %s\n", dlerror());
			exit(1);
		}

		cgi_sym = dlsym(cgi_lib, colon+1);
		if (!cgi_sym) {
			uwsgi_log("unknown symbol %s in lib %s\n", colon+1, ll->value);
			exit(1);
		}

		cgi_sym();
		uwsgi_log("[cgi-loadlib] loaded symbol %s from %s\n", colon+1, ll->value);

		*colon = ':';
		ll = ll->next;
	}

	return 0;	

}

char *uwsgi_cgi_get_helper(char *filename) {

	struct uwsgi_dyn_dict *helpers = uc.helpers;
	size_t len = strlen(filename);

	while(helpers) {
		if (len >= (size_t) helpers->keylen) {
			if (!uwsgi_strncmp((filename+len)-helpers->keylen, helpers->keylen, helpers->key, helpers->keylen)) {
				return helpers->value;
			}
		}
		helpers = helpers->next;
	}

	return NULL;
	
}

int uwsgi_cgi_parse(struct wsgi_request *wsgi_req, char *buf, size_t len) {

	size_t i;
	char *key = buf, *value = NULL;
	size_t header_size = 0;
	int status_sent = 0;

	struct uwsgi_string_list *ah = uwsgi.additional_headers;

	struct iovec iov[4];

	for(i=0;i<len;i++) {
		// end of a line
		if (buf[i] == '\n') {
			// end of headers
			if (key == NULL) {
				goto send_body;
			}
			// invalid header
			else if (value == NULL) {
				return -1;	
			}
			header_size = (buf+i) - key;
			// security check
			if (buf+i > buf) {
				if ((buf[i-1]) == '\r') {
					header_size--;
				}
			}

#ifdef UWSGI_DEBUG
			uwsgi_log("found CGI header: %.*s\n", header_size, key);
#endif

			if (status_sent == 0) {
				// "Status: NNN"
				if (header_size >= 11) {
					if (!strncasecmp("Status: ", key, 8)) {
						wsgi_req->status = uwsgi_str3_num(key+8);
						iov[0].iov_base = wsgi_req->protocol;
						iov[0].iov_len = wsgi_req->protocol_len;
						iov[1].iov_base = " ";
                                		iov[1].iov_len = 1;
						iov[2].iov_base = key+8;
						iov[2].iov_len = header_size - 8;
						iov[3].iov_base = "\r\n";
                                		iov[3].iov_len = 2;
						wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 4);
						status_sent = 1;
						key = NULL;
						value = NULL;
						continue;
					}
				}
			}

			// default status
			if (status_sent == 0) {

				// Location: X
				if (header_size >= 11) {
					if (!strncasecmp("Location: ", key, 10)) {

						wsgi_req->status = 302;
						iov[0].iov_base = wsgi_req->protocol;
						iov[0].iov_len = wsgi_req->protocol_len;
						iov[1].iov_base = " 302 Found\r\n";
						iov[1].iov_len = 12;
						wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 2);
						status_sent = 1;
					}
				}

				if (status_sent == 0) {
					wsgi_req->status = 200;
					iov[0].iov_base = wsgi_req->protocol;
					iov[0].iov_len = wsgi_req->protocol_len;
					iov[1].iov_base = " 200 OK\r\n";
					iov[1].iov_len = 9;
					wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 2);
					status_sent = 1;
				}
			}

			iov[0].iov_base = key;
			iov[0].iov_len = header_size;
			iov[1].iov_base = "\r\n";
                        iov[1].iov_len = 2;
			wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 2);
                        wsgi_req->header_cnt++;

			key = NULL;
			value = NULL;
		}
		else if (buf[i] == ':') {
			value = buf+i;
		}
		else if (buf[i] != '\r') {
			if (key == NULL) {
				key = buf + i;
			}
		}
	}

	return -1;

send_body:

        while(ah) {
        	iov[0].iov_base = ah->value;
        	iov[0].iov_len = ah->len;
                iov[1].iov_base = "\r\n";
                iov[1].iov_len = 2;
                wsgi_req->headers_size += wsgi_req->socket->proto_writev_header(wsgi_req, iov, 2);
                wsgi_req->header_cnt++;
        	ah = ah->next;
        }

	wsgi_req->headers_size += wsgi_req->socket->proto_write_header(wsgi_req, "\r\n", 2);
	if (len-i > 0) {
		wsgi_req->response_size += wsgi_req->socket->proto_write(wsgi_req, buf+i, len-i);
	}

	return 0;	
}

char *uwsgi_cgi_get_docroot(char *path_info, uint16_t path_info_len, int *need_free, int *is_a_file, int *discard_base, char **script_name) {

	struct uwsgi_dyn_dict *udd = uc.mountpoint, *choosen_udd = NULL;
	int best_found = 0;
	struct stat st;
	char *path = NULL;

	if (uc.has_mountpoints) {
		while(udd) {
			if (udd->vallen) {
				if (!uwsgi_starts_with(path_info, path_info_len, udd->key, udd->keylen) && udd->keylen > best_found) {
					best_found = udd->keylen ;
					choosen_udd = udd;
					path = udd->value;
					*script_name = udd->key;
					*discard_base = udd->keylen;
					if (udd->key[udd->keylen-1] == '/') {
						*discard_base = *discard_base-1;
					}
				}
			}
			udd = udd->next;
		}
	}

	if (choosen_udd == NULL) {
		choosen_udd = uc.default_cgi;
		if (!choosen_udd) return NULL;
		path = choosen_udd->key;
	}

	if (choosen_udd->status == 0) {
		char *tmp_udd = uwsgi_malloc(PATH_MAX+1);
		if (!realpath(path, tmp_udd)) {
			return NULL;
		}

		if (stat(tmp_udd, &st)) {
			uwsgi_error("stat()");
			free(tmp_udd);
			return NULL;
		}

		if (!S_ISDIR(st.st_mode)) {
			*is_a_file = 1;
		}

		*need_free = 1;
		return tmp_udd;
	}

	if (choosen_udd->status == 2)
		*is_a_file = 1;
	return path;
}

int uwsgi_cgi_walk(struct wsgi_request *wsgi_req, char *full_path, char *docroot, size_t docroot_len, int discard_base, char **path_info) {

	// and now start walking...
        uint16_t i;
        char *ptr = wsgi_req->path_info+discard_base;
        char *dst = full_path+docroot_len;
        char *part = ptr;
        int part_size = 0;
	struct stat st;

        if (ptr[0] == '/') part_size++;

        for(i=0;i<wsgi_req->path_info_len-discard_base;i++) {
        	if (ptr[i] == '/') {
                	memcpy(dst, part, part_size-1);
                        *(dst+part_size-1) = 0;

                        if (stat(full_path, &st)) {
                        	uwsgi_cgi_404(wsgi_req);
                                return -1;
                        }


			// not a directory, stop walking
                        if (!S_ISDIR(st.st_mode)) {
				if (i < (wsgi_req->path_info_len-discard_base)-1) {
                        		*path_info = ptr + i;
				}

				return 0;
                        }


			// check for buffer overflow !!!
                        *(dst+part_size-1) = '/';
                        *(dst+part_size) = 0;

                        dst += part_size ;
                        part_size = 0;
                        part = ptr + i + 1;
         	}

                part_size++;
	}

	if (part < wsgi_req->path_info+wsgi_req->path_info_len) {
		memcpy(dst, part, part_size-1);
		*(dst+part_size-1) = 0;
	}

	return 0;


}

int uwsgi_cgi_request(struct wsgi_request *wsgi_req) {

	int i;
	pid_t cgi_pid;
	int waitpid_status;
	char *argv[3];
	char full_path[PATH_MAX];
	char tmp_path[PATH_MAX];
	int cgi_pipe[2];
	ssize_t len;
	struct stat cgi_stat;
	int need_free = 0;
	int is_a_file = 0;
	int discard_base = 0;
	size_t docroot_len = 0;
	size_t full_path_len = 0;
	char *helper = NULL;
	char *command = NULL;
	char *path_info = NULL;
	char *script_name = NULL;

	/* Standard CGI request */
	if (!wsgi_req->uh.pktsize) {
		uwsgi_log("Invalid CGI request. skip.\n");
		return -1;
	}


	if (uwsgi_parse_vars(wsgi_req)) {
		return -1;
	}

	// check for file availability (and 'runnability')

	char *docroot = uwsgi_cgi_get_docroot(wsgi_req->path_info, wsgi_req->path_info_len, &need_free, &is_a_file, &discard_base, &script_name);

	if (docroot == NULL) {
		uwsgi_cgi_404(wsgi_req);
		return UWSGI_OK;
	}

	docroot_len = strlen(docroot);
	memcpy(full_path, docroot, docroot_len);

	if (!is_a_file) {

		*(full_path+docroot_len) = '/';
		*(full_path+docroot_len+1) = 0;

		if (uwsgi_cgi_walk(wsgi_req, full_path, docroot, docroot_len, discard_base, &path_info)) {
			if (need_free)
				free(docroot);
			return UWSGI_OK;
		}

		if (realpath(full_path, tmp_path) == NULL) {
			if (need_free)
				free(docroot);
			uwsgi_cgi_404(wsgi_req);
			return UWSGI_OK;
		}

		full_path_len = strlen(tmp_path);
		// add +1 to copy the null byte
		memcpy(full_path, tmp_path, full_path_len+1);

		if (uwsgi_starts_with(full_path, full_path_len, docroot, docroot_len)) {
			if (need_free)
				free(docroot);
                	uwsgi_log("CGI security error: %s is not under %s\n", full_path, docroot);
                	return -1;
        	}

	}
	else {
		*(full_path+docroot_len) = 0;
		path_info = wsgi_req->path_info+discard_base;
	}

	if (stat(full_path, &cgi_stat)) {
		uwsgi_cgi_404(wsgi_req);
		if (need_free)
			free(docroot);
		return UWSGI_OK;
	}

	if (S_ISDIR(cgi_stat.st_mode)) {

		// add / to directories
		if (wsgi_req->path_info_len == 0 || wsgi_req->path_info[wsgi_req->path_info_len-1] != '/') {
			uwsgi_cgi_redirect_to_slash(wsgi_req);
			if (need_free)
                        	free(docroot);
                	return UWSGI_OK;
		}
		struct uwsgi_string_list *ci = uc.index;
		full_path[full_path_len] = '/';
		full_path_len++;
		int found = 0;
		while(ci) {
			if (full_path_len + ci->len + 1 < PATH_MAX) {
				// add + 1 to ensure null byte
				memcpy(full_path+full_path_len, ci->value, ci->len + 1);
				if (!access(full_path, R_OK)) {
					found = 1;
					break;
				}
			}
			ci = ci->next;
		}

		if (!found) {
			uwsgi_cgi_404(wsgi_req);
			if (need_free)
				free(docroot);
			return UWSGI_OK;
		}

	}
	else {
		full_path_len = strlen(full_path);
	}

	int cgi_allowed = 1;
	struct uwsgi_string_list *allowed = uc.allowed_ext;
	while(allowed) {
		cgi_allowed = 0;
		if (full_path_len >= allowed->len) {
			if (!uwsgi_strncmp(full_path+(full_path_len-allowed->len), allowed->len, allowed->value, allowed->len)) {
				cgi_allowed = 1;
				break;
			}
		}
		allowed = allowed->next;
	}

	if (!cgi_allowed) {
		uwsgi_cgi_403(wsgi_req);
		if (need_free)
			free(docroot);
		return UWSGI_OK;
	}

	if (is_a_file) {
		command = docroot;
	}
	else {
		command = full_path;
		helper = uwsgi_cgi_get_helper(full_path);

		if (helper == NULL) {
			if (access(full_path, X_OK)) {
				uwsgi_error("access()");
				uwsgi_cgi_403(wsgi_req);
                		if (need_free)
                        		free(docroot);
				return UWSGI_OK;
			}
		}
	}

	if (pipe(cgi_pipe)) {
		if (need_free)
			free(docroot);
		uwsgi_error("pipe()");
		return UWSGI_OK;
	}

	cgi_pid = fork();

	if (cgi_pid < 0) {
		uwsgi_error("fork()");
		if (need_free)
			free(docroot);
		return -1;
	}

	if (cgi_pid > 0) {

		if (need_free)
			free(docroot);

		close(cgi_pipe[1]);
		// wait for data
		char *headers_buf = uwsgi_malloc(uc.buffer_size);
		char *ptr = headers_buf;
		size_t remains = uc.buffer_size;
		int completed = 0;
		while(remains > 0) {
			int ret = uwsgi_waitfd(cgi_pipe[0], uc.timeout);
			if (ret > 0) {
				len = read(cgi_pipe[0], ptr, remains);
				if (len > 0) {
					ptr+=len;
					remains -= len;
				}
				else if (len == 0) {
					completed = 1;
					break;
				}
				else {
					uwsgi_error("read()");
					goto clear;
				}
				continue;
			}
			else if (ret == 0) {
				uwsgi_log("CGI timeout !!!\n");
				goto clear;
			}
			break;
		}

		if (uwsgi_cgi_parse(wsgi_req, headers_buf, uc.buffer_size-remains)) {
			uwsgi_log("invalid CGI output !!!\n");
			goto clear;
		}

		while (!completed) {
			int ret = uwsgi_waitfd(cgi_pipe[0], uc.timeout);
			if (ret > 0) {
				len = read(cgi_pipe[0], headers_buf, uc.buffer_size);
				if (len > 0) {
					wsgi_req->response_size += wsgi_req->socket->proto_write(wsgi_req, headers_buf, len);
				}
				// end of output
				else if (len == 0) {
					break;
				}
				else {
					uwsgi_error("read()");
					goto clear;
				}
				continue;
			}
			else if (ret == 0) {
                                uwsgi_log("CGI timeout !!!\n");
                                goto clear;
                        }
                        break;
		}

clear:
		free(headers_buf);
		close(cgi_pipe[0]);

		// now wait for process exit/death
		if (waitpid(cgi_pid, &waitpid_status, 0) < 0) {
			uwsgi_error("waitpid()");
		}

		return UWSGI_OK;
	}

	// close all the fd except wsgi_req->poll.fd and 2;

	for(i=0;i< (int)uwsgi.max_fd;i++) {
		if (i != wsgi_req->poll.fd && i != 2 && i != cgi_pipe[1]) {
			close(i);
		}
	}

	// now map wsgi_req->poll.fd to 0 & cgi_pipe[1] to 1
	if (wsgi_req->poll.fd != 0) {
		dup2(wsgi_req->poll.fd, 0);
		close(wsgi_req->poll.fd);
	}

#ifdef UWSGI_DEBUG
	uwsgi_log("mapping cgi_pipe %d to 1\n", cgi_pipe[1]);
#endif

	dup2(cgi_pipe[1],1);
	
	// fill cgi env
	for(i=0;i<wsgi_req->var_cnt;i++) {
		// no need to free the putenv() memory
		if (putenv(uwsgi_concat3n(wsgi_req->hvec[i].iov_base, wsgi_req->hvec[i].iov_len, "=", 1, wsgi_req->hvec[i+1].iov_base, wsgi_req->hvec[i+1].iov_len))) {
			uwsgi_error("putenv()");
		}
		i++;
	}


	if (setenv("GATEWAY_INTERFACE", "CGI/1.1", 0)) {
		uwsgi_error("setenv()");
	}

	if (setenv("SERVER_SOFTWARE", uwsgi_concat2("uWSGI/", UWSGI_VERSION), 0)) {
		uwsgi_error("setenv()");
	}

	// for newer php
	if (setenv("REDIRECT_STATUS", "200", 0)) {
		uwsgi_error("setenv()");
	}

	if (path_info && wsgi_req->path_info_len-discard_base > 0) {
		if (setenv("PATH_INFO", uwsgi_concat2n(path_info, wsgi_req->path_info_len-discard_base, "", 0), 1)) {
			uwsgi_error("setenv()");
		}

		if (setenv("PATH_TRANSLATED", uwsgi_concat4n(docroot, docroot_len, "/", 1, path_info, wsgi_req->path_info_len-discard_base, "", 0) , 1)) {
			uwsgi_error("setenv()");
		}
	}
	else {
		unsetenv("PATH_INFO");
		unsetenv("PATH_TRANSLATED");
	}

	if (is_a_file) {
		if (setenv("DOCUMENT_ROOT", uwsgi.cwd, 0)) {
			uwsgi_error("setenv()");
		}

		if (setenv("SCRIPT_FILENAME", docroot, 0)) {
			uwsgi_error("setenv()");
		}

		if (script_name && discard_base > 1) {
			if (setenv("SCRIPT_NAME", uwsgi_concat2n(script_name, discard_base, "", 0), 1)) {
				uwsgi_error("setenv()");
			}
		}
	}
	else {
		if (setenv("DOCUMENT_ROOT", docroot, 0)) {
			uwsgi_error("setenv()");
		}

		if (setenv("SCRIPT_FILENAME", full_path, 0)) {
			uwsgi_error("setenv()");
		}

		if (setenv("SCRIPT_NAME", full_path+(docroot_len-discard_base), 1)) {
			uwsgi_error("setenv()");
		}

		char *base = uwsgi_get_last_char(full_path, '/');
		if (base) {
			// a little trick :P
			*base = 0;
			if (chdir(full_path)) {
                                uwsgi_error("chdir()");
                        }
			*base = '/';
		}
		else {
			if (chdir(docroot)) {
				uwsgi_error("chdir()");
			}
		}
	}

	struct uwsgi_string_list *drop_env = uc.unset;
	while(drop_env) {
		unsetenv(drop_env->value);
		drop_env = drop_env->next;
	}

	if (helper) {
		if (!uwsgi_starts_with(helper, strlen(helper), "sym://", 6)) {
			void (*cgi_func)(char *) = dlsym(RTLD_DEFAULT, helper+6);
			if (cgi_func) {
				cgi_func(command);
			}
			else {
				uwsgi_log("unable to find symbol %s\n", helper+6);
			}
			exit(0);	
		}
		argv[0] = helper;
		argv[1] = command;
		argv[2] = NULL;
	}
	else {
		argv[0] = command;
		argv[1] = NULL;
	}

	if (execvp(argv[0], argv)) {
		uwsgi_error("execvp()");
	}

	// never here
	exit(1);
}


void uwsgi_cgi_after_request(struct wsgi_request *wsgi_req) {

	if (uwsgi.shared->options[UWSGI_OPTION_LOGGING])
		log_request(wsgi_req);
}

int uwsgi_cgi_manage_options(int i, char *optarg) {

	char *value;

        switch(i) {
                case LONG_ARGS_CGI:
			value = strchr(optarg, '=');
			if (!value) {
				uwsgi_dyn_dict_new(&uc.mountpoint, optarg, strlen(optarg), NULL, 0);
			}
			else {
				uwsgi_dyn_dict_new(&uc.mountpoint, optarg, value-optarg, value+1, strlen(value+1));
			}
                        return 1;
                case LONG_ARGS_CGI_BUFFER_SIZE:
                        uc.buffer_size = atoi(optarg);
                        return 1;
                case LONG_ARGS_CGI_TIMEOUT:
                        uc.timeout = atoi(optarg);
                        return 1;
                case LONG_ARGS_CGI_INDEX:
			uwsgi_string_new_list(&uc.index, optarg);
                        return 1;
                case LONG_ARGS_CGI_ALLOWED_EXT:
			uwsgi_string_new_list(&uc.allowed_ext, optarg);
                        return 1;
                case LONG_ARGS_CGI_LOADLIB:
			uwsgi_string_new_list(&uc.loadlib, uwsgi_str(optarg));
                        return 1;
                case LONG_ARGS_CGI_UNSET:
			uwsgi_string_new_list(&uc.unset, optarg);
                        return 1;
		case LONG_ARGS_CGI_MAP_HELPER:
			value = strchr(optarg, '=');
			if (!value) {
				uwsgi_log("invalid CGI helper syntax, must be ext=command\n");
				exit(1);
			}
			uwsgi_dyn_dict_new(&uc.helpers, optarg, value-optarg, value+1, strlen(value+1));
			return 1;
        }

        return 0;
}


struct uwsgi_plugin cgi_plugin = {

	.name = "cgi",
	.modifier1 = 9,
	.init = uwsgi_cgi_init,
	.init_apps = uwsgi_cgi_apps,
	.options = uwsgi_cgi_options,
	.manage_opt = uwsgi_cgi_manage_options,
	.request = uwsgi_cgi_request,
	.after_request = uwsgi_cgi_after_request,

};
