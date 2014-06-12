#include "ubj_internal.h"

void ubjrw_write_dynamic_t(ubjw_context_t* ctx, ubjr_dynamic_t dobj)
{
	size_t ctyp, csize;
	switch (dobj.type)
	{
	case UBJ_MIXED:
		return;///error, can't be mixed
	case UBJ_NULLTYPE:
		ubjw_write_null(ctx);
		return;
	case UBJ_NOOP:
		ubjw_write_noop(ctx);
		return;
	case UBJ_BOOL_FALSE:
		ubjw_write_bool(ctx, 0);
		return;
	case UBJ_BOOL_TRUE:
		ubjw_write_bool(ctx, 1);
		return;
	case UBJ_CHAR:
		ubjw_write_char(ctx, *dobj.string);//first character of string
		return;
	case UBJ_STRING:
		ubjw_write_string(ctx, dobj.string);
		return;
	case UBJ_HIGH_PRECISION:
		ubjw_write_high_precision(ctx, dobj.string);
		return;
	case UBJ_INT8:
		ubjw_write_int8(ctx, (int8_t)dobj.integer);
		return;
	case UBJ_UINT8:
		ubjw_write_uint8(ctx, (uint8_t)dobj.integer);
		return;
	case UBJ_INT16:
		ubjw_write_int16(ctx, (int16_t)dobj.integer);
		return;
	case UBJ_INT32:
		ubjw_write_int32(ctx, (int32_t)dobj.integer);
		return;
	case UBJ_INT64:
		ubjw_write_int64(ctx, dobj.integer);
		return;
	case UBJ_FLOAT32:
		ubjw_write_float32(ctx, (float)dobj.real);
		return;
	case UBJ_FLOAT64:
		ubjw_write_float64(ctx, dobj.real);
		return;
	case UBJ_ARRAY:
		if (dobj.container_array.type != UBJ_MIXED && dobj.container_array.type != UBJ_OBJECT && dobj.container_array.type != UBJ_ARRAY)
		{
			ubjw_write_buffer(ctx, dobj.container_array.values, dobj.container_array.type, dobj.container_array.size);
			return;
		}
		else
		{
			ctyp = dobj.container_array.type;
			csize = dobj.container_array.size;

			size_t i;
			ubjr_dynamic_t scratch;
			size_t ls = UBJR_TYPE_localsize[dobj.container_array.type];
			ubjw_begin_array(ctx, dobj.container_array.type, dobj.container_array.size);
			for (i = 0; i < dobj.container_array.size; i++)
			{
				scratch = priv_ubjr_pointer_to_dynamic(dobj.container_array.type, dobj.container_array.values+ls*i);
				ubjrw_write_dynamic_t(ctx, scratch);
			}
			ubjw_end(ctx);
		}
	case UBJ_OBJECT:
		break;
	};

}
