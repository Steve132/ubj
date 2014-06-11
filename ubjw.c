#include "ubj.h"
#include "ubj_internal.h"

#define CONTAINER_IS_SIZED		0x1
#define CONTAINER_IS_TYPED		0x2
#define CONTAINER_IS_UBJ_ARRAY		0x4
#define CONTAINER_IS_UBJ_OBJECT		0x8

#define CONTAINER_EXPECTS_KEY	0x10

#define CONTAINER_STACK_MAX		64
#define BUFFER_OUT_SIZE			1024


struct priv_ubjw_container_t
{
	uint8_t flags;
	UBJ_TYPE type;
	size_t elements_remaining;
};

struct ubjw_context_t_s
{
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata);
	int(*close_cb)(void* userdata);
	void (*error_cb)(const char* error_msg);
	
	void* userdata;

	struct priv_ubjw_container_t container_stack[CONTAINER_STACK_MAX];
	struct priv_ubjw_container_t* head;

	uint8_t ignore_container_flags;

	uint16_t last_error_code;

	size_t total_written;
};



ubjw_context_t* ubjw_open_callback(void* userdata,
	size_t(*write_cb)(const void* data, size_t size, size_t count, void* userdata),
	int(*close_cb)(void* userdata),
	void (*error_cb)(const char* error_msg)
 					)
{
	ubjw_context_t* ctx = (ubjw_context_t*)malloc(sizeof(ubjw_context_t));
	ctx->userdata = userdata;
	ctx->write_cb = write_cb;
	ctx->close_cb = close_cb;
	ctx->error_cb = error_cb;
	
	ctx->head = ctx->container_stack;
	ctx->head->flags = 0;
	ctx->head->type = UBJ_MIXED;
	ctx->head->elements_remaining = 0;

	ctx->ignore_container_flags = 0;

	ctx->last_error_code = 0;

	ctx->total_written = 0;
	return ctx;
}
ubjw_context_t* ubjw_open_file(FILE* fd)
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

ubjw_context_t* ubjw_open_memory(uint8_t* be, uint8_t* en)
{
	struct mem_w_fd* mfd = (struct mem_w_fd*)malloc(sizeof(struct mem_w_fd));
	mfd->current = be;
	mfd->begin = be;
	mfd->end = en;
	return ubjw_open_callback(mfd, memwrite, memclose,NULL);
}

static inline void priv_ubjw_context_append(ubjw_context_t* ctx, uint8_t a)
{
	ctx->total_written += 1;
	ctx->write_cb(&a, 1, 1, ctx->userdata);
}

static inline void priv_ubjw_context_finish_container(ubjw_context_t* ctx, struct priv_ubjw_container_t* head)
{
	if (head->flags & CONTAINER_IS_SIZED)
	{
		if (head->elements_remaining > 0)
		{
			//error not all elements written
		}
	}
	else if (head->flags & CONTAINER_IS_UBJ_ARRAY)
	{
		priv_ubjw_context_append(ctx, (uint8_t)']');
	}
	else if (head->flags & CONTAINER_IS_UBJ_OBJECT)
	{
		priv_ubjw_context_append(ctx, (uint8_t)'}');
	}
}

static inline priv_ubjw_container_stack_push(ubjw_context_t* ctx, const struct priv_ubjw_container_t* cnt)
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
static inline struct priv_ubjw_container_t priv_ubjw_container_stack_pop(ubjw_context_t* ctx)
{
	return *ctx->head--;
}

size_t ubjw_close_context(ubjw_context_t* ctx)
{
	while (ctx->head > ctx->container_stack)
	{
		struct priv_ubjw_container_t cnt = priv_ubjw_container_stack_pop(ctx);
		priv_ubjw_context_finish_container(ctx, &cnt);
	};
	size_t n = ctx->total_written;
	if (ctx->close_cb)
		ctx->close_cb(ctx->userdata);
	free(ctx);
	return n;
}


static inline size_t priv_ubjw_context_write(ubjw_context_t* ctx, const uint8_t* data, size_t sz)
{
	ctx->total_written += sz;
	return ctx->write_cb(data, 1, sz, ctx->userdata);
}

