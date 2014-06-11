#include "ubj_internal.h"

void ubjrw_write_dynamic_t(ubjw_context_t* ctx, ubjr_dynamic_t dobj)
{
	switch (dobj.type)
	{
	case UBJ_MIXED:
		break;///error, can't be mixed
	case UBJ_NULLTYPE:
		ubjw_write_null(ctx);
		break;
	case UBJ_NOOP:
		ubjw_write_noop(ctx);
		break;
	case UBJ_BOOL_FALSE:
		ubjw_write_bool(ctx, 0);
		break;
	case UBJ_BOOL_TRUE:
		ubjw_write_bool(ctx, 1);
		break;
	case UBJ_CHAR:
		ubjw_write_char(ctx, *dobj.string);//first character of string
		break;
	case UBJ_STRING:
		ubjw_write_string(ctx, dobj.string);
		break;
	case UBJ_HIGH_PRECISION:
		ubjw_write_high_precision(ctx, dobj.string);
		break;
	case UBJ_INT8:
		ubjw_write_int8(ctx, dobj.integer);
		break;
	case UBJ_UINT8:
		ubjw_write_uint8(ctx, dobj.integer);
		break;
	case UBJ_INT16:
		ubjw_write_int16(ctx, dobj.integer);
		break;
	case UBJ_INT32:
		ubjw_write_int32(ctx, dobj.integer);
		break;
	case UBJ_INT64:
		ubjw_write_int64(ctx, dobj.integer);
		break;
	case UBJ_FLOAT32:
		ubjw_write_float32(ctx, dobj.real);
		break;
	case UBJ_FLOAT64:
		ubjw_write_float64(ctx, dobj.real);
		break;
	case UBJ_ARRAY:
		if (dobj.container_array.type != UBJ_MIXED && dobj.container_array.type != UBJ_OBJECT && dobj.container_array.type != UBJ_ARRAY)
		{
			ubjw_write_buffer(ctx, dobj.container_array.values, dobj.container_array.type, dobj.container_array.size);
		}
		else
		{
			size_t i = 0;
			ubjw_begin_array(ctx, dobj.container_array.type, dobj.container_array.size);
			for (i = 0; i < dobj.container_array.size; i++)
			{
				ubjw_
			}
			ubjw_end(ctx);
		}
	case UBJ_OBJECT:
		break;
	};

}
