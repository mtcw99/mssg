#include "files.h"
#include "states.h"

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
make_rel(const char *base_path, const char *full_path)
{
	int i;
	char *rel_path = (char *) calloc(PATH_SIZE, sizeof(char));

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
	char *build_path = (char *) calloc(PATH_SIZE, sizeof(char));
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
files_init(void)
{
	files *f = (files *) malloc(sizeof(files));

	f->fil = (file_info *) malloc(ALLOC_FILES * sizeof(file_info));
	for (int i=0; i < ALLOC_FILES; ++i) {
		f->fil[i].type = -1;
		f->fil[i].path_full = (char *) calloc(PATH_SIZE, sizeof(char));
		f->fil[i].path_relative = (char *) calloc(PATH_SIZE, sizeof(char));
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
files_read(file_info *fil)
{
	if (mkdir("build", 0777) < 0) {
		// If the error is not directory already exists
		if (errno != 17) {
			perror("Error: Directory creation error:");
		}
	} else {
		printf("Build directory created\n");
	}

	state s;

	state_init(&s);
	state_set_level_file(&s, fil->path_relative);
	state_set_output_file(&s, fil->make_path);
	state_generate(&s);
	state_destroy(&s);

	return 0;
}

int
files_directory(files *f, const char *base_dirpath, const char *dirpath, int type)
{
	struct dirent *entry;
	DIR *dp = NULL;
	char *path_full = (char *) calloc(PATH_SIZE, sizeof(char));

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
				if (!strcmp(entry->d_name, "conf.toml")) {
					f->fil[f->fii].type = 1;
				} else if (type != -1) {
					f->fil[f->fii].type = type;
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
files_traverse(files *f, const char *startpath)
{
	files_directory(f, startpath, startpath, -1);

	for (unsigned int i=0; i < f->fii; ++i) {
		f->fil[i].make_path = make_build(f->fil[i].path_relative);
		if (f->fil[i].make_path != NULL) {
			files_read(&f->fil[i]);
		} else {
			//printf("%d: %s\n", f->fil[i].type, f->fil[i].path_relative);
		}
	}

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
