#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termcap.h>

char PC;
char *BC;
char *UP;
short ospeed;

static char *ent;

static char*
slurp(char *file)
{
	char *b, *nb;
	long n, m;
	int fd, r;

	fd = open(file, O_RDONLY);
	if(fd < 0)
		return NULL;
	b = NULL;
	n = m = 0;
	for(;;){
		if(n+1 >= m){
			m = m ? 2*m : 8192;
			nb = realloc(b, m);
			if(nb == NULL){
				free(b);
				close(fd);
				return NULL;
			}
			b = nb;
		}
		r = read(fd, b+n, m-n-1);
		if(r <= 0)
			break;
		n += r;
	}
	close(fd);
	if(r < 0){
		free(b);
		return NULL;
	}
	b[n] = 0;
	return b;
}

static int
named(char *e, const char *name)
{
	char *p;
	int n;

	n = strlen(name);
	if(n == 0)
		return 0;
	for(p = e; ; p++){
		if(strncmp(p, name, n) == 0 && (p[n] == '|' || p[n] == ':' || p[n] == 0))
			return 1;
		p += strcspn(p, "|:");
		if(*p != '|')
			return 0;
	}
}

static char*
entry(char *s, const char *name)
{
	char *b, *q;

	b = malloc(strlen(s)+1);
	if(b == NULL)
		return NULL;
	while(*s != 0){
		q = b;
		while(*s != 0 && *s != '\n'){
			if(s[0] == '\\' && s[1] == '\n'){
				s += 2;
				while(*s == ' ' || *s == '\t')
					s++;
				continue;
			}
			*q++ = *s++;
		}
		*q = 0;
		if(*s == '\n')
			s++;
		if(b[0] != '#' && b[0] != 0 && named(b, name))
			return b;
	}
	free(b);
	return NULL;
}

int
tgetent(char *bp, const char *name)
{
	char *e, *file, *s;

	free(ent);
	ent = NULL;
	if(name == NULL)
		return 0;
	file = "/sys/lib/termcap";
	e = getenv("TERMCAP");
	if(e != NULL && e[0] == '/')
		file = e;
	else if(e != NULL && e[0] != 0)
		ent = entry(e, name);
	if(ent == NULL){
		s = slurp(file);
		if(s == NULL)
			return -1;
		ent = entry(s, name);
		free(s);
		if(ent == NULL)
			return 0;
	}
	if(bp != NULL){
		strncpy(bp, ent, 1023);
		bp[1023] = 0;
	}
	return 1;
}

static char*
skip(char *p)
{
	for(; *p != 0 && *p != ':'; p++)
		if(p[0] == '\\' && p[1] != 0)
			p++;
	return p;
}

static char*
find(const char *id)
{
	char *p;

	if(ent == NULL || id == NULL || id[0] == 0 || id[1] == 0)
		return NULL;
	for(p = skip(ent); *p == ':'; p = skip(p+1))
		if(p[1] == id[0] && p[2] == id[1])
			switch(p[3]){
			case '@':
				return NULL;
			case ':':
			case '=':
			case '#':
			case 0:
				return p+3;
			}
	return NULL;
}

int
tgetnum(const char *id)
{
	char *p, *v;
	int n;

	v = NULL;
	if(id != NULL && strcmp(id, "co") == 0)
		v = getenv("COLUMNS");
	else if(id != NULL && strcmp(id, "li") == 0)
		v = getenv("LINES");
	if(v != NULL && (n = atoi(v)) > 0)
		return n;
	p = find(id);
	if(p == NULL || *p != '#')
		return -1;
	return strtol(p+1, NULL, p[1] == '0' ? 8 : 10);
}

int
tgetflag(const char *id)
{
	char *p;

	p = find(id);
	return p != NULL && (*p == ':' || *p == 0);
}

char*
tgetstr(const char *id, char **area)
{
	char *p, *s, *q;
	int c, i;

	p = find(id);
	if(p == NULL || *p != '=')
		return NULL;
	p++;
	if(area != NULL && *area != NULL)
		s = *area;
	else if((s = malloc(strlen(p)+1)) == NULL)
		return NULL;
	for(q = s; *p != 0 && *p != ':'; p++){
		c = *p;
		if(c == '^' && p[1] != 0){
			c = *++p;
			c = c == '?' ? 0177 : c & 037;
		}else if(c == '\\' && p[1] != 0){
			c = *++p;
			switch(c){
			case 'E':
			case 'e':
				c = 033;
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 's':
				c = ' ';
				break;
			default:
				if(c >= '0' && c <= '7'){
					c = 0;
					for(i = 0; i < 3 && *p >= '0' && *p <= '7'; i++)
						c = c*8 + *p++ - '0';
					p--;
					if(c == 0)
						c = 0200;
				}
			}
		}
		*q++ = c;
	}
	*q++ = 0;
	if(area != NULL && *area != NULL)
		*area = q;
	return s;
}

char*
tgoto(const char *cm, int col, int line)
{
	static char buf[256];
	char added[64], *d, *e;
	int v, oncol, n;

	if(cm == NULL)
		return "OOPS";
	added[0] = 0;
	d = buf;
	e = buf + sizeof buf - 16;
	oncol = 0;
	v = line;
	while(*cm != 0){
		if(d >= e)
			return "OOPS";
		if(*cm != '%'){
			*d++ = *cm++;
			continue;
		}
		cm++;
		switch(*cm++){
		case 'd':
		case '2':
		case '3':
			n = cm[-1] == 'd' ? 0 : cm[-1] - '0';
			d += sprintf(d, "%0*d", n, v);
			break;
		case '+':
			if(*cm == 0)
				return "OOPS";
			v += *cm++;
		case '.':
			if(v == 0 || v == 004 || v == '\n')
				if(oncol || UP != NULL)
					do{
						if(strlen(added) + 16 > sizeof added)
							return "OOPS";
						strcat(added, oncol ? (BC != NULL ? BC : "\b") : UP);
						v++;
					}while(v == '\n');
			*d++ = v;
			break;
		case '>':
			if(cm[0] == 0 || cm[1] == 0)
				return "OOPS";
			if(v > cm[0])
				v += cm[1];
			cm += 2;
			continue;
		case 'r':
			oncol = 1;
			v = col;
			continue;
		case 'i':
			col++;
			line++;
			v++;
			continue;
		case 'n':
			col ^= 0140;
			line ^= 0140;
			v = oncol ? col : line;
			continue;
		case 'B':
			v = (v/10 << 4) + v%10;
			continue;
		case 'D':
			v = v - 2*(v%16);
			continue;
		case '%':
			*d++ = '%';
			continue;
		default:
			return "OOPS";
		}
		oncol = !oncol;
		v = oncol ? col : line;
	}
	if(d + strlen(added) >= buf + sizeof buf)
		return "OOPS";
	strcpy(d, added);
	return buf;
}

int
tputs(const char *s, int affcnt, int (*outc)(int))
{
	if(s == NULL)
		return -1;
	while((*s >= '0' && *s <= '9') || *s == '.')
		s++;
	if(*s == '*')
		s++;
	while(*s != 0)
		outc(*s++);
	return 0;
}
