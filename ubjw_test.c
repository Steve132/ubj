#include "ubjw.h"
#include <stdlib.h>
#include <string.h>

static const char* expected_mixed_dynamic_array = "[i\x05Si\x05helloi\x0a]";
static const size_t expected_mixed_dynamic_array_size = 14;
void test_mixed_dynamic_array(struct ubjw_context_t* ctx)
{
	ubjw_begin_array(ctx, MIXED, 0);
	ubjw_write_int8(ctx, 5);
	ubjw_write_string(ctx, "hello");
	ubjw_write_int8(ctx, 10);
	ubjw_end(ctx);
}
static const char* expected_mixed_static_array = "[#i\x03i\x05Si\x05helloi\x0a";
static const size_t expected_mixed_static_array_size = 16;
void test_mixed_static_array(struct ubjw_context_t* ctx)
{
	ubjw_begin_array(ctx, MIXED, 3);
	ubjw_write_int8(ctx, 5);
	ubjw_write_string(ctx, "hello");
	ubjw_write_int8(ctx, 10);
	ubjw_end(ctx);
}

static const char* expected_mixed_typed_static_array = "[$U#i\x03\xFF\xAA\xDD";
static const size_t expected_mixed_typed_static_array_size = 9;
void test_mixed_typed_static_array(struct ubjw_context_t* ctx)
{
	ubjw_begin_array(ctx, UINT8, 3);
	ubjw_write_uint8(ctx, 0xFF);
	ubjw_write_uint8(ctx, 0xAA);
	ubjw_write_uint8(ctx, 0xDD);
	ubjw_end(ctx);
}


/*typedef enum
{
NULLTYPE = 'Z',
NOOP = 'N',
BOOL_TRUE = 'T',
BOOL_FALSE = 'F',

STRING = 'S',
CHAR = 'C',

INT8 = 'i',
UINT8 = 'U',
INT16 = 'I',
INT32 = 'l',
INT64 = 'L',
FLOAT32 = 'd',
FLOAT64 = 'D',
HIGH_PRECISION = 'H',

ARRAY = '[',
OBJECT = '{',

MIXED = 0
*/
void printhex(const char* buffer, size_t sz)
{
	for (size_t i = 0; i < sz; i++)
	{
		printf("%X", (unsigned char)buffer[i]);
	}
}
void run_test(const char* name, void(*tp)(struct ubjw_context_t* ctx), const char* expected, size_t sz)
{
	printf("%s :", name);
	uint8_t* memory = malloc(2 * sz);
	struct ubjw_context_t* ctx = ubjw_open_memory(memory, memory + 2 * sz);
	tp(ctx);
	size_t n = ubjw_close_context(ctx);//returns total written?  maybe?
	int c = memcmp(memory, expected, sz);
	if (c == 0)
	{
		printf("PASSED\n");
	}
	else
	{
		printf("FAILED (");
		printhex(memory, n);
		printf("?=");
		printhex(expected, sz);
		printf(")\n");
	}
	free(memory);
}


int main(int argc, char** argv)
{
	run_test("mixed_dynamic_array", test_mixed_dynamic_array, expected_mixed_dynamic_array, expected_mixed_dynamic_array_size);
	run_test("mixed_static_array", test_mixed_static_array, expected_mixed_static_array, expected_mixed_static_array_size);
	run_test("mixed_typed_static_array", test_mixed_typed_static_array, expected_mixed_typed_static_array, expected_mixed_typed_static_array_size);

	return 0;
}
