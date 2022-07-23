#include <inttypes.h>

#include "helpers.hpp"

/* Max 24 bit */
uint32_t getbits(const uint8_t *buf, uint32_t offset, uint8_t len)
{
	const uint8_t *a = buf + (offset / 8);
	uint32_t retval = 0;
	uint32_t mask = 1;

	retval = ((*(a) << 8) | *(a + 1));
	mask <<= len;

	if (len > 8)
	{
		retval <<= 8;
		retval |= *(a + 2);
		len -= 8;
	}
	if (len > 8)
	{
		retval <<= 8;
		retval |= *(a + 3);
		len -= 8;
	}
	if (len > 8)
	{
		uint64_t tmp = retval << 8;
		tmp |= *(a + 4);
		tmp >>= ((8 - (offset % 8)) + (8 - (len)));
		return tmp & (mask - 1);
	}

	retval >>= ((8 - (offset % 8)) + (8 - len));
	return retval & (mask - 1);
}
