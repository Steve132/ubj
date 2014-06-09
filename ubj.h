#ifndef UBJ_H
#define UBJ_H

#ifdef __cplusplus
extern "C" {
#endif

#include<inttypes.h>
#include<stdio.h>

typedef enum
{
	UBJ_NULLTYPE = 'Z',
	UBJ_NOOP = 'N',
	UBJ_BOOL_TRUE = 'T',
	UBJ_BOOL_FALSE = 'F',

	UBJ_STRING = 'S',
	UBJ_CHAR = 'C',

	UBJ_INT8 = 'i',
	UBJ_UINT8 = 'U',
	UBJ_INT16 = 'I',
	UBJ_INT32 = 'l',
	UBJ_INT64 = 'L',
	UBJ_FLOAT32 = 'd',
	UBJ_FLOAT64 = 'D',
	UBJ_HIGH_PRECISION = 'H',

	UBJ_ARRAY = '[',
	UBJ_OBJECT = '{',

	UBJ_MIXED = 0     //This should not be used to be written to, only to control arrays
} UBJ_TYPE;

struct ubjw_context_t_s;
typedef struct ubjw_context_t_s ubjw_context_t;

ubjw_context_t* ubjw_open_callback(void* userdata,
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata),
	int(*close_cb)(void* userdata),
	void(*error_cb)(const char* error_msg)
	);
ubjw_context_t* ubjw_open_file(FILE*);
ubjw_context_t* ubjw_open_memory(uint8_t* dst_b, uint8_t* dst_e);

size_t ubjw_close_context(ubjw_context_t* ctx);

void ubjw_write_string(ubjw_context_t* dst, const char* out);
void ubjw_write_char(ubjw_context_t* dst, char out);

void ubjw_write_uint8(ubjw_context_t* dst, uint8_t out);
void ubjw_write_int8(ubjw_context_t* dst, int8_t out);
void ubjw_write_int16(ubjw_context_t* dst, int16_t out);
void ubjw_write_int32(ubjw_context_t* dst, int32_t out);
void ubjw_write_int64(ubjw_context_t* dst, int64_t out);
void ubjw_write_high_precision(ubjw_context_t* dst, const char* hp);

void ubjw_write_integer(ubjw_context_t* dst, int64_t out);

void ubjw_write_float32(ubjw_context_t* dst, float out);
void ubjw_write_float64(ubjw_context_t* dst, double out);

void ubjw_write_floating_point(ubjw_context_t* dst, double out);

void ubjw_write_noop(ubjw_context_t* dst);
void ubjw_write_null(ubjw_context_t* dst);
void ubjw_write_bool(ubjw_context_t* dst, uint8_t out);

void ubjw_begin_array(ubjw_context_t* dst, UBJ_TYPE type, size_t count);
void ubjw_begin_object(ubjw_context_t* dst, UBJ_TYPE type, size_t count);
void ubjw_write_key(ubjw_context_t* dst, const char* key);
void ubjw_end(ubjw_context_t* dst);

//output an efficient buffer of types
void ubjw_write_buffer(ubjw_context_t* dst, const uint8_t* data, UBJ_TYPE type, size_t count);


//////////here is the declarations for the reader API////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////

struct ubjr_context_t_s;//this should JUST be container. open array makes a copy of the current container...container can be default NULL which has special rules...no dont' do that
typedef struct ubjr_context_t_s ubjr_context_t;

struct ubjr_context_t* ubjr_open_callback(void* userdata,
	size_t(*read_cb)(void* data, size_t size, size_t count, void* userdata),
	int(*peek_cb)(void* userdata),
	int(*close_cb)(void* userdata),
	void(*error_cb)(const char* error_msg)
	);
struct ubjr_context_t* ubjr_open_file(FILE*);
struct ubjr_context_t* ubjr_open_memory(uint8_t* dst_b, uint8_t* dst_e);

size_t ubjr_close_context(struct ubjr_context_t* ctx);

/*
typedef struct ubjr_string_t_s
{
	const char* data;
	size_t length;
} ubjr_string_t;  //HAVE to do this because of the way strings are defined (unicode is valid so null-termination isn't)*/

typedef const char* ubjr_string_t;

typedef struct ubjr_array_t_s
{
	UBJ_TYPE type;
	size_t size;
	void* values;
} ubjr_array_t;

typedef struct ubjr_object_t_s
{
	UBJ_TYPE type;
	size_t size;
	void* values;
	const ubjr_string_t* keys;
	const void* sorted_keys;
} ubjr_object_t;

typedef struct ubjr_dynamic_t_s
{
	UBJ_TYPE type;
	union
	{
		uint8_t boolean;
		double real;
		int64_t integer;
		ubjr_string_t string;
		ubjr_array_t container_array;
		ubjr_object_t container_object;
	};
} ubjr_dynamic_t;

ubjr_dynamic_t ubjr_parse(ubjr_context_t* ctx);

ubjr_dynamic_t ubjr_object_lookup(ubjr_object_t* obj, const char* key);

//output an efficient buffer of types
///void ubjr_read_buffer(struct ubjr_context_t* dst, const uint8_t* data, UBJ_TYPE type, size_t count);





///////UBJ_RW

ubjrw_write_dynamic_t(ubjw_context_t* ctx, const ubjr_dynamic_t dobj);
//ubjrw_append_object(ubjw_context_t* ctx, const ubjr_dynamic_t dobj);
//ubjrw_append_array(ubjw_context_t* ctx, const ubjr_dynamic_t dobj);

#ifdef __cplusplus
}
#endif

#endif
