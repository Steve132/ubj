#include "ubjw.h"
#include <stdlib.h>
#include <string.h>

/*
#if __STDC_VERSION__ <= 199901L
#define inline inline
#else
#if _MSC_VER
#define inline __inline
#else
#define inline __inline__
#endif
#endif*/

#if _MSC_VER
#define inline __inline
#endif

struct ubjw_context_t
{
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata);
	int(*close_cb)(void* userdata);
	void (*error_cb)(const char* error_msg);
	
	void* userdata;

	uint64_t flags;

	uint64_t typestack;	     //max of 32 recursions..possibly arbitrarily increase this but I don't know.
	size_t  stack_height;
	size_t  max_stack_height;

	uint16_t last_error_code;
};


struct ubjw_context_t* ubjw_open_callback(void* userdata,
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata),
	int(*close_cb)(void* userdata))
{
	struct ubjw_context_t* ctx = (struct ubjw_context_t*)malloc(sizeof(struct ubjw_context_t));
	ctx->userdata = userdata;
	ctx->write_cb = write_cb;
	ctx->close_cb = close_cb;
	ctx->error_cb = error_cb;
	
	ctx->flags = 0;
	ctx->typestack = 0;
	ctx->stack_height = 0;
	ctx->max_stack_height = sizeof(uint64_t) * 8;
	ctx->last_error_code = 0;
	return ctx;
}
struct ubjw_context_t* ubjw_open_file(FILE* fd)
{
	return ubjw_open_callback(fd, fwrite,fclose,NULL);
}

static struct mem_w_fd
{
	uint8_t* current, *end;
};
static int memclose(void* mfd)
{
	free(mfd);
	return 0;
}
static size_t memwrite(const void* data, size_t size, size_t count, struct mem_w_fd* fp)
{
	size_t n = size*count;
	size_t lim = fp->end - fp->current;
	if (lim > n)
	{
		n = lim;
	}
	memcpy(fp->current, data, n);
	fp->current += n;
	return n;
}

struct ubjw_context_t* ubjw_open_memory(uint8_t* be, uint8_t* en)
{
	struct mem_w_fd* mfd = (struct mem_w_fd*)malloc(sizeof(struct mem_w_fd));
	mfd->current = be;
	mfd->end = en;
	return ubjw_open_callback(mfd, memwrite, memclose,NULL);
}



void ubjw_close_context(struct ubjw_context_t* ctx)
{
	if(ctx->close_cb)
		ctx->close_cb(ctx->userdata);
	free(ctx);
}


typedef enum
{
	C_ARRAY = 0,
	C_OBJECT = 1,
	SIZED_C_ARRAY = 2,
	SIZED_C_OBJECT = 3,
} CONTAINER_STACK_ID;

static inline _ubjw_container_stack_push(struct ubjw_context_t* ctx, CONTAINER_STACK_ID cid)
{
	ctx->typestack <<= 2;
	ctx->typestack |= (unsigned int)cid;
	ctx->stack_height++;
}
static inline CONTAINER_STACK_ID _ubjw_container_stack_pop(struct ubjw_context_t* ctx)
{
	CONTAINER_STACK_ID cid = (CONTAINER_STACK_ID)(ctx->typestack & 0x3);
	ctx->stack_height--;
	ctx->typestack >>= 2;	//TODO:use max_height_here
	return cid;

}

static inline void _ubjw_context_append(struct ubjw_context_t* ctx, uint8_t a)
{
	ctx->write_cb(&a, 1, 1, ctx->userdata);
}
static inline size_t _ubjw_context_write(struct ubjw_context_t* ctx, const uint8_t* data, size_t sz)
{
	return ctx->write_cb(data, 1, sz, ctx->userdata);
}
//*this code is so clever.  Too bad its slow :(
/*
static void inline _to_bigendian(uint8_t* outbuffer, uint64_t input, uint8_t num_bytes)
{
#if 0 //implementation 1
	uint8_t* op = outbuffer + num_bytes - 1;//initialize it to the last element
	for (uint8_t shiftamount = 0; shiftamount < 64; shiftamount += 8)
	{
		*op-- = (input >> shiftamount) & 0xFF;
	}
#else
	switch (num_bytes)
	{
	case 8:
		*outbuffer++ = (input >> 56ULL) & 0xFF;
		*outbuffer++ = (input >> 48ULL) & 0xFF;
		*outbuffer++ = (input >> 40ULL) & 0xFF;
		*outbuffer++ = (input >> 32ULL) & 0xFF;
	case 4:
		*outbuffer++ = (input >> 24ULL) & 0xFF;
		*outbuffer++ = (input >> 16ULL) & 0xFF;
	case 2:
		*outbuffer++ = (input >> 8ULL) & 0xFF;
	case 1:
		*outbuffer++ = (input >> 0ULL) & 0xFF;
	default:
		break;
	};
#endif

}*/

