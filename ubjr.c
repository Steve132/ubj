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


void ubjw_read_string(ubjw_context_t* dst, const char* out);
void ubjw_read_char(ubjw_context_t* dst, char out);
*/

#include "ubj.h"
#include "ubj_internal.h"
#include <stdlib.h>
#include <string.h>

#if _MSC_VER
#define inline __inline
#endif

typedef struct ubjr_context_t_s
{
	size_t (*read_cb )(void* data, size_t size, size_t count, void* userdata);
	int (*peek_cb )(void* userdata);
	int    (*close_cb)(void* userdata);
	void   (*error_cb)(const char* error_msg);

	void* userdata;

//	struct _ubjr_container_t container_stack[CONTAINER_STACK_MAX];
//	struct _ubjr_container_t* head;

	uint8_t ignore_container_flags;

	uint16_t last_error_code;

	size_t total_read;
} ubjr_context_t;

static inline uint8_t priv_ubjr_context_getc(ubjr_context_t* ctx)
{
	uint8_t a;
	ctx->total_read += 1;
	ctx->read_cb(&a, 1, 1, ctx->userdata);
	return a;
}

static int fpeek(void* fp)
{
    int c;
    c = fgetc(fp);
    ungetc(c, fp);

    return c;
}

static inline int priv_ubjr_context_peek(ubjr_context_t* ctx)
{
	return ctx->peek_cb(ctx->userdata);
}
static inline size_t priv_ubjr_context_read(ubjr_context_t* ctx,uint8_t* dst,size_t n)
{
	size_t nr=ctx->read_cb(&dst,n,1,ctx->userdata);
	ctx->total_read+=nr;
	return nr;
}

typedef struct priv_ubjr_sorted_key_t_s
{
	ubjr_string_t key;
	const uint8_t* value;

} priv_ubjr_sorted_key_t;

static int _obj_key_cmp(const void* av, const void* bv)
{
	const priv_ubjr_sorted_key_t *a, *b;
	a = (const priv_ubjr_sorted_key_t*)av;
	b = (const priv_ubjr_sorted_key_t*)bv;
	return strcmp(a->key,b->key);
}



static inline UBJ_TYPE priv_ubjr_type_from_char(uint8_t c)
{
	int i = 0;								//TODO: Benchmark this and see if it should be a switch statement where the compiler implements fastest switch e.g. binary search (17 cases might be binary search fast)
	for (i = 0; i < UBJ_NUM_TYPES && UBJI_TYPEC_convert[i] != c; i++);
	return (UBJ_TYPE)i;
}


static inline priv_ubjr_sorted_key_t* priv_ubjr_object_build_sorted_keys(ubjr_object_t* obj)
{
	priv_ubjr_sorted_key_t* sorted_keysmem = malloc(obj->size*sizeof(priv_ubjr_sorted_key_t));
	size_t i;
	for (i = 0; i < obj->size; i++)
	{
		sorted_keysmem[i].key = obj->keys[i];
		sorted_keysmem[i].value = (const uint8_t*)obj->values + i*UBJR_TYPE_localsize[obj->type];
	}
	qsort(sorted_keysmem, obj->size, sizeof(priv_ubjr_sorted_key_t), _obj_key_cmp);
	return sorted_keysmem;
}



//warning...null-terminated strings are assumed...when this is not necessarily valid. FIXED: we don't use null-terminated strings in the reader (NOT FIXED...string type is awkward)
static inline ubjr_dynamic_t priv_ubjr_pointer_to_dynamic(UBJ_TYPE typ, const void* dat)
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
	case UBJ_STRING:
	case UBJ_CHAR://possibly if char allocate, otherwise don't
		outdyn.string = *(const ubjr_string_t*)dat;
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
		outdyn = *(const ubjr_dynamic_t*)dat;
	};
	return outdyn;
}

