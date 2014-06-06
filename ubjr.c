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
	ctx->total_written += 1;
	ctx->read_cb(&a, 1, 1, ctx->userdata);
	return a;
}

uint8_t type;
union
{
	double
	float
	struct { const char* data; size_t len; } string;
};
void ubjw_read_string(struct ubjw_context_t* dst, const char* out);
void ubjw_read_char(struct ubjw_context_t* dst, char out);
