#include "files.h"
#include "states.h"
#include "extra.h"

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define ALLOC_FILES (2560)
#define PATH_SIZE (1024)

char *
path_omit_name(const char *path)
{
	unsigned int i, slash_pos = 0, path_len = strlen(path);
	char *new_path = calloc(path_len, sizeof(char));

	for (i = 0; path[i] != '\0' && i < path_len; ++i) {
		if (path[i] == '/') {
			slash_pos = i;
		}
	}

	strncpy(new_path, path, ++slash_pos);
	if (new_path[strlen(new_path) - 1] != '/') {
		free(new_path);
		return NULL;
	}
	return new_path;
}

char *
make_rel(const char *base_path, const char *full_path)
{
	int i;
	char *rel_path = calloc(PATH_SIZE, sizeof(char));

	for (i = 0; base_path[i] == full_path[i]; ++i) {
	}

	++i;	// Take out the forward-slash in the beginning

	strncpy(rel_path, full_path+(i), strlen(full_path)-i);
	return rel_path;
}

char *
make_build(const char *src_path)
{
	int i;
	char *build_path = calloc(PATH_SIZE, sizeof(char));
	char src_prefix[] = "src/";

	for (i = 0; src_path[i] == src_prefix[i]; ++i) {
	}

	// Return NULL if not for build_path
	if ((i < 4) || (strstr(src_path, "index.") == NULL)) {
		free(build_path);
		return NULL;
	}

	sprintf(build_path, "build/%s", src_path+(i));

	return build_path;
}

files *
files_new(void)
{
	files *f = malloc(sizeof(files));

	f->fil = malloc(ALLOC_FILES * sizeof(file_info));
	for (int i=0; i < ALLOC_FILES; ++i) {
		f->fil[i].type = -1;
		f->fil[i].path_full = calloc(PATH_SIZE, sizeof(char));
		f->fil[i].path_relative = calloc(PATH_SIZE, sizeof(char));
	}
	f->fii = 0;

	return f;
}

int
files_destroy(files *f)
{
	for (int i=0; i < ALLOC_FILES; ++i) {
		if (f->fil[i].path_full != NULL) { free(f->fil[i].path_full); }
		if (f->fil[i].path_relative != NULL) { free(f->fil[i].path_relative); }
		if (f->fil[i].make_path != NULL) { free(f->fil[i].make_path); }
	}
	free(f->fil);
	free(f);

	return 0;
}

int
file_read(file_info *fil, state *s)
{
	state_set_level_file(s, fil->path_relative, fil->type);
	// Type is HTML file
	if (fil->type == 0) {
		state_set_output_file(s, fil->make_path);
	}
	state_generate(s);

	return 0;
}

int
files_directory(files *f, const char *base_dirpath, const char *dirpath, int type)
{
	struct dirent *entry;
	DIR *dp = NULL;
	char *path_full = calloc(PATH_SIZE, sizeof(char));

	if (path_full == NULL) {
		perror("calloc");
		return -2;
	}

	if ((dp = opendir(dirpath)) == NULL) {
		perror("opendir");
		return -1;
	}

	while ((entry = readdir(dp))) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			sprintf(path_full, "%s/%s", dirpath, entry->d_name);

			// 4 == Directory
			if (entry->d_type == 4) {
				// If in base directory
				if (type == -1) {
					if (!strcmp(entry->d_name, "src")) {
						files_directory(f, base_dirpath, path_full, 0);
					}
				} else {
					files_directory(f, base_dirpath, path_full, type);
				}
			} else {
				if (!strcmp(entry->d_name, "config")) {
					f->fil[f->fii].type = 1;
				} else if (type != -1) {
					f->fil[f->fii].type = 0;
				}

				if (f->fil[f->fii].type != -1) {
					strcpy(f->fil[f->fii].path_full, path_full);
					strcpy(f->fil[f->fii].path_relative, make_rel(base_dirpath, path_full));
					++f->fii;
				}
			}
		}
	}

	if (path_full != NULL) {
		free(path_full);
	}
	closedir(dp);
	return 0;
}