static inline uint8_t priv_ubjr_read_1b(ubjr_context_t* ctx)
{
	return priv_ubjr_context_getc(ctx);
}
static inline uint16_t priv_ubjr_read_2b(ubjr_context_t* ctx)
{
	return (uint16_t)priv_ubjr_read_1b(ctx) << 8 | priv_ubjr_read_1b(ctx);
}
static inline uint32_t priv_ubjr_read_4b(ubjr_context_t* ctx)
{
	return (uint32_t)priv_ubjr_read_2b(ctx) << 16 | priv_ubjr_read_2b(ctx);
}
static inline uint32_t priv_ubjr_read_8b(ubjr_context_t* ctx)
{
	return (uint64_t)priv_ubjr_read_4b(ctx) << 32ULL | priv_ubjr_read_4b(ctx);
}

static inline void priv_ubjr_read_to_ptr(const ubjr_context_t* ctx, uint8_t* dst, UBJ_TYPE typ)
{
	int64_t n = 1;
	char *tstr;
	switch (typ)
	{
	case UBJ_MIXED:
	{
		*(ubjr_dynamic_t*)dst = ubjr_read_dynamic(ctx);
		break;
	}
	case UBJ_STRING:
	case UBJ_HIGH_PRECISION:
	{
		n = ubjw_read_integer(ctx);
	}
	case UBJ_CHAR:
	{
		tstr = malloc(n + 1);
		priv_ubjr_context_read(ctx, tstr, n);
		tstr[n] = 0;
		*(ubjr_string_t*)dst = tstr;
		break;
	}
	case UBJ_INT8:
	case UBJ_UINT8:
	{
		*dst = priv_ubjr_read_1b(ctx);
		break;
	}
	case UBJ_INT16:
	{
		*(uint16_t*)dst = priv_ubjr_read_2b(ctx);
		break;
	}
	case UBJ_INT32:
	case UBJ_FLOAT32:
	{
		*(uint32_t*)dst = priv_ubjr_read_4b(ctx);
		break;
	}
	case UBJ_INT64:
	case UBJ_FLOAT64:
	{
		*(uint64_t*)dst = priv_ubjr_read_8b(ctx);
		break;
	}
	case UBJ_ARRAY:
	{
		*(ubjr_array_t*)dst = priv_ubjr_read_raw_array(ctx);
		break;
	}
	case UBJ_OBJECT:
	{
		*(ubjr_array_t*)dst = priv_ubjr_read_raw_array(ctx);
		break;
	}
	};
}

ubjr_dynamic_t ubjr_object_lookup(ubjr_object_t* obj, const char* key)
{
	if (obj->sorted_keys == NULL)
	{
		//memcpy(obj->sorted_keys,obj->keys)
		obj->sorted_keys = priv_ubjr_object_build_sorted_keys(obj);
	}
	void* result=bsearch(key, obj->sorted_keys,obj->size, sizeof(priv_ubjr_sorted_key_t),_obj_key_cmp);
	if (result == NULL)
	{
		ubjr_dynamic_t nulldyn;
		nulldyn.type = UBJ_NULLTYPE;
		return nulldyn;
	}
	const priv_ubjr_sorted_key_t* result_key = (const priv_ubjr_sorted_key_t*)result;
	return priv_ubjr_pointer_to_dynamic(obj->type,result_key->value);
}




/*uint16_t ubjr_read_raw_uint16_t(ubjr_context_t* ctx)
{
	uint16_t r = (uint8_t)ubjr_read_raw_uint8_t(ctx) << 8 | (uint8_t)ubjr_read_raw_uint8_t(ctx);
	return 
}*/

ubjr_dynamic_t ubjr_read_dynamic(ubjr_context_t* ctx)
{
	ubjr_dynamic_t scratch; //scratch memory
	UBJ_TYPE newtyp = priv_ubjr_type_from_char(priv_ubjr_context_getc(ctx));
	priv_ubjr_read_to_ptr(ctx, &scratch, newtyp);
	return priv_ubjr_pointer_to_dynamic(newtyp, &scratch);
}

