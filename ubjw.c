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

#define CONTAINER_IS_SIZED		0x1
#define CONTAINER_IS_TYPED		0x2
#define CONTAINER_IS_ARRAY		0x4
#define CONTAINER_STACK_SIZE		32

struct ubjw_container_t
{
	uint8_t flags;
	uint8_t type;
	size_t elements_remaining;
};

struct ubjw_context_t
{
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata);
	int(*close_cb)(void* userdata);
	void (*error_cb)(const char* error_msg);
	
	void* userdata;

	uint64_t flags;

	struct ubjw_container_t container_stack[CONTAINER_STACK_SIZE];
	size_t stack_height;
//	uint64_t typestack;	     //max of 32 recursions..possibly arbitrarily increase this but I don't know.
//	size_t  stack_height;

	uint16_t last_error_code;
};



struct ubjw_context_t* ubjw_open_callback(void* userdata,
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata),
	int(*close_cb)(void* userdata),
	void (*error_cb)(const char* error_msg)
 					)
{
	struct ubjw_context_t* ctx = (struct ubjw_context_t*)malloc(sizeof(struct ubjw_context_t));
	ctx->userdata = userdata;
	ctx->write_cb = write_cb;
	ctx->close_cb = close_cb;
	ctx->error_cb = error_cb;
	
	ctx->flags = 0;
//	ctx->typestack = 0;
//	ctx->stack_height = 0;
//	ctx->max_stack_height = sizeof(uint64_t) * 8;
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


static inline _ubjw_container_stack_push(struct ubjw_context_t* ctx, const struct container_t* cnt)
{
	if(ctx->stack_height < CONTAINER_STACK_SIZE)
	{
		ctx->container_stack[ctx->stack_height++]=*cnt;
	}
	else
	{
		//todo::error
	}
}
static inline struct container_t _ubjw_container_stack_pop(struct ubjw_context_t* ctx)
{
	return ctx->container_stack[--(ctx->stack_height)];

}

static inline void _ubjw_context_append(struct ubjw_context_t* ctx, uint8_t a)
{
	ctx->write_cb(&a, 1, 1, ctx->userdata);
}
static inline void _ubjw_context_tag(struct ubjw_context_t* ctx,UBJW_TYPE tid)
{
	
}
static inline size_t _ubjw_context_write(struct ubjw_context_t* ctx, const uint8_t* data, size_t sz)
{
	return ctx->write_cb(data, 1, sz, ctx->userdata);
}

static inline void _to_bigendian16(uint8_t* outbuffer, uint16_t input)
{
	*outbuffer++ = (input >> 8); // Get top order byte (guaranteed endian-independent since machine registers)
	*outbuffer++ = input & 0xFF; // Get bottom order byte
}
static inline void _to_bigendian32(uint8_t* outbuffer, uint32_t input)
{
	_to_bigendian16(outbuffer, (uint16_t)(input >> 16)); // Get top order 2 bytes
	_to_bigendian16(outbuffer + 2, (uint16_t)input); // Get bottom order 2 bytes
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
	_ubjw_context_write(ctx, (const uint8_t*)out, n);
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
	_to_bigendian16(buf, *(uint16_t*)&out);
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
	_to_bigendian32(buf, *(uint32_t*)&out);
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
	_to_bigendian64(buf, *(uint64_t*)&out);
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
//return the array variables...then you can directly use the stack to define it.
//Cool
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
	FILE* fp=fopen("file.ubj","wb");
	struct ubjw_context_t* CTX=ubjw_open_file(fp);
	ubjw_write_string(CTX,"Hello, World");
	ubjw_write_integer(CTX,2323);
	ubjw_close_context(CTX);
	return 0;
}