int
files_build(files *f, const char *startpath)
{
	int type_loop = 1;
	unsigned int i;
	state *s = state_new();

	files_directory(f, startpath, startpath, -1);

	// Make the make paths (if available)
	for (i = 0; i < f->fii; ++i) {
		f->fil[i].make_path = make_build(f->fil[i].path_relative);
	}

	// Try to make the directory
	if (mkdir("build", 0777) < 0) {
		// If the error is not directory already exists
		if (errno != 17) {
			perror("Error: Directory creation error:");
		}
	} else {
		printf("Build directory created\n");
	}

	i = 0;
	while (type_loop >= 0) {
		// If i is out of range, reset i and shift down type
		if (i >= f->fii) {
			--type_loop;
			i = 0;
			continue;
		}

		switch (type_loop) {
		case 1:
			if (f->fil[i].type == 1) {
#ifdef DEBUG
				printf("file_read config: \"%s\"\n", f->fil[i].path_relative);
#endif
				file_read(&f->fil[i], s);
			}
			break;
		case 0:
			if (f->fil[i].make_path != NULL) {
#ifdef DEBUG
				printf("file_read html: \"%s\" \"%s\"\n", f->fil[i].make_path, f->fil[i].path_relative);
#endif
				// Try to make the directory
				char *pon = path_omit_name(f->fil[i].make_path);
				if (pon != NULL) {
					if (m_mkdir(pon, 0777) < 0) {
						// If the error is not directory already exists
						if (errno != 17) {
							perror("Error: Directory creation error:");
						}
					}
					free(pon);
				}

				file_read(&f->fil[i], s);
			}
			break;
		}

		++i;
	}

	state_destroy(s);

	return 0;
}

int
init_site(const char *name)
{
	typedef struct {
		char *filepath;
		char *content;
	} file_create_list;

	file_create_list *f_list = (file_create_list []) {
		// Directories
		{"src", NULL},
		// Files
		{"config", "string name \"Template Website\"\n"
			"lang en\n"},
		{"src/index.html", "<h1>Template website</h1>\n"
			"<p>Test site generated by mssg</p>\n"
		},
		{"src/base.html", "<!DOCTYPE html>\n<html>\n"
			"<head><title>{{ name }}</title></head>\n"
			"<body>\n"
			"{%% SUB_CONTENT %%}\n"
			"</body>\n</html>\n</body>\n"
		},
		{NULL, NULL}
	};

	FILE *fp = NULL;
	file_create_list *f_ptr = f_list;

	// If not an empty string
	if (name[0] != '\0') {
		if (mkdir(name, 0777) < 0) {
			fprintf(stderr, "Cannot created '%s' directory\n", name);
			perror("Error: Directory creation error");
			return EXIT_FAILURE;
		} else {
			// If successful change to the directory
			if (chdir(name) < 0) {
				perror("Error: Cannot change directory");
				return EXIT_FAILURE;
			}
		}
	}

	// File creation
	for (; f_ptr->filepath != NULL; ++f_ptr) {
		// Check if directory, otherwise a file
		if (f_ptr->content == NULL) {
			if (mkdir(f_ptr->filepath, 0777) < 0) {
				fprintf(stderr, "Cannot created src directory\n");
				perror("Error: Directory creation error");
				return EXIT_FAILURE;
			}
		} else {
			if ((fp = fopen(f_ptr->filepath, "w")) == NULL) {
				fprintf(stderr, "Cannot create '%s' file\n", f_ptr->filepath);
				perror("Error: File write error");
				return EXIT_FAILURE;
			}

			fprintf(fp, f_ptr->content);

			fclose(fp);
		}
	}

	// Print successful if no error
	if (name[0] == '\0') {
		printf("Site initialized\n");
	} else {
		printf("Site '%s' initialized\n", (name[0] != '\0') ? name : "");
	}

	return 0;
}