static inline void priv_read_container_params(ubjr_context_t* ctx, UBJ_TYPE* typout, size_t* sizeout)
{
	int nextchar = priv_ubjr_context_peek(ctx);
	if (nextchar == '$')
	{
		priv_ubjr_context_getc(ctx);
		*typout = priv_ubjr_type_from_char(priv_ubjr_context_getc(ctx));
		nextchar = priv_ubjr_context_peek(ctx);
	}
	else
	{
		*typout = UBJ_MIXED;
	}

	if (nextchar == '#')
	{
		*sizeout = ubjw_read_integer(ctx);
	}
	else
	{
		*sizeout = 0;
	}
}

//TODO: This can be reused for object

static inline ubjr_array_t priv_ubjr_read_raw_array(ubjr_context_t* ctx)
{
	ubjr_array_t myarray;
	priv_read_container_params(ctx,&myarray.type,&myarray.size);

	size_t ls = UBJR_TYPE_localsize[myarray.type];
	if (myarray.size == 0)
	{
		size_t arrpot = 0;
		myarray.values=malloc(1*ls+1); //the +1 is for memory for the 0-size elements
		for (myarray.size = 0; priv_ubjr_context_peek(ctx) != ']'; myarray.size++)
		{
			if (myarray.size >= (1ULL << arrpot))
			{
				arrpot ++;
				myarray.values = realloc(myarray.values, (1ULL << arrpot)*ls+1);
			}
			priv_read_to_ptr(ctx,(const uint8_t*)myarray.values + ls*myarray.size,myarray.type);
		}
	}
	else
	{
		size_t i;
		myarray.values = malloc(ls*myarray.size+1);
		size_t sz = UBJI_TYPE_size[myarray.type];

		if (sz >= 0 && myarray.type != UBJ_STRING && myarray.type != UBJ_HIGH_PRECISION && myarray.type != UBJ_CHAR) //constant size,fastread
		{
			priv_ubjr_context_read(ctx, myarray.values, sz*myarray.size);
			buf_endian_swap(myarray.values, sz, myarray.size); //do nothing for 0-sized buffers
		}
		else
		{
			for (i = 0; i < myarray.size; i++)
			{
				priv_ubjr_read_to_ptr(ctx, (uint8_t*)myarray.values + ls*i, myarray.type);
			}
		}
	}
	return myarray;
}

static inline ubjr_object_t priv_ubjr_read_raw_object(ubjr_context_t* ctx)
{
	ubjr_object_t myobject;
	priv_read_container_params(ctx, &myobject.type, myobject.size);

	size_t ls = UBJR_TYPE_localsize[myobject.type];
	if (myobject.size == 0)
	{
		size_t arrpot = 0;
		myobject.values = malloc(1 * ls + 1); //the +1 is for memory for the 0-size elements
		myobject.keys = malloc(1 * sizeof(ubjr_string_t));
		for (myobject.size = 0; priv_ubjr_context_peek(ctx) != '}'; myobject.size++)
		{
			if (myobject.size >= (1ULL << arrpot))
			{
				arrpot++;
				myobject.values = realloc(myobject.values, (1ULL << arrpot)*ls + 1);
				myobject.keys = realloc(myobject.keys, (1ULL << arrpot)*sizeof(ubjr_string_t));
			}
			priv_read_to_ptr(ctx, myobject.keys + myobject.size, UBJ_STRING);
			priv_read_to_ptr(ctx, (const uint8_t*)myobject.values + ls*myobject.size, myobject.type);
		}
	}
	else
	{
		size_t i;
		myobject.values = malloc(ls*myobject.size + 1);
		myobject.keys = malloc(myobject.size * sizeof(ubjr_string_t));

		for (i = 0; i < myobject.size; i++)
		{
			priv_ubjr_read_to_ptr(ctx, myobject.keys + myobject.size, UBJ_STRING);
			priv_ubjr_read_to_ptr(ctx, (uint8_t*)myobject.values + ls*i, myobject.type);
		}
	}
	return myobject;
}