static inline void _to_bigendian16(uint8_t* outbuffer, uint16_t input)
{
	*outbuffer++ = (input >> 8) * 0xFF;
	*outbuffer++ = (input >> 0) * 0xFF;
}
static inline void _to_bigendian32(uint8_t* outbuffer, uint32_t input)
{
	_to_bigendian16(outbuffer, (uint16_t)(input >> 16));
	_to_bigendian16(outbuffer+2, (uint16_t)input);
}
static inline void _to_bigendian64(uint8_t* outbuffer, uint64_t input)
{
	_to_bigendian32(outbuffer, (uint32_t)(input >> 32));
	_to_bigendian32(outbuffer + 4, (uint32_t)input);
}

inline void _ubjw_write_raw_string(struct ubjw_context_t* ctx, const char* out)//TODO: possibly use a safe string
{
	size_t n = strlen(out);
	ubjw_write_integer(ctx, (int64_t)n);
	_ubjw_context_write(ctx, out, n);
}
void ubjw_write_string(struct ubjw_context_t* ctx, const char* out)
{
	_ubjw_context_append(ctx, (uint8_t)STRING);
	_ubjw_write_raw_string(ctx, out);
}

inline void _ubjw_write_raw_char(struct ubjw_context_t* ctx, char out)
{
	_ubjw_context_append(ctx, (uint8_t)out);
}
void ubjw_write_char(struct ubjw_context_t* ctx, char out)
{
	_ubjw_context_append(ctx, (uint8_t)CHAR);
	_ubjw_write_raw_char(ctx, out);
}
/*
inline void _ubjw_write_raw_integer_dyn(uint8_t sz, struct ubjw_context_t* ctx, int64_t out)
{
	uint8_t intws[8];
	_to_bigendian(intws, *(uint64_t*)&out, sz);
	_ubjw_context_write(ctx, intws, sz);
}

static inline void _ubjw_write_integer_dyn(UBJW_TYPE inttype, uint8_t sz, struct ubjw_context_t* ctx, int64_t out)
{
	_ubjw_context_append(ctx, (uint8_t)inttype);
	_ubjw_write_raw_integer_dyn(sz, ctx, out);
}*/

inline void _ubjw_write_raw_uint8(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_context_append(ctx, out);
}
void ubjw_write_uint8(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_context_append(ctx, (uint8_t)UINT8);
	_ubjw_write_raw_uint8(ctx, out);
}

inline void _ubjw_write_raw_int8(struct ubjw_context_t* ctx, int8_t out)
{
	_ubjw_context_append(ctx, *(uint8_t*)&out);
}
void ubjw_write_int8(struct ubjw_context_t* ctx, int8_t out)
{
	_ubjw_context_append(ctx, (uint8_t)INT8);
	_ubjw_write_raw_int8(ctx, out);
}

