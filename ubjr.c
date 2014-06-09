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
#include <string.h>

typedef struct priv_ubjr_sorted_key_t_s
{
	ubjr_string_t key;
	const uint8_t* value;

} priv_ubjr_sorted_key_t;

static int _obj_key_cmp(const void* av, const char* bv)
{
	const priv_ubjr_sorted_key_t *a, *b;
	a = (const priv_ubjr_sorted_key_t*)av;
	b = (const priv_ubjr_sorted_key_t*)bv;
	return strcmp(a->key,b->key);
}
static __inline priv_ubjr_sorted_key_t* priv_ubjr_object_build_sorted_keys(ubjr_object_t* obj)
{
	priv_ubjr_sorted_key_t* sorted_keysmem = malloc(obj->size*sizeof(priv_ubjr_sorted_key_t));
	for (size_t i = 0; i < obj->size; i++)
	{
		sorted_keysmem[i].key = obj->keys[i];
		sorted_keysmem[i].value = (const uint8_t*)obj->values + i*priv_ubjr_type_localsize(obj->type);
	}
	qsort(sorted_keysmem, obj->size, sizeof(priv_ubjr_sorted_key_t), _obj_key_cmp);
}

//warning...null-terminated strings are assumed...when this is not necessarily valid. FIXED: we don't use null-terminated strings in the reader (NOT FIXED...string type is awkward)
static __inline ubjr_dynamic_t priv_ubjr_pointer_to_dynamic(UBJ_TYPE typ, const void* dat)
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
		n = strlen(dat);
	case UBJ_CHAR:
		char* tstr = malloc(n+1);
		memcpy(tstr, dat, n);
		tstr[n] = 0; //make sure the new string is null-terminated
		outdyn.string = tstr;
		break;
	case UBJ_INT8:
		outdyn.integer = *(const int8_t*)dat;
		break;
	case UBJ_UINT8:
		outdyn.integer = *(const uint8_t*)dat;
		break;
	case UBJ_INT16:
		outdyn.integer = *(const int16_t*)dat;
		break;
	case UBJ_INT32:
		outdyn.integer = *(const int32_t*)dat;
		break;
	case UBJ_INT64:
		outdyn.integer = *(const int64_t*)dat;
		break;
	case UBJ_FLOAT32:
		outdyn.real = *(const float*)dat;
		break;
	case UBJ_FLOAT64:
		outdyn.real = *(const double*)dat;
		break;
	case UBJ_ARRAY:
		outdyn.container_array = *(const ubjr_array_t*)dat;
		break;
	case UBJ_OBJECT:
		outdyn.container_object = *(const ubjr_object_t*)dat;
		break;
	case UBJ_MIXED:
		outdyn = *(const ubjr_dynamic_t*)dat; //TODO: or this should error?
	};
	return outdyn;
}

static __inline ubjr_dynamic_t priv_ubjr_read_to_ptr(const ubjr_context_t* ctx, uint8_t* dst, UBJ_TYPE typ)
{

}

static __inline size_t priv_ubjr_type_localsize(UBJ_TYPE typ)
{
	switch (typ)
	{
	case UBJ_NULLTYPE:
	case UBJ_NOOP:
	case UBJ_BOOL_TRUE:
	case UBJ_BOOL_FALSE:
		return 0;
	case UBJ_HIGH_PRECISION:
	case UBJ_STRING:
	case UBJ_CHAR:
		return sizeof(ubjr_string_t);
	case UBJ_INT8:
		return sizeof(int8_t);
	case UBJ_UINT8:
		return sizeof(uint8_t);
	case UBJ_INT16:
		return sizeof(int16_t);
	case UBJ_INT32:
		return sizeof(int32_t);
	case UBJ_INT64:
		return sizeof(int64_t);
	case UBJ_FLOAT32:
		return sizeof(float); 
	case UBJ_FLOAT64:
		return sizeof(double);
	case UBJ_ARRAY:
		return sizeof(ubjr_array_t);
	case UBJ_OBJECT:
		return sizeof(ubjr_object_t);
	case UBJ_MIXED:
		return sizeof(ubjr_dynamic_t);
	};
}

ubjr_dynamic_t ubjr_object_lookup(ubjr_object_t* obj, const char* key)
{
	if (obj->sorted_keys == NULL)
	{
		//memcpy(obj->sorted_keys,obj->keys)
		obj->sorted_keys = priv_ubjr_object_build_sorted_keys(obj);
	}
	void* result=bsearch(key, obj->sorted_keys,
		obj->size, sizeof(priv_ubjr_sorted_key_t),
		_obj_key_cmp);
	if (result == NULL)
	{
		ubjr_dynamic_t nulldyn;
		nulldyn.type = UBJ_NULLTYPE;
		return nulldyn;
	}
	const priv_ubjr_sorted_key_t* result_key = (const priv_ubjr_sorted_key_t*)result;
	return priv_ubjr_pointer_to_dynamic(obj->type,result_key->value);
}
//TODO: This can be reused for object
static __inline ubjr_array_t priv_ubjr_read_raw_array(ubjr_context_t* ctx)
{
	ubjr_array_t myarray;
	int nextchar = priv_ubjr_context_peek(ctx);
	if (nextchar == '$')
	{
		priv_ubjr_context_getc(ctx);
		myarray.type = priv_ubjr_context_getc(ctx);
		nextchar = priv_ubjr_context_peek(ctx);
	}
	else
	{
		myarray.type = UBJ_MIXED;
	}

	if (nextchar == '#')
	{
		myarray.size = ubjw_read_integer(ctx);
	}
	else
	{
		myarray.size = 0;
	}

	size_t ls = priv_ubjr_type_localsize(myarray.type);
	if (myarray.size == 0)
	{
		size_t arrpot = 0;
		myarray.values=malloc(1*ls+1); //the +1 is for memory for the 0-size elements
		for (myarray.size = 0; priv_ubjr_context_peek(ctx) != ']'; myarray.size++)
		{
			if (myarray.size >= (1ULL << arrpot))
			{
				arrpot <<= 1;
				myarray.values = realloc(myarray.values, (1ULL << arrpot)*ls+1);
			}
			priv_read_raw_dynamic_mem(ctx,(const uint8_t*)myarray.values + ls*myarray.size,myarray.type);
		}
	}
	else
	{
		myarray.values = malloc(ls*myarray.size+1);
		//read stuff...if its typed then read raw..if not then just read dynamic..if its typed and static sized then dump buffer
		for (size_t i = 0; i < myarray.size; i++)//here is where we would just read stuff hyperfast if we could...but only if its typed
		{
			priv_read_raw_dynamic_mem(ctx, (const uint8_t*)myarray.values + ls*myarray.size, myarray.type+1);
		}
	}
	return myarray;
}

ubjr_dynamic_t ubjr_read_dynamic(ubjr_context_t* ctx)
{

}