#include "ubjw.h"
#include <stdlib.h>
#include <string.h>

#if _MSC_VER
#define inline __inline
#endif

#define CONTAINER_IS_SIZED		0x1
#define CONTAINER_IS_TYPED		0x2
#define CONTAINER_IS_ARRAY		0x4
#define CONTAINER_IS_OBJECT		0x8

#define CONTAINER_EXPECTS_KEY	0x10


#define CONTAINER_STACK_MAX		64
#define BUFFER_OUT_SIZE			1024


struct _ubjw_container_t
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

	struct _ubjw_container_t container_stack[CONTAINER_STACK_MAX];
	struct _ubjw_container_t* head;

	uint8_t ignore_container_flags;

	uint16_t last_error_code;

	size_t total_written;
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
	
	ctx->head = ctx->container_stack;
	ctx->head->flags = 0;
	ctx->head->type = MIXED;
	ctx->head->elements_remaining = 0;

	ctx->ignore_container_flags = 0;

	ctx->last_error_code = 0;

	ctx->total_written = 0;
	return ctx;
}
struct ubjw_context_t* ubjw_open_file(FILE* fd)
{
	return ubjw_open_callback(fd, fwrite,fclose,NULL);
}

static struct mem_w_fd
{
	uint8_t *begin,*current, *end;
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
	if (lim < n)
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
	mfd->begin = be;
	mfd->end = en;
	return ubjw_open_callback(mfd, memwrite, memclose,NULL);
}

static inline void _ubjw_context_append(struct ubjw_context_t* ctx, uint8_t a)
{
	ctx->total_written += 1;
	ctx->write_cb(&a, 1, 1, ctx->userdata);
}

static inline void _ubjw_context_finish_container(struct ubjw_context_t* ctx, struct _ubjw_container_t* head)
{
	if (head->flags & CONTAINER_IS_SIZED)
	{
		if (head->elements_remaining > 0)
		{
			//error not all elements written
		}
	}
	else if (head->flags & CONTAINER_IS_ARRAY)
	{
		_ubjw_context_append(ctx, (uint8_t)']');
	}
	else if (head->flags & CONTAINER_IS_OBJECT)
	{
		_ubjw_context_append(ctx, (uint8_t)'}');
	}
}

static inline _ubjw_container_stack_push(struct ubjw_context_t* ctx, const struct _ubjw_container_t* cnt)
{
	size_t height = ctx->head-ctx->container_stack+1;
	if(height < CONTAINER_STACK_MAX)
	{
		*(++(ctx->head))=*cnt;
	}
	else
	{
		//todo::error
	}
}
static inline struct _ubjw_container_t _ubjw_container_stack_pop(struct ubjw_context_t* ctx)
{
	return *ctx->head--;
}

size_t ubjw_close_context(struct ubjw_context_t* ctx)
{
	while (ctx->head > ctx->container_stack)
	{
		struct _ubjw_container_t cnt = _ubjw_container_stack_pop(ctx);
		_ubjw_context_finish_container(ctx, &cnt);
	};
	size_t n = ctx->total_written;
	if (ctx->close_cb)
		ctx->close_cb(ctx->userdata);
	free(ctx);
	return n;
}


static inline size_t _ubjw_context_write(struct ubjw_context_t* ctx, const uint8_t* data, size_t sz)
{
	ctx->total_written += sz;
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
	_to_bigendian16(outbuffer + 2, (uint16_t)(input & 0xFFFF)); // Get bottom order 2 bytes
}
static inline void _to_bigendian64(uint8_t* outbuffer, uint64_t input)
{
	_to_bigendian32(outbuffer, (uint32_t)(input >> 32));
	_to_bigendian32(outbuffer + 4, (uint32_t)(input & 0xFFFFFFFF));
}

static inline void _ubjw_tag_public(struct ubjw_context_t* ctx, UBJW_TYPE tid)
{
	struct _ubjw_container_t* ch = ctx->head;
	if (!ctx->ignore_container_flags)
	{
		/*if (
			(!(ch->flags & (CONTAINER_IS_ARRAY | CONTAINER_IS_OBJECT))) &&
			(tid != ARRAY && tid !=OBJECT))
		{
			//error, only array and object can be first written
		}*/
		if (ch->flags & CONTAINER_EXPECTS_KEY)
		{
			//error,a key expected
			return;
		}
		if (ch->flags & CONTAINER_IS_SIZED)
		{
			ch->elements_remaining--; //todo: error if elements remaining is 0;
		}
		if (ch->flags & CONTAINER_IS_OBJECT)
		{
			ch->flags |= CONTAINER_EXPECTS_KEY; //set key expected next time in this context
		}
		if ((ch->flags & CONTAINER_IS_TYPED) && ch->type == tid)
		{
			return;
		}
	}
	_ubjw_context_append(ctx, (uint8_t)tid);
}

