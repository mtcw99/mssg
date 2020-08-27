#include "templates.h"

#include <string.h>
#include <stdlib.h>

#include "generic_list.h"

enum vartype {
	TEMPLATE_VARTYPE_INT = 0,
	TEMPLATE_VARTYPE_DOUBLE,
	TEMPLATE_VARTYPE_STRING,
	TEMPLATE_VARTYPE_ERROR
};

struct variable {
	char		name[64];
	enum vartype	type;
	union {
		int32_t	num_int;
		double	num_double;
		char	string[256];
	} var;
};

struct block {
	char	name[256];
	FILE	*stream;
};

static void
templates__blocks_cleanup(void *data)
{
	struct block *block = data;
	if (block->stream != NULL)
	{
		fclose(block->stream);
	}
}

// In-file variables
static struct generic_list variables = {
	.list = NULL,
	.length = 0,
	.allocated = 0,
	.ALLOC_CHUNK = 16,
	.type_size = sizeof(struct variable),
	.is_pointer = false,
	.cleanup = NULL
};

static struct generic_list blocks = {
	.list = NULL,
	.length = 0,
	.allocated = 0,
	.ALLOC_CHUNK = 16,
	.type_size = sizeof(struct block),
	.is_pointer = false,
	.cleanup = templates__blocks_cleanup
};

enum templates_argc_min {
	TEMPLATE_ARGCMIN_NOT_FOUND = 0,
	TEMPLATE_ARGCMIN_ROOT = 0,
	TEMPLATE_ARGCMIN_VARIABLE = 1,
	TEMPLATE_ARGCMIN_LOOP = 3,
	TEMPLATE_ARGCMIN_SET_VAR = 2,
	TEMPLATE_ARGCMIN_SET_BLOCK = 1,
	TEMPLATE_ARGCMIN_PUT_BLOCK = 1,
	TEMPLATE_ARGCMIN_BASE = 1,
	TEMPLATE_ARGCMIN_LINK = 1,
	TEMPLATE_ARGCMIN_END = 0,
};

void
file_append_file(FILE *out, FILE *in)
{
	enum {
		CHUNK_SIZE = 512
	};

	rewind(in);
	char chunk[CHUNK_SIZE] = { 0 };
	size_t read_size = CHUNK_SIZE;
	size_t write_size = 0;
	while (read_size == CHUNK_SIZE)
	{
		read_size = fread(chunk, sizeof(char), CHUNK_SIZE, in);
		write_size = fwrite(chunk, sizeof(char), read_size, out);

		if (ferror(in) || (read_size != write_size))
		{
			fprintf(stderr, "file_append_file: rw error.\n");
			break;
		}
	}
}

static struct variable *
templates_varlist_get(const char *name)
{
	for (uint32_t i = 0; i < variables.length; ++i)
	{
		if (!strcmp(name,
			((struct variable *)
			 	generic_list_get(&variables, i))->name))
		{
			return generic_list_get(&variables, i);
		}
	}

	return NULL;
}

static FILE *
templates_block_get(const char *name)
{
	for (uint32_t i = 0; i < blocks.length; ++i)
	{
		if (!strcmp(name,
			((struct block *)
			 	generic_list_get(&blocks, i))->name))
		{
			return ((struct block *)
					generic_list_get(&blocks, i))->stream;
		}
	}

	return NULL;
}

static enum vartype
templates_dettype(const char *data)
{
	const uint32_t length = strlen(data);
	uint32_t tfloat = 0;
	uint32_t tint = 0;

	for (uint32_t i = 0; i < length; ++i)
	{
		if (i > 0 && data[i] == '.')
		{
			++tfloat;
		}
		else if ('0' <= data[i] && data[i] <= '9')
		{
			++tint;
		}
		else
		{
			return TEMPLATE_VARTYPE_STRING;
		}
	}

	if (tfloat == 1)
	{
		return TEMPLATE_VARTYPE_DOUBLE;
	}
	else if (tfloat > 1)
	{
		return TEMPLATE_VARTYPE_ERROR;
	}
	else
	{
		return TEMPLATE_VARTYPE_INT;
	}
}

FILE *
templates__add_block(const char *name)
{
	struct block *block = generic_list_add(&blocks); 
	strcpy(block->name, name);
	block->stream = tmpfile();
	return block->stream;
}

void
templates_deinit(void)
{
	generic_list_destroy(&variables);
	generic_list_destroy(&blocks);
}

