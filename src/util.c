#include <ctype.h>
#include <stdlib.h>

#include "util.h"

unsigned long oct2dec(unsigned long int value)
{
	unsigned long int oct, dec = 0;
	int i = 0;

	oct = value;
	while ( oct != 0 ) {
		dec = dec + (oct % 10) * (1 << (3 * (i++)));
		oct = oct / 10;
	}

	return dec;
}


static int nextDigit(int so_far, char ** in, int base)
{
	char buf[2] = "0";
	buf[0] = **in;
	if ( isxdigit(buf[0]) ) {
		++(*in);
		return so_far * base + strtol(buf, NULL, base);
	} else {
		return so_far;
	}
}


char * nextToken(char * in, char * out)
{
	char quote = 0;
	int end = 0;
	char c;
	for ( ; *in && isspace(*in); ++in ) {}
	for ( ; (c = *in) != 0 && ! end; ++in ) {
		int escaped = 0;
		switch ( c ) {
			case '"':
			case '\'':
				if ( quote == 0 ) {
					quote = *in;
					c = 0;
				} else if ( quote == *in ) {
					quote = 0;
					c = 0;
				} else {
					/* treat as a normal character */
				}
			break;
			case '\\':
				c = *(++in);
				escaped = 1;
				switch ( c ) {
					case '\0':
						--in;
						c = 0;
						break;
					case 'a':  c = '\a'; break;
					case 'b':  c = '\b'; break;
					case 'E':
					case 'e':  c = 27;   break; /* escape */
					case 'f':  c = '\f'; break;
					case 'n':  c = '\n'; break;
					case 'r':  c = '\r'; break;
					case 't':  c = '\t'; break;
					case 'v':  c = '\v'; break;
					case 'x':
						++in;
						c = nextDigit(0, &in, 16);
						c = nextDigit(c, &in, 16);
						--in;
						break;
					case '0'...'7':
						c = nextDigit(0, &in, 8);
						c = nextDigit(c, &in, 8);
						c = nextDigit(c, &in, 8);
						--in;
						break;
					default:						
						break;
				}
				break;
			case '\t':
			case ' ':
				if ( quote == 0 && ! escaped ) {
					c = 0;
					end = 1;
				}
			default:
				break;
		}
		if ( c && out ) {
			*(out++) = c;
		}
	}
	if ( out ) {
		*out = 0;
	}
	return in;
}