inline void _ubjw_write_raw_int16(struct ubjw_context_t* ctx, int16_t out)
{
	uint8_t buf[2];
	_to_bigendian16(buf, *(uint16_t*)out);
	_ubjw_context_write(ctx, buf, 2);
}
void ubjw_write_int16(struct ubjw_context_t* ctx, int16_t out)
{
	_ubjw_context_append(ctx, (uint8_t)INT16);
	_ubjw_write_raw_int16(ctx, out);
}
inline void _ubjw_write_raw_int32(struct ubjw_context_t* ctx, int32_t out)
{
	uint8_t buf[4];
	_to_bigendian32(buf, *(uint32_t*)out);
	_ubjw_context_write(ctx, buf, 4);
}
void ubjw_write_int32(struct ubjw_context_t* ctx, int32_t out)
{
	_ubjw_context_append(ctx, (uint8_t)INT32);
	_ubjw_write_raw_int32(ctx, out);
}
inline void _ubjw_write_raw_int64(struct ubjw_context_t* ctx, int64_t out)
{
	uint8_t buf[8];
	_to_bigendian64(buf, *(uint64_t*)out);
	_ubjw_context_write(ctx, buf, 8);
}
void ubjw_write_int64(struct ubjw_context_t* ctx, int64_t out)
{
	_ubjw_context_append(ctx, (uint8_t)INT64);
	_ubjw_write_raw_int64(ctx, out);
}

void ubjw_write_high_precision(struct ubjw_context_t* ctx, const char* hp)
{
	_ubjw_context_append(ctx, (uint8_t)HIGH_PRECISION);
	_ubjw_write_raw_string(ctx, hp);
}

void ubjw_write_integer(struct ubjw_context_t* ctx, int64_t out)
{
	uint64_t mc = llabs(out);
	if (mc < 0x80)
	{
		ubjw_write_int8(ctx, (int8_t)out);
	}
	else if (out > 0 && mc < 0x100)
	{
		ubjw_write_uint8(ctx, (uint8_t)out);
	}
	else if (mc < 0x8000)
	{
		ubjw_write_int16(ctx, (int16_t)out);
	}
	else if (mc < 0x80000000)
	{
		ubjw_write_int32(ctx, (int32_t)out);
	}
	else
	{
		ubjw_write_int64(ctx, out);
	}
}

inline void _ubjw_write_raw_float32(struct ubjw_context_t* ctx, float out)
{
	uint32_t fout = *(uint32_t*)&out;
	uint8_t outbuf[4];
	_to_bigendian32(outbuf,fout);
	_ubjw_context_write(ctx, outbuf, 4);
}
void ubjw_write_float32(struct ubjw_context_t* ctx, float out)
{
	_ubjw_context_append(ctx, (uint8_t)FLOAT32);
	_ubjw_write_raw_float32(ctx, out);
}
inline void _ubjw_write_raw_float64(struct ubjw_context_t* ctx, double out)
{
	uint64_t fout = *(uint64_t*)&out;
	uint8_t outbuf[8];
	_to_bigendian64(outbuf, fout);
	_ubjw_context_write(ctx, outbuf, 8);
}
void ubjw_write_float64(struct ubjw_context_t* ctx, double out)
{
	_ubjw_context_append(ctx, (uint8_t)FLOAT64);
	_ubjw_write_raw_float64(ctx, out);
}

void ubjw_write_floating_point(struct ubjw_context_t* ctx, double out);

void ubjw_write_noop(struct ubjw_context_t* ctx)
{
	_ubjw_context_append(ctx, (uint8_t)NOOP);
}
void ubjw_write_null(struct ubjw_context_t* ctx)
{
	_ubjw_context_append(ctx, (uint8_t)NULLTYPE);
}
void ubjw_write_bool(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_context_append(ctx, (uint8_t)(out ? BOOL_TRUE : BOOL_FALSE));
}

void ubjw_begin_array(struct ubjw_context_t* ctx, UBJW_TYPE type, size_t count)
{
	_ubjw_context_append(ctx, (uint8_t)ARRAY);
	if (type != MIXED)
	{
		_ubjw_context_append(ctx, '$');
		_ubjw_context_append(ctx, (uint8_t)type);
		if (count == 0)
		{
			//error
		}
	}
	if (count != 0)
	{
		_ubjw_context_append(ctx, '#');
		ubjw_write_integer(ctx, (int64_t)count);
		_ubjw_container_stack_push(ctx, SIZED_C_ARRAY);
	}
}
void ubjw_end_array(struct ubjw_context_t* ctx);

void ubjw_begin_object(struct ubjw_context_t* ctx, UBJW_TYPE type, size_t count);
void ubjw_write_key(struct ubjw_context_t* ctx, const char* key);
void ubjw_end_object(struct ubjw_context_t* ctx);

int main(int argc, char** argv)
{
	return 0;
}