static inline void priv_ubjw_tag_public(ubjw_context_t* ctx, UBJ_TYPE tid)
{
	struct priv_ubjw_container_t* ch = ctx->head;
	if (!ctx->ignore_container_flags)
	{
		/*if (
			(!(ch->flags & (CONTAINER_IS_UBJ_ARRAY | CONTAINER_IS_UBJ_OBJECT))) &&
			(tid != UBJ_ARRAY && tid !=UBJ_OBJECT))
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
		if (ch->flags & CONTAINER_IS_UBJ_OBJECT)
		{
			ch->flags |= CONTAINER_EXPECTS_KEY; //set key expected next time in this context
		}
		if ((ch->flags & CONTAINER_IS_TYPED) && ch->type == tid)
		{
			return;
		}
	}
	priv_ubjw_context_append(ctx, UBJI_TYPEC_convert[tid]);
}

static inline void priv_ubjw_write_raw_string(ubjw_context_t* ctx, const char* out)//TODO: possibly use a safe string
{
	size_t n = strlen(out);
	ctx->ignore_container_flags = 1; 
	ubjw_write_integer(ctx, (int64_t)n);
	ctx->ignore_container_flags = 0;
	priv_ubjw_context_write(ctx, (const uint8_t*)out, n);
}
void ubjw_write_string(ubjw_context_t* ctx, const char* out)
{
	priv_ubjw_tag_public(ctx,UBJ_STRING);
	priv_ubjw_write_raw_string(ctx, out);
}

static inline void priv_ubjw_write_raw_char(ubjw_context_t* ctx, char out)
{
	priv_ubjw_context_append(ctx, (uint8_t)out);
}
void ubjw_write_char(ubjw_context_t* ctx, char out)
{
	priv_ubjw_tag_public(ctx,UBJ_CHAR);
	priv_ubjw_write_raw_char(ctx, out);
}

static inline void priv_ubjw_write_raw_uint8(ubjw_context_t* ctx, uint8_t out)
{
	priv_ubjw_context_append(ctx, out);
}
void ubjw_write_uint8(ubjw_context_t* ctx, uint8_t out)
{
	priv_ubjw_tag_public(ctx,UBJ_UINT8);
	priv_ubjw_write_raw_uint8(ctx, out);
}

static inline void priv_ubjw_write_raw_int8(ubjw_context_t* ctx, int8_t out)
{
	priv_ubjw_context_append(ctx, *(uint8_t*)&out);
}
void ubjw_write_int8(ubjw_context_t* ctx, int8_t out)
{
	priv_ubjw_tag_public(ctx,UBJ_INT8);
	priv_ubjw_write_raw_int8(ctx, out);
}

static inline void priv_ubjw_write_raw_int16(ubjw_context_t* ctx, int16_t out)
{
	uint8_t buf[2];
	_to_bigendian16(buf, *(uint16_t*)&out);
	priv_ubjw_context_write(ctx, buf, 2);
}
void ubjw_write_int16(ubjw_context_t* ctx, int16_t out)
{
	priv_ubjw_tag_public(ctx,UBJ_INT16);
	priv_ubjw_write_raw_int16(ctx, out);
}
static inline void priv_ubjw_write_raw_int32(ubjw_context_t* ctx, int32_t out)
{
	uint8_t buf[4];
	_to_bigendian32(buf, *(uint32_t*)&out);
	priv_ubjw_context_write(ctx, buf, 4);
}
void ubjw_write_int32(ubjw_context_t* ctx, int32_t out)
{
	priv_ubjw_tag_public(ctx,UBJ_INT32);
	priv_ubjw_write_raw_int32(ctx, out);
}
static inline void priv_ubjw_write_raw_int64(ubjw_context_t* ctx, int64_t out)
{
	uint8_t buf[8];
	_to_bigendian64(buf, *(uint64_t*)&out);
	priv_ubjw_context_write(ctx, buf, 8);
}
void ubjw_write_int64(ubjw_context_t* ctx, int64_t out)
{
	priv_ubjw_tag_public(ctx,UBJ_INT64);
	priv_ubjw_write_raw_int64(ctx, out);
}

void ubjw_write_high_precision(ubjw_context_t* ctx, const char* hp)
{
	priv_ubjw_tag_public(ctx,UBJ_HIGH_PRECISION);
	priv_ubjw_write_raw_string(ctx, hp);
}

void ubjw_write_integer(ubjw_context_t* ctx, int64_t out)
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

static inline void priv_ubjw_write_raw_float32(ubjw_context_t* ctx, float out)
{
	uint32_t fout = *(uint32_t*)&out;
	uint8_t outbuf[4];
	_to_bigendian32(outbuf,fout);
	priv_ubjw_context_write(ctx, outbuf, 4);
}
void ubjw_write_float32(ubjw_context_t* ctx, float out)
{
	priv_ubjw_tag_public(ctx,UBJ_FLOAT32);
	priv_ubjw_write_raw_float32(ctx, out);
}
static inline void priv_ubjw_write_raw_float64(ubjw_context_t* ctx, double out)
{
	uint64_t fout = *(uint64_t*)&out;
	uint8_t outbuf[8];
	_to_bigendian64(outbuf, fout);
	priv_ubjw_context_write(ctx, outbuf, 8);
}
void ubjw_write_float64(ubjw_context_t* ctx, double out)
{
	priv_ubjw_tag_public(ctx,UBJ_FLOAT64);
	priv_ubjw_write_raw_float64(ctx, out);
}

void ubjw_write_floating_point(ubjw_context_t* ctx, double out)
{
	//this may not be possible to implement correctly...for now we just write it as a float64'
	ubjw_write_float64(ctx,out);
}

void ubjw_write_noop(ubjw_context_t* ctx)
{
	priv_ubjw_tag_public(ctx,UBJ_NOOP);
}
void ubjw_write_null(ubjw_context_t* ctx)
{
	priv_ubjw_tag_public(ctx,UBJ_NULLTYPE);
}
void ubjw_write_bool(ubjw_context_t* ctx, uint8_t out)
{
	priv_ubjw_tag_public(ctx,(out ? UBJ_BOOL_TRUE : UBJ_BOOL_FALSE));
}

void priv_ubjw_begin_container(struct priv_ubjw_container_t* cnt, ubjw_context_t* ctx, UBJ_TYPE typ, size_t count)
{
	cnt->flags=0;
	cnt->elements_remaining = count;
	cnt->type = typ;

	if (typ != UBJ_MIXED)
	{
		if (count == 0)
		{
			//error and return;
		}
		priv_ubjw_context_append(ctx, '$');
		priv_ubjw_context_append(ctx, UBJI_TYPEC_convert[typ]);
		cnt->flags |= CONTAINER_IS_TYPED;
	}
	if (count != 0)
	{
		priv_ubjw_context_append(ctx, '#');
		ctx->ignore_container_flags = 1;
		ubjw_write_integer(ctx, (int64_t)count);
		ctx->ignore_container_flags = 0;
		
		cnt->flags |= CONTAINER_IS_SIZED;
		cnt->elements_remaining = count;
	}
}
void ubjw_begin_array(ubjw_context_t* ctx, UBJ_TYPE type, size_t count)
{
	priv_ubjw_tag_public(ctx, UBJ_ARRAY); //todo: should this happen before any erro potential?
	struct priv_ubjw_container_t ch;
	priv_ubjw_begin_container(&ch, ctx, type, count);
	ch.flags |= CONTAINER_IS_UBJ_ARRAY;
	priv_ubjw_container_stack_push(ctx, &ch);
}

void ubjw_begin_object(ubjw_context_t* ctx, UBJ_TYPE type, size_t count)
{
	priv_ubjw_tag_public(ctx, UBJ_OBJECT);
	struct priv_ubjw_container_t ch;
	priv_ubjw_begin_container(&ch, ctx, type, count);
	ch.flags |= CONTAINER_EXPECTS_KEY | CONTAINER_IS_UBJ_OBJECT;
	priv_ubjw_container_stack_push(ctx, &ch);
}
void ubjw_write_key(ubjw_context_t* ctx, const char* key)
{
	if (ctx->head->flags & CONTAINER_EXPECTS_KEY && ctx->head->flags & CONTAINER_IS_UBJ_OBJECT)
	{
		priv_ubjw_write_raw_string(ctx, key);
		ctx->head->flags ^= CONTAINER_EXPECTS_KEY; //turn off container 
	}
	else
	{
		//error unexpected key
	}
}
void ubjw_end(ubjw_context_t* ctx)
{
	struct priv_ubjw_container_t ch = priv_ubjw_container_stack_pop(ctx);
	if ((ch.flags & CONTAINER_IS_UBJ_OBJECT) && !(ch.flags & CONTAINER_EXPECTS_KEY))
	{
		//error expected value
	}
	priv_ubjw_context_finish_container(ctx, &ch);
}


static inline void priv_ubjw_write_byteswap(ubjw_context_t* ctx, const uint8_t* data, int sz, size_t count)
{
	uint8_t buf[BUFFER_OUT_SIZE];

	size_t i;
	size_t nbytes = sz*count;
	for (i = 0; i < nbytes; i+=BUFFER_OUT_SIZE)
	{
		size_t npass = min(nbytes - i, BUFFER_OUT_SIZE);
		memcpy(buf, data + i, npass);
		buf_endian_swap(buf, sz, npass/sz);
		priv_ubjw_context_write(ctx, buf, npass);
	}
}
void ubjw_write_buffer(ubjw_context_t* ctx, const uint8_t* data, UBJ_TYPE type, size_t count)
{
	int typesz = UBJI_TYPE_size[type];
	if (typesz < 0)
	{
		//error cannot read from a buffer of this type.
	}
	ubjw_begin_array(ctx, type, count);
	if (type == UBJ_STRING || type == UBJ_HIGH_PRECISION)
	{
		const char** databufs = (const char**)data;
		size_t i;
		for (i = 0; i < count; i++)
		{
			priv_ubjw_write_raw_string(ctx, databufs[i]);
		}
	}
	else if (typesz == 1 || _is_bigendian())
	{
		size_t n = typesz*count;
		priv_ubjw_context_write(ctx, data, typesz*count);
	}
	else if (typesz > 1) //and not big-endian
	{
		priv_ubjw_write_byteswap(ctx, data,typesz,count);
	}
	ubjw_end(ctx);
}