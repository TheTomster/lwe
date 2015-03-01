/* err.c (c) 2015 Tom Wright */

#include <stdio.h>

static char errbuf[256];

void seterr(char *str)
{
	snprintf(errbuf, sizeof(errbuf), "%s", str);
}

void geterr(char *str, int sz)
{
	snprintf(str, sz, "%s", errbuf);
}

