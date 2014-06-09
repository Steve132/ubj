/*

#define CONTAINER_IS_SIZED		0x1
#define CONTAINER_IS_TYPED		0x2
#define CONTAINER_IS_ARRAY		0x4
#define CONTAINER_IS_OBJECT		0x8

#define CONTAINER_EXPECTS_KEY	0x10


#define CONTAINER_STACK_MAX		64
#define BUFFER_OUT_SIZE			1024

//note could possibly be both ways with just reader or writer flag for the context....at least so far..signature is the saem
struct _ubjr_container_t
{
	uint8_t flags;
	uint8_t type;
	size_t elements_remaining;
};

struct ubjr_context_t
{
	size_t(*read_cb)(void* data, size_t size, size_t count, void* userdata);
	int(*close_cb)(void* userdata);
	void(*error_cb)(const char* error_msg);

	void* userdata;

	struct _ubjr_container_t container_stack[CONTAINER_STACK_MAX];
	struct _ubjr_container_t* head;

	uint8_t ignore_container_flags;

	uint16_t last_error_code;

	size_t total_read;
};

static inline uint8_t _ubjr_context_getc(struct ubjr_context_t* ctx)
{
	uint8_t a;
	ctx->total_read += 1;
	ctx->read_cb(&a, 1, 1, ctx->userdata);
	return a;
}

void ubjw_read_string(ubjw_context_t* dst, const char* out);
void ubjw_read_char(ubjw_context_t* dst, char out);
*/

#include "ubj.h"
#include <stdlib.h>

static int _obj_key_cmp(const void* av, const char* bv)
{
	return strcmp(*(const char**)av, *(const char**)bv);
}
static __inline const char** priv_ubjr_object_build_sorted_keys(ubjr_object_t* obj)
{
	void* sorted_keysmem = malloc(obj->size*(2 * sizeof(const char*)));
	for (size_t i = 0; i < obj->size; i++)
	{
		char** location = (char**)(obj->sorted_keys + (i << 1));
		location[0] = obj->keys[i];
		location[1] = (const uint8_t*)obj->values + i*priv_ubjr_type_size(obj->type);
	}
	qsort(sorted_keysmem, obj->size, sizeof(char**) * 2, _obj_key_cmp);
}
//warning...null-terminated strings are assumed...when this is not necessarily valid.
static __inline ubjr_dynamic_t priv_ubjr_pointer_to_dynamic(UBJ_TYPE typ, const void* dat,size_t sz)
{
	ubjr_dynamic_t outdyn;
	outdyn.type = typ;
	size_t n = 1;
	switch (typ)
	{
	case UBJ_NULLTYPE:
	case UBJ_NOOP:
		break;
	case UBJ_BOOL_TRUE:
	case UBJ_BOOL_FALSE:
		outdyn.boolean = (typ == UBJ_BOOL_TRUE ? 1 : 0);
		break;
	case UBJ_HIGH_PRECISION:
		//warn here that high-precision strings are not correctly supported as ints (string)
	case UBJ_STRING:
		size_t ndetlen = strlen(dat);
		if (n != ndetlen)
		{
			n = ndetlen;
		}
	case UBJ_CHAR:
		outdyn.string = malloc(n);
		memcpy(outdyn.string, dat, n);
		break;
	case UBJ_INT8:
		outdyn.string = 
	case UBJ_UINT8:
		return 1;
	case UBJ_INT16:
		return 2;
	case UBJ_INT32:
	case UBJ_FLOAT32:
		return 4;
	case UBJ_INT64:
	case UBJ_FLOAT64:
		return 8;
	case UBJ_ARRAY:
	case UBJ_OBJECT:
	case UBJ_MIXED:
		return -1;
	};
	return outdyn;
}

ubjr_dynamic_t ubjr_object_lookup(ubjr_object_t* obj, const char* key)
{
	if (obj->sorted_keys == NULL)
	{
		//memcpy(obj->sorted_keys,obj->keys)
		obj->sorted_keys = priv_ubjr_object_build_sorted_keys(obj);
	}
	void* result=bsearch(key, obj->sorted_keys,
		obj->size, 2*sizeof(char**),
		_obj_key_cmp);
	if (result == NULL)
	{
		return NULL;
	}
	char** pair = (const char**)pair;

	return pair[1];
}


ubjr_dynamic_t ubjr_read_dynamic(ubjr_context_t* ctx)
{

}
ubjr_dynamic_t ubjr_object_lookup(ubjr_object_t* obj, const char* key);