static enum templates_error_codes
templates_variable(struct templates templates)
{
	FILE *out_stream = (*templates.generate_outside) ?
		templates.stream : *templates.indirect_stream;

	const struct variable *var = templates_varlist_get(templates.argv[0]);
	if (var == NULL)
	{
		return TEMPLATE_ERROR_GETVAR_NOT_FOUND;
	}

	switch (var->type)
	{
	case TEMPLATE_VARTYPE_STRING:
		fprintf(out_stream, "%s", var->var.string);
		break;
	case TEMPLATE_VARTYPE_INT:
		fprintf(out_stream, "%d", var->var.num_int);
		break;
	case TEMPLATE_VARTYPE_DOUBLE:
		fprintf(out_stream, "%f", var->var.num_double);
		break;
	default:
		break;
	}

	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_loop(struct templates templates)
{
	(void) templates;
	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_set_var(struct templates templates)
{
	const char *name = templates.argv[0];
	const char *data = templates.argv[1];
	const enum vartype type = templates_dettype(data);

	if (type == TEMPLATE_VARTYPE_ERROR)
	{
		return TEMPLATE_ERROR_SETVAR_NO_TYPE;
	}

	struct variable *var = generic_list_add(&variables);
	strcpy(var->name, name);
	var->type = type;
	switch (var->type)
	{
	case TEMPLATE_VARTYPE_INT:
		var->var.num_int = atoi(data);
		break;
	case TEMPLATE_VARTYPE_DOUBLE:
		var->var.num_double = atof(data);
		break;
	case TEMPLATE_VARTYPE_STRING:
		strcpy(var->var.string, data);
		break;
	default:
		return TEMPLATE_ERROR_SETVAR_UNUSED_TYPE;
	}

	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_set_block(struct templates templates)
{
	*templates.generate_outside = false;
	*templates.indirect_stream = templates__add_block(templates.argv[0]);

	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_put_block(struct templates templates)
{
	FILE *file = templates_block_get(templates.argv[0]);
	if (file)
	{
		file_append_file(templates.stream, file);
	}
	else
	{
		fprintf(stderr, "ERROR: Cannot get file '%s'!\n",
				templates.argv[0]);
	}
	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_link(struct templates templates)
{
	(void) templates;
	return TEMPLATE_ERROR_NONE;
}

static enum templates_error_codes
templates_end(struct templates templates)
{
	*templates.generate_outside = true;

	switch (templates.parent_type)
	{
	case TEMPLATE_SET_BLOCK:
		break;
	case TEMPLATE_LOOP:
		break;
	default:
		break;
	}

	*templates.indirect_stream = NULL;

	return TEMPLATE_ERROR_NONE;
}

struct templates_type_info {
	const char 			*keyword;
	const uint32_t			max_params;
	enum templates_error_codes (* const func)(struct templates);
};

static const struct templates_type_info templates_table[TEMPLATE_TOTAL] = {
	// enum templates_type	const char *	int32_t	function
	// 			keyword		max_p.
	[TEMPLATE_NOT_FOUND] 	= { NULL,	TEMPLATE_ARGCMIN_NOT_FOUND,	NULL },
	[TEMPLATE_ROOT] 	= { NULL,	TEMPLATE_ARGCMIN_ROOT,		NULL },
	[TEMPLATE_VARIABLE] 	= { "var",	TEMPLATE_ARGCMIN_VARIABLE,	templates_variable },
	[TEMPLATE_LOOP] 	= { "loop",	TEMPLATE_ARGCMIN_LOOP,		templates_loop },
	[TEMPLATE_SET_VAR] 	= { "set", 	TEMPLATE_ARGCMIN_SET_VAR,	templates_set_var },
	[TEMPLATE_SET_BLOCK]	= { "setblock",	TEMPLATE_ARGCMIN_SET_BLOCK,	templates_set_block },
	[TEMPLATE_PUT_BLOCK]	= { "putblock",	TEMPLATE_ARGCMIN_PUT_BLOCK,	templates_put_block },
	[TEMPLATE_BASE]		= { "base",	TEMPLATE_ARGCMIN_BASE,		NULL },
	[TEMPLATE_LINK]		= { "link",	TEMPLATE_ARGCMIN_LINK,		templates_link },
	[TEMPLATE_END]		= { "end",	TEMPLATE_ARGCMIN_END,		templates_end },
};

enum templates_type
templates_str_to_type(const char *keyword)
{
	for (uint32_t i = 0; i < TEMPLATE_TOTAL; ++i)
	{
		if (templates_table[i].keyword != NULL &&
				!strcmp(templates_table[i].keyword, keyword))
		{
			return i;
		}
	}

	return TEMPLATE_NOT_FOUND;
}

const char *
templates_type_to_str(const enum templates_type type)
{
	return templates_table[type].keyword;
}

enum templates_error_codes
templates(struct templates templates)
{
	const struct templates_type_info row = templates_table[templates.type];

	if (row.func != NULL)
	{

		if (templates.argc < row.max_params)
		{
			return TEMPLATE_ERROR_ARGCMIN_NSAT;
		}
		else
		{
			return row.func(templates);
		}
	}
	else
	{
		return TEMPLATE_ERROR_NO_FUNC;
	}
}

const char *
templates_error(const enum templates_error_codes code)
{
	static const char *error_codes_str[TEMPLATE_ERROR_TOTAL] = {
		[TEMPLATE_ERROR_NONE] = "none",
		[TEMPLATE_ERROR_ARGCMIN_NSAT] = "argc not satisfied",
		[TEMPLATE_ERROR_ARGV_ERROR] = "argv generic error",
		[TEMPLATE_ERROR_SETVAR_NO_TYPE] = "set var no type",
		[TEMPLATE_ERROR_SETVAR_UNUSED_TYPE] = "set var unused type",
		[TEMPLATE_ERROR_GETVAR_NOT_FOUND] = "get var not found",
		[TEMPLATE_ERROR_NO_FUNC] = "no function"
	};

	return error_codes_str[code];
}

