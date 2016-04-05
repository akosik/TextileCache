#include <stdio.h>
#include <stdbool.h>
#include "testing.h"

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define RESET "\033[0m"
#define CHECK "\xE2\x9C\x94"
#define EX "\xE2\x9C\x98"

void test(bool test, const char *label)
{
    if (test) printf(KGRN "" CHECK "" RESET "Passed : %s\n", label);
    else printf(KRED "" EX "" RESET "Failed : %s\n", label);
}
