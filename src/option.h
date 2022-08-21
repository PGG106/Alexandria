#pragma once
#ifndef OPTION_H
#define OPTION_H
#include <stdlib.h>
#include <string.h>

// Enum for supported option types
typedef enum option_type_e
{
	OptionSpinInt,
	OptionSpinFlt,
} option_type_t;

// Some helper macros for tuning stuff

#define TUNE_SCORE(x, minval, maxval)                                \
    do {                                                             \
        extern score_t x;                                            \
        add_option_score(&OptionList, #x, &x, minval, maxval, NULL); \
    } while (0);

#define TUNE_SP(x, minval, maxval)                                                    \
    do {                                                                              \
        extern scorepair_t x;                                                         \
        add_option_scorepair(                                                         \
            &OptionList, #x, &x, SPAIR(minval, minval), SPAIR(maxval, maxval), NULL); \
    } while (0);

#define TUNE_SP_ARRAY(x, len, start, end, minval, maxval)                                         \
    do {                                                                                          \
        extern scorepair_t x[len];                                                                \
        char __buf[128];                                                                          \
        for (int __i = start; __i < end; ++__i)                                                   \
        {                                                                                         \
            sprintf(__buf, #x "_%02d", __i);                                                      \
            add_option_scorepair(                                                                 \
                &OptionList, __buf, &x[__i], SPAIR(minval, minval), SPAIR(maxval, maxval), NULL); \
        }                                                                                         \
    } while (0);

#define TUNE_LONG(x, minval, maxval)                                    \
    do {                                                                \
        extern long x;                                                  \
        add_option_spin_int(&OptionList, #x, &x, minval, maxval, NULL); \
    } while (0);

#define TUNE_BOOL(x)                                 \
    do {                                             \
        extern bool x;                               \
        add_option_check(&OptionList, #x, &x, NULL); \
    } while (0);

#define TUNE_DOUBLE(x, minval, maxval)                                  \
    do {                                                                \
        extern double x;                                                \
        add_option_spin_flt(&OptionList, #x, &x, minval, maxval, NULL); \
    } while (0);

// Warning: int spins are always treated as long
// and flt spins are always treated as double.

// Struct for an option
typedef struct option_s
{
	char* name;
	option_type_t type;
	void* data;
	void (*callback)(void*);
	void* def;
	void* min;
	void* max;
	char** comboList;
} option_t;

// Struct for a list of options
typedef struct option_list_s
{
	option_t* options;
	size_t size;
	size_t maxSize;
} option_list_t;

// Global option list
extern option_list_t OptionList;

// Initializes the option list.
void init_option_list(option_list_t* list);

// Frees all memory associated with the option list.
void quit_option_list(option_list_t* list);

// Creates a new option of type `spin` holding a long and adds it to the option list.
void add_option_spin_int(option_list_t* list, const char* name, int* data, long min, long max,
	void (*callback)(void*));

// Displays the option list as specified by the UCI protocol.
void show_options(option_list_t* list);

// Sets the value of an option.
void set_option(option_list_t* list, const char* name, const char* value);

#endif // OPTION_H