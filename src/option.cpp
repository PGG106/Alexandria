#include "option.h"
#include <stdio.h>

void option_allocation_failure(void)
{
	perror("Unable to allocate option table");
	exit(EXIT_FAILURE);
}

void init_option_list(option_list_t* list)
{
	list->options = NULL;
	list->size = 0;
	list->maxSize = 0;
}

void quit_option_list(option_list_t* list)
{
	for (size_t i = 0; i < list->size; ++i)
	{
		option_t* cur = &list->options[i];

		free(cur->name);

		switch (cur->type)
		{
		case OptionSpinInt:
		case OptionSpinFlt:
			free(cur->def);
			free(cur->min);
			free(cur->max);
			break;
		}
	}
	
	free(list->options);
	list->options = NULL;
	list->maxSize = 0;
}

option_t* insert_option(option_list_t* list, const char* name)
{
	if (list->size == list->maxSize)
	{
		list->maxSize += (!list->maxSize) ? 16 : list->maxSize;
		list->options = (option_t*) realloc(list->options, list->maxSize * sizeof(option_t));
		if (!list->options) option_allocation_failure();
	}

	// Do a binary search to find the index of our new option.

	size_t left = 0;
	size_t right = list->size;
	size_t i;

	while (left < right)
	{
		i = (left + right) / 2;
		if (strcmp(name, list->options[i].name) < 0)
			right = i;
		else
			left = i + 1;
	}
	memmove(&list->options[left + 1], &list->options[left], sizeof(option_t) * (list->size - left));
	memset(&list->options[left], 0, sizeof(option_t));

	list->options[left].name = strdup(name);

	if (!list->options[left].name) option_allocation_failure();

	list->size++;

	return (&list->options[left]);
}


void add_option_spin_int(option_list_t* list, const char* name, int* data, long min, long max, void(*callback)(void*))
{
	option_t* cur = insert_option(list, name);

	cur->type = OptionSpinInt;
	cur->data = data;
	cur->callback = callback;
	cur->def = malloc(sizeof(long));
	cur->min = malloc(sizeof(long));
	cur->max = malloc(sizeof(long));
	if (!cur->def || !cur->min || !cur->max) option_allocation_failure();

	*(long*)cur->def = *data;
	*(long*)cur->min = min;
	*(long*)cur->max = max;
}

void add_option_spin_flt(option_list_t* list, const char* name, double* data, double min,
	double max, void (*callback)(void*))
{
	option_t* cur = insert_option(list, name);

	cur->type = OptionSpinFlt;
	cur->data = data;
	cur->callback = callback;
	cur->def = malloc(sizeof(double));
	cur->min = malloc(sizeof(double));
	cur->max = malloc(sizeof(double));
	if (!cur->def || !cur->min || !cur->max) option_allocation_failure();

	*(double*)cur->def = *data;
	*(double*)cur->min = min;
	*(double*)cur->max = max;
}

void set_option(option_list_t* list, const char* name, const char* value)
{
	size_t left = 0;
	size_t right = list->size;
	size_t i;

	while (left < right)
	{
		i = (left + right) / 2;

		int result = strcmp(name, list->options[i].name);

		if (result < 0)
			right = i;
		else if (result > 0)
			left = i + 1;
		else
		{
			option_t* cur = &list->options[i];
			long l;
			double d;

			switch (cur->type)
			{
			case OptionSpinInt:
				sscanf(value, "%ld", &l);
				if (l >= *(long*)cur->min && l <= *(long*)cur->max) *(long*)cur->data = l;
				break;

			case OptionSpinFlt:
				sscanf(value, "%lf", &d);
				if (d >= *(double*)cur->min && d <= *(double*)cur->max)
					*(double*)cur->data = d;
				break;
			}

			if (cur->callback) cur->callback(cur->data);

			return;
		}
	}
}



void show_options( option_list_t* list)
{
	for (size_t i = 0; i < list->size; ++i)
	{
		const option_t* cur = &list->options[i];

		switch (cur->type)
		{
		case OptionSpinInt:
			printf("option name %s type spin default %ld min %ld max %ld\n", cur->name,
				*(long*)cur->def, *(long*)cur->min, *(long*)cur->max);
			break;

			// Tricky case: spins can't be floats, so we show them as strings and
			// handle them internally.

		case OptionSpinFlt:
			printf("option name %s type string default %lf\n", cur->name, *(double*)cur->def);
			break;
		}
	}
	fflush(stdout);
}