static inline void _ubjw_write_raw_string(struct ubjw_context_t* ctx, const char* out)//TODO: possibly use a safe string
{
	size_t n = strlen(out);
	ctx->ignore_container_flags = 1; 
	ubjw_write_integer(ctx, (int64_t)n);
	ctx->ignore_container_flags = 0;
	_ubjw_context_write(ctx, (const uint8_t*)out, n);
}
void ubjw_write_string(struct ubjw_context_t* ctx, const char* out)
{
	_ubjw_tag_public(ctx,STRING);
	_ubjw_write_raw_string(ctx, out);
}

static inline void _ubjw_write_raw_char(struct ubjw_context_t* ctx, char out)
{
	_ubjw_context_append(ctx, (uint8_t)out);
}
void ubjw_write_char(struct ubjw_context_t* ctx, char out)
{
	_ubjw_tag_public(ctx,CHAR);
	_ubjw_write_raw_char(ctx, out);
}

static inline void _ubjw_write_raw_uint8(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_context_append(ctx, out);
}
void ubjw_write_uint8(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_tag_public(ctx,UINT8);
	_ubjw_write_raw_uint8(ctx, out);
}

static inline void _ubjw_write_raw_int8(struct ubjw_context_t* ctx, int8_t out)
{
	_ubjw_context_append(ctx, *(uint8_t*)&out);
}
void ubjw_write_int8(struct ubjw_context_t* ctx, int8_t out)
{
	_ubjw_tag_public(ctx,INT8);
	_ubjw_write_raw_int8(ctx, out);
}

static inline void _ubjw_write_raw_int16(struct ubjw_context_t* ctx, int16_t out)
{
	uint8_t buf[2];
	_to_bigendian16(buf, *(uint16_t*)&out);
	_ubjw_context_write(ctx, buf, 2);
}
void ubjw_write_int16(struct ubjw_context_t* ctx, int16_t out)
{
	_ubjw_tag_public(ctx,INT16);
	_ubjw_write_raw_int16(ctx, out);
}
static inline void _ubjw_write_raw_int32(struct ubjw_context_t* ctx, int32_t out)
{
	uint8_t buf[4];
	_to_bigendian32(buf, *(uint32_t*)&out);
	_ubjw_context_write(ctx, buf, 4);
}
void ubjw_write_int32(struct ubjw_context_t* ctx, int32_t out)
{
	_ubjw_tag_public(ctx,INT32);
	_ubjw_write_raw_int32(ctx, out);
}
static inline void _ubjw_write_raw_int64(struct ubjw_context_t* ctx, int64_t out)
{
	uint8_t buf[8];
	_to_bigendian64(buf, *(uint64_t*)&out);
	_ubjw_context_write(ctx, buf, 8);
}
void ubjw_write_int64(struct ubjw_context_t* ctx, int64_t out)
{
	_ubjw_tag_public(ctx,INT64);
	_ubjw_write_raw_int64(ctx, out);
}

