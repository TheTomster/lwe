/* (C) 2015 Tom Wright. */

#include "draw.h"

int scroll;

int scroll_line()
{
	return scroll;
}

void set_scroll(int n)
{
	scroll = n;
	if (scroll < 0)
		scroll = 0;
}

