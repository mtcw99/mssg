#ifndef GENERIC_LIST_H
#define GENERIC_LIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct generic_list {
	void	 	**list;
	uint32_t	length;
	uint32_t	allocated;
	uint32_t	ALLOC_CHUNK;
	size_t		type_size;
	void		(*cleanup)(void *);
};

extern void generic_list_create(struct generic_list *generic_list,
		const uint32_t alloc_chunk,
		const size_t type_size,
		void (* const cleanup)(void *));
extern void generic_list_expand(struct generic_list *generic_list);
extern void *generic_list_get(struct generic_list *generic_list,
		const uint32_t index);
extern void *generic_list_get_last(struct generic_list *generic_list);
extern void *generic_list_add(struct generic_list *generic_list);
extern void generic_list_destroy(struct generic_list *generic_list);

#ifdef GENERIC_LIST_IMPLEMENTATION_H

void
generic_list_create(struct generic_list *generic_list,
		const uint32_t alloc_chunk,
		const size_t type_size,
		void (* const cleanup)(void *))
{
	generic_list->list = NULL;
	generic_list->length = 0;
	generic_list->allocated = 0;
	generic_list->ALLOC_CHUNK = alloc_chunk;
	generic_list->type_size = type_size;
	generic_list->cleanup = cleanup;
}

void
generic_list_expand(struct generic_list *generic_list)
{
	if (generic_list->list == NULL)
	{
		generic_list->allocated = generic_list->ALLOC_CHUNK;
		generic_list->list = calloc(
				sizeof(void *), generic_list->allocated);
		for (uint32_t i = 0; i < generic_list->allocated; ++i)
		{
			generic_list->list[i] = calloc(
					generic_list->type_size, 1);
		}
	}
	else if (generic_list->length == (generic_list->allocated - 1))
	{
		const uint32_t i_start = generic_list->allocated;
		generic_list->allocated += generic_list->ALLOC_CHUNK;
		generic_list->list = realloc(generic_list->list,
				generic_list->type_size *
					generic_list->allocated
				);
		for (uint32_t i = i_start; i < generic_list->allocated; ++i)
		{
			generic_list->list[i] = calloc(
					generic_list->type_size, 1);
		}
	}
}

void *
generic_list_get(struct generic_list *generic_list,
		const uint32_t index)
{
	if (generic_list->list == NULL)
	{
		return NULL;
	}

	return (index < generic_list->length) ?
		generic_list->list[index] : NULL;
}

void *
generic_list_get_last(struct generic_list *generic_list)
{
	return (generic_list->length > 0) ?
		generic_list->list[generic_list->length - 1] : NULL;
}

void *
generic_list_add(struct generic_list *generic_list)
{
	generic_list_expand(generic_list);
	return generic_list->list[generic_list->length++];
}

void
generic_list_destroy(struct generic_list *generic_list)
{
	for (uint32_t i = 0; i < generic_list->allocated; ++i)
	{
		if (generic_list->cleanup != NULL)
		{
			generic_list->cleanup(generic_list->list[i]);
		}
		free(generic_list->list[i]);
	}
	free(generic_list->list);
	generic_list = NULL;
}

#endif // GENERIC_LIST_IMPLEMENTATION_H

#endif // GENERIC_LIST_H