void ubjw_write_high_precision(struct ubjw_context_t* ctx, const char* hp)
{
	_ubjw_tag_public(ctx,HIGH_PRECISION);
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

static inline void _ubjw_write_raw_float32(struct ubjw_context_t* ctx, float out)
{
	uint32_t fout = *(uint32_t*)&out;
	uint8_t outbuf[4];
	_to_bigendian32(outbuf,fout);
	_ubjw_context_write(ctx, outbuf, 4);
}
void ubjw_write_float32(struct ubjw_context_t* ctx, float out)
{
	_ubjw_tag_public(ctx,FLOAT32);
	_ubjw_write_raw_float32(ctx, out);
}
static inline void _ubjw_write_raw_float64(struct ubjw_context_t* ctx, double out)
{
	uint64_t fout = *(uint64_t*)&out;
	uint8_t outbuf[8];
	_to_bigendian64(outbuf, fout);
	_ubjw_context_write(ctx, outbuf, 8);
}
void ubjw_write_float64(struct ubjw_context_t* ctx, double out)
{
	_ubjw_tag_public(ctx,FLOAT64);
	_ubjw_write_raw_float64(ctx, out);
}

void ubjw_write_floating_point(struct ubjw_context_t* ctx, double out);

void ubjw_write_noop(struct ubjw_context_t* ctx)
{
	_ubjw_tag_public(ctx,NOOP);
}
void ubjw_write_null(struct ubjw_context_t* ctx)
{
	_ubjw_tag_public(ctx,NULLTYPE);
}
void ubjw_write_bool(struct ubjw_context_t* ctx, uint8_t out)
{
	_ubjw_tag_public(ctx,(out ? BOOL_TRUE : BOOL_FALSE));
}

void _ubjw_begin_container(struct _ubjw_container_t* cnt, struct ubjw_context_t* ctx, UBJW_TYPE typ, size_t count)
{
	cnt->flags=0;
	cnt->elements_remaining = count;
	cnt->type = typ;

	if (typ != MIXED)
	{
		if (count == 0)
		{
			//error and return;
		}
		_ubjw_context_append(ctx, '$');
		_ubjw_context_append(ctx, (uint8_t)typ);
		cnt->flags |= CONTAINER_IS_TYPED;
	}
	if (count != 0)
	{
		_ubjw_context_append(ctx, '#');
		ctx->ignore_container_flags = 1;
		ubjw_write_integer(ctx, (int64_t)count);
		ctx->ignore_container_flags = 0;
		
		cnt->flags |= CONTAINER_IS_SIZED;
		cnt->elements_remaining = count;
	}
}
void ubjw_begin_array(struct ubjw_context_t* ctx, UBJW_TYPE type, size_t count)
{
	_ubjw_tag_public(ctx, ARRAY); //todo: should this happen before any erro potential?
	struct _ubjw_container_t ch;
	_ubjw_begin_container(&ch, ctx, type, count);
	ch.flags |= CONTAINER_IS_ARRAY;
	_ubjw_container_stack_push(ctx, &ch);
}

void ubjw_begin_object(struct ubjw_context_t* ctx, UBJW_TYPE type, size_t count)
{
	_ubjw_tag_public(ctx, OBJECT);
	struct _ubjw_container_t ch;
	_ubjw_begin_container(&ch, ctx, type, count);
	ch.flags |= CONTAINER_EXPECTS_KEY | CONTAINER_IS_OBJECT;
	_ubjw_container_stack_push(ctx, &ch);
}
void ubjw_write_key(struct ubjw_context_t* ctx, const char* key)
{
	if (ctx->head->flags & CONTAINER_EXPECTS_KEY && ctx->head->flags & CONTAINER_IS_OBJECT)
	{
		_ubjw_write_raw_string(ctx, key);
		ctx->head->flags ^= CONTAINER_EXPECTS_KEY; //turn off container 
	}
	else
	{
		//error unexpected key
	}
}
void ubjw_end(struct ubjw_context_t* ctx)
{
	struct _ubjw_container_t ch = _ubjw_container_stack_pop(ctx);
	if ((ch.flags & CONTAINER_IS_OBJECT) && !(ch.flags & CONTAINER_EXPECTS_KEY))
	{
		//error expected value
	}
	_ubjw_context_finish_container(ctx, &ch);
}

static inline int _ubjw_type_size(UBJW_TYPE type)
{
	switch (type)
	{
	case NULLTYPE:
	case NOOP:
	case BOOL_TRUE:
	case BOOL_FALSE:
		return 0;
	case STRING:
	case HIGH_PRECISION:
		return sizeof(const char*);
	case CHAR:
	case INT8:
	case UINT8:
		return 1;
	case INT16:
		return 2;
	case INT32:
	case FLOAT32:
		return 4;
	case INT64:
	case FLOAT64:
		return 8;
	case ARRAY:
	case OBJECT:
	case MIXED:
		return -1;
	};
	return -1;
}

static inline uint8_t _is_bigendian()
{
	int i = 1;
	char *low = (char*)&i;
	return *low ? 0 : 1;
}

#define BUF_BIGENDIAN_LOOP(type,func)						\
					const type* datamod=(const type*)data; \
					for (size_t i = 0; i < count / bufelems; i++) \
										{ \
						for (size_t j = 0; j < bufelems; j++) \
												{ \
							size_t d = i*bufelems + j; \
							func(buf + d*sizeof(type), datamod[d]); \
												} \
						_ubjw_context_write(ctx,buf,BUFFER_OUT_SIZE);  \
					}\

static inline void _ubjw_write_byteswap(struct ubjw_context_t* ctx, const uint8_t* data, int sz, size_t count)
{
	uint8_t buf[BUFFER_OUT_SIZE];
	size_t bufelems = BUFFER_OUT_SIZE / sz;
	//TODO: speed this up with an auxilary buffer
	switch (sz)
	{
	case 2:
	{
		BUF_BIGENDIAN_LOOP(uint16_t, _to_bigendian16);
		break;
	}
	case 4:
	{
		BUF_BIGENDIAN_LOOP(uint32_t, _to_bigendian32);
		break;
	}
	case 8:
	{
		BUF_BIGENDIAN_LOOP(uint64_t, _to_bigendian64);
		break;
	}
	};

}
void ubjw_write_buffer(struct ubjw_context_t* ctx, const uint8_t* data, UBJW_TYPE type, size_t count)
{
	int typesz = _ubjw_type_size(type);
	if (typesz < 0)
	{
		//error cannot read from a buffer of this type.
	}
	ubjw_begin_array(ctx, type, count);
	if (type == STRING || type == HIGH_PRECISION)
	{
		const char** databufs = (const char**)data;
		for (size_t i = 0; i < count; i++)
		{
			_ubjw_write_raw_string(ctx, databufs[i]);
		}
	}
	else if (typesz == 1 || _is_bigendian())
	{
		size_t n = typesz*count;
		_ubjw_context_write(ctx, data, typesz*count);
	}
	else if (typesz > 1) //and not big-endian
	{
		_ubjw_write_byteswap(ctx, data,typesz,count);
	}
	ubjw_end(ctx);
}