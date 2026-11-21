#include <u.h>
#include <libc.h>
#include <vt.h>

ulong vtansi[16] = {
	0x000000, 0xAA0000, 0x00AA00, 0xFF5500, 0x0000FF, 0xAA00AA, 0x00AAAA, 0x7F7F7F,
	0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF,
};

static int
cube(int n)
{
	return n ? 55+40*n : 0;
}

ulong
vtrgb(ulong c, ulong *pal, ulong def)
{
	int i, g;

	switch(c & 0xFF000000){
	case Vindex:
		i = c & 0xFF;
		if(i < 16)
			return pal != nil ? pal[i] : vtansi[i];
		if(i < 232){
			i -= 16;
			return cube(i/36)<<16 | cube(i/6%6)<<8 | cube(i%6);
		}
		g = 8+10*(i-232);
		return g<<16 | g<<8 | g;
	case Vrgb:
		return c & 0xFFFFFF;
	}
	return def;
}
