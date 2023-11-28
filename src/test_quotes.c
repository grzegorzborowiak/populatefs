/* compile manually:

   gcc -o test_quotes test_quotes.c util.c

*/

#include "util.h"

#include <stdio.h>
#include <string.h>

void test(const char *str)
{
    const char * p;
    char buf[10000];
    p = nextToken(str, buf);
    printf("in:   |%s|\n", str);
    printf("out:  |%s|\n", buf);
    printf("rest: |%s|\n", p);
    puts("");
}

int main()
{
    test("");
    test(" ");
    test("admin 2");
    test("solo");
    test("raw space 53");
    test("\"quoted space\" 12 34");
    test("\"quoted 'x' space\" 13");
    test("escaped\\ space 90");
    test("enter\\nsandman 13");
    test("enter\\nthe\\ndragon");
    test("octal_code_\\101 65");
    test("hex_code_\\x42 66");
    test("truncated_octal_code_\\2");
    test("truncated_octal_code_\\2 rest");
    test("truncated_hex_code_\\x9");
    test("truncated_hex_code_\\x9 rest");
    return 0;
}
