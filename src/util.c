#include <math.h>
#include "util.h"

unsigned long oct2dec(unsigned long int value)
{
	unsigned long int oct, dec = 0;
	int i = 0;

	oct = value;
	while ( oct != 0 ) {
		dec = dec + (oct % 10) * pow(8, i++);
		oct = oct / 10;
	}

	return dec;
}
