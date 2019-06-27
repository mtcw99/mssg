#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "mssg.h"
#include "messages.h"

#define LINE_BUFFER (1024)

int
preproc(const char *line, char *htmlline, const int inpara)
{
	sprintf(htmlline, "%s", (inpara) ? "</p>" : "");
	return 0;
}

int
newline(const char *line, char *htmlline, const int inpara)
{
	sprintf(htmlline, "%s", (inpara) ? "</p>" : "");
	return 0;
}

int
header(const char *line, char *htmlline, const int inpara)
{
	int ret_val = 0;
	int hcounter = 1;

	while (line[hcounter] == '#') {
		++hcounter;
	}
	if (hcounter > 6) {
		fprintf(stderr, "%s: warning: Header level too high,"
				" setting to h6\n", TSH);
		hcounter = 6;
		ret_val = 1;
	}
	sprintf(htmlline, "%s<h%d>%.*s</h%d>\n",
			(inpara) ? "</p>" : "",
			hcounter,
			(int) strlen(line+hcounter)-1, line+hcounter,
			hcounter);
	return ret_val;
}

char *
mdline_to_html(const char *line, int *inpara)
{
	int func_used = 0;

	typedef struct func_m {
		char symbol;
		int (*f)(const char *, char *, const int);
	} func_m;

	func_m *funcs = (func_m []) {
		{'#',	&header},
		{'\n',	&newline},
		{'%',	&preproc},
		{'\0',	NULL}
	};

	func_m *fp = funcs;

	char *htmlline = (char *) malloc(sizeof(char) *
			(strlen(line) + LINE_BUFFER));

	for (; fp->symbol != '\0'; ++fp) {
		if (line[0] == fp->symbol) {
			(*fp->f)(line, htmlline, *inpara);
			*inpara = 0;
			func_used = 1;
			break;
		}
	}

	// Paragraph
	if (!func_used) {
		sprintf(htmlline, "<p>\n%s", line);
		*inpara = 1;
	}

	return htmlline;
}

