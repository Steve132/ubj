#ifndef UBJ_INTERNAL_H
#endif UBJ_INTERNAL_H

#include "ubj.h"
#include <stdlib.h>
#include <string.h>

#if _MSC_VER
#define inline __inline
#endif


static const uint8_t UBJI_TYPEC_convert[] = "\x00ZNTFSCiUIlLdDH[{";

static inline int priv_UBJ_TYPE_size(UBJ_TYPE type)
{
	static const int UBJI_TYPE_size[] = 
	{	-1,	
		0, 
		0, 
		0, 
		0, 
		sizeof(const char*),


		
		; typedef enum
	{
		UBJ_MIXED = 0,
		UBJ_NULLTYPE = 'Z',
		UBJ_NOOP = 'N',
		UBJ_BOOL_TRUE = 'T',
		UBJ_BOOL_FALSE = 'F',

		UBJ_STRING = 'S',
		UBJ_CHAR = 'C',

		UBJ_INT8 = 'i',
		UBJ_UINT8 = 'U',
		UBJ_INT16 = 'I',
		UBJ_INT32 = 'l',
		UBJ_INT64 = 'L',
		UBJ_FLOAT32 = 'd',
		UBJ_FLOAT64 = 'D',
		UBJ_HIGH_PRECISION = 'H',

		UBJ_ARRAY = '[',
		UBJ_OBJECT = '{',
	} UBJ_TYPE;
	switch (type)
	{
	case UBJ_NULLTYPE:
	case UBJ_NOOP:
	case UBJ_BOOL_TRUE:
	case UBJ_BOOL_FALSE:
		return 0;
	case UBJ_STRING:
	case UBJ_HIGH_PRECISION:
		return sizeof(const char*);
	case UBJ_CHAR:
	case UBJ_INT8:
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
	return -1;
}


/*typedef enum
{
	UBJ_MIXED = 0,
	UBJ_NULLTYPE = 'Z',
	UBJ_NOOP = 'N',
	UBJ_BOOL_TRUE = 'T',
	UBJ_BOOL_FALSE = 'F',

	UBJ_STRING = 'S',
	UBJ_CHAR = 'C',

	UBJ_INT8 = 'i',
	UBJ_UINT8 = 'U',
	UBJ_INT16 = 'I',
	UBJ_INT32 = 'l',
	UBJ_INT64 = 'L',
	UBJ_FLOAT32 = 'd',
	UBJ_FLOAT64 = 'D',
	UBJ_HIGH_PRECISION = 'H',

	UBJ_ARRAY = '[',
	UBJ_OBJECT = '{',
} UBJ_TYPEc;*/

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

static inline uint8_t _is_bigendian()
{
	int i = 1;
	char *low = (char*)&i;
	return *low ? 0 : 1;
}

#define BUF_BIG_ENDIAN_SWAP(type,func,ptr,num)  \
	{											\
		size_t i;type* d = (type*)ptr; 					\
		for (i = 0; i < num; i++)				\
		{										\
			func((uint8_t*)&d[i], d[i]);		\
		}										\
	}											\

static inline void buf_endian_swap(uint8_t* buf, size_t sz, size_t n)
{
	if (!_is_bigendian())
	{
		switch (sz)
		{
		case 2:
			BUF_BIG_ENDIAN_SWAP(uint16_t, _to_bigendian16,buf,n);
			break;
		case 4:
			BUF_BIG_ENDIAN_SWAP(uint32_t, _to_bigendian32,buf,n);
			break;
		case 8:
			BUF_BIG_ENDIAN_SWAP(uint64_t, _to_bigendian64,buf,n);
			break;
		};
	}
}