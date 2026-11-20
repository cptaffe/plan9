#include <u.h>
#include <libc.h>

enum
{
	Lmonth = 1,
	Smonth,
	Nmonth,
	Zmonth,
	Lwday,
	Swday,
	Day,
	Uday,
	Zday,
	Uyday,
	Zyday,
	Hour,
	H12,
	Z12,
	Min,
	Zmin,
	Sec,
	Zsec,
	Lyear,
	Year,
	PM,
	Pm,
	TZ,
	Iso,
	Isosec,
	Isoshort,
	Isocolon,
	Isocolonsec,
	Num,
	Numsec,
	Numshort,
	Numcolon,
	Numcolonsec,
	Frac0,
	Frac9,
};

static char *lmonth[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};
static char *lwday[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};
static int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int std0x[] = {Zmonth, Zday, Z12, Zmin, Zsec, Year};

static char *zones[] = {
	"-070000", "-07:00:00", "-0700", "-07:00", "-07",
	"Z070000", "Z07:00:00", "Z0700", "Z07:00", "Z07",
};
static int zcode[] = {
	Numsec, Numcolonsec, Num, Numcolon, Numshort,
	Isosec, Isocolonsec, Iso, Isocolon, Isoshort,
};

static int
lower(char *s)
{
	return *s >= 'a' && *s <= 'z';
}

static int
isdig(char c)
{
	return c >= '0' && c <= '9';
}

static int
chunk(char *l, int *pre, int *len, int *nd)
{
	char *s, *t;
	int i;

	for(s = l; *s; s++){
		*pre = s - l;
		switch(*s){
		case 'J':
			if(strncmp(s, "January", 7) == 0){
				*len = 7;
				return Lmonth;
			}
			if(strncmp(s, "Jan", 3) == 0 && !lower(s+3)){
				*len = 3;
				return Smonth;
			}
			break;
		case 'M':
			if(strncmp(s, "Monday", 6) == 0){
				*len = 6;
				return Lwday;
			}
			if(strncmp(s, "Mon", 3) == 0 && !lower(s+3)){
				*len = 3;
				return Swday;
			}
			if(strncmp(s, "MST", 3) == 0){
				*len = 3;
				return TZ;
			}
			break;
		case '0':
			if(s[1] >= '1' && s[1] <= '6'){
				*len = 2;
				return std0x[s[1]-'1'];
			}
			if(s[1] == '0' && s[2] == '2'){
				*len = 3;
				return Zyday;
			}
			break;
		case '1':
			if(s[1] == '5'){
				*len = 2;
				return Hour;
			}
			*len = 1;
			return Nmonth;
		case '2':
			if(strncmp(s, "2006", 4) == 0){
				*len = 4;
				return Lyear;
			}
			*len = 1;
			return Day;
		case '_':
			if(s[1] == '2'){
				if(strncmp(s+1, "2006", 4) == 0){
					*pre = s+1 - l;
					*len = 4;
					return Lyear;
				}
				*len = 2;
				return Uday;
			}
			if(s[1] == '_' && s[2] == '2'){
				*len = 3;
				return Uyday;
			}
			break;
		case '3':
			*len = 1;
			return H12;
		case '4':
			*len = 1;
			return Min;
		case '5':
			*len = 1;
			return Sec;
		case 'P':
			if(s[1] == 'M'){
				*len = 2;
				return PM;
			}
			break;
		case 'p':
			if(s[1] == 'm'){
				*len = 2;
				return Pm;
			}
			break;
		case '-':
		case 'Z':
			for(i = 0; i < nelem(zones); i++)
				if(zones[i][0] == *s && strncmp(s, zones[i], strlen(zones[i])) == 0){
					*len = strlen(zones[i]);
					return zcode[i];
				}
			break;
		case '.':
		case ',':
			if(s[1] == '0' || s[1] == '9'){
				for(t = s+1; *t == s[1]; t++)
					;
				if(!isdig(*t)){
					*len = t - s;
					*nd = t - s - 1;
					return s[1] == '0' ? Frac0 : Frac9;
				}
			}
			break;
		}
	}
	*pre = s - l;
	*len = 0;
	return 0;
}

static int
leap(int y)
{
	return y%4 == 0 && (y%100 != 0 || y%400 == 0);
}

static int
monthdays(int m, int y)
{
	if(m == 1 && leap(y))
		return 29;
	return mdays[m];
}

static vlong
days(int y, int m, int d)
{
	vlong era, yoe, doy, doe;

	if(m < 2)
		y--;
	era = (y >= 0 ? y : y-399) / 400;
	yoe = y - era*400;
	doy = (153*(m + (m > 1 ? -2 : 10)) + 2)/5 + d-1;
	doe = yoe*365 + yoe/4 - yoe/100 + doy;
	return era*146097 + doe - 719468;
}

vlong
tmsec(Tm *tm)
{
	return days(tm->year+1900, tm->mon, tm->mday)*86400 + tm->hour*3600 + tm->min*60 + tm->sec - tm->tzoff;
}

static void
num(Fmt *f, int v, int w)
{
	if(v < 0){
		fmtprint(f, "-");
		v = -v;
	}
	fmtprint(f, "%0*d", w, v);
}

static void
zone(Fmt *f, int c, int off)
{
	int m;

	if(off == 0 && c >= Iso && c <= Isocolonsec){
		fmtprint(f, "Z");
		return;
	}
	m = off/60;
	if(m < 0){
		fmtprint(f, "-");
		m = -m;
		off = -off;
	}else
		fmtprint(f, "+");
	fmtprint(f, "%02d", m/60);
	if(c == Isocolon || c == Numcolon || c == Isocolonsec || c == Numcolonsec)
		fmtprint(f, ":");
	if(c != Isoshort && c != Numshort)
		fmtprint(f, "%02d", m%60);
	if(c == Isosec || c == Numsec || c == Isocolonsec || c == Numcolonsec){
		if(c == Isocolonsec || c == Numcolonsec)
			fmtprint(f, ":");
		num(f, off%60, 2);
	}
}

char*
tmfmt(Tm *tm, char *layout)
{
	Fmt f;
	int c, pre, len, nd, h, y;

	fmtstrinit(&f);
	while(*layout){
		nd = 0;
		c = chunk(layout, &pre, &len, &nd);
		fmtprint(&f, "%.*s", pre, layout);
		layout += pre+len;
		y = tm->year+1900;
		h = tm->hour%12;
		if(h == 0)
			h = 12;
		switch(c){
		case Year:
			num(&f, (y < 0 ? -y : y)%100, 2);
			break;
		case Lyear:
			num(&f, y, 4);
			break;
		case Smonth:
			fmtprint(&f, "%.3s", lmonth[tm->mon]);
			break;
		case Lmonth:
			fmtprint(&f, "%s", lmonth[tm->mon]);
			break;
		case Nmonth:
			num(&f, tm->mon+1, 0);
			break;
		case Zmonth:
			num(&f, tm->mon+1, 2);
			break;
		case Swday:
			fmtprint(&f, "%.3s", lwday[tm->wday]);
			break;
		case Lwday:
			fmtprint(&f, "%s", lwday[tm->wday]);
			break;
		case Day:
			num(&f, tm->mday, 0);
			break;
		case Uday:
			fmtprint(&f, "%2d", tm->mday);
			break;
		case Zday:
			num(&f, tm->mday, 2);
			break;
		case Uyday:
			fmtprint(&f, "%3d", tm->yday+1);
			break;
		case Zyday:
			num(&f, tm->yday+1, 3);
			break;
		case Hour:
			num(&f, tm->hour, 2);
			break;
		case H12:
			num(&f, h, 0);
			break;
		case Z12:
			num(&f, h, 2);
			break;
		case Min:
			num(&f, tm->min, 0);
			break;
		case Zmin:
			num(&f, tm->min, 2);
			break;
		case Sec:
			num(&f, tm->sec, 0);
			break;
		case Zsec:
			num(&f, tm->sec, 2);
			break;
		case PM:
			fmtprint(&f, tm->hour >= 12 ? "PM" : "AM");
			break;
		case Pm:
			fmtprint(&f, tm->hour >= 12 ? "pm" : "am");
			break;
		case TZ:
			if(tm->zone[0])
				fmtprint(&f, "%s", tm->zone);
			else
				zone(&f, Num, tm->tzoff);
			break;
		case Frac0:
			fmtprint(&f, "%c%0*d", layout[-len], nd > 9 ? 9 : nd, 0);
			break;
		case Frac9:
			break;
		default:
			if(c >= Iso && c <= Numcolonsec)
				zone(&f, c, tm->tzoff);
			break;
		}
	}
	return fmtstrflush(&f);
}

static int
lookup(char **s, char **tab, int n, int len)
{
	int i, m;

	for(i = 0; i < n; i++){
		m = len ? len : strlen(tab[i]);
		if(cistrncmp(*s, tab[i], m) == 0){
			*s += m;
			return i;
		}
	}
	return -1;
}

static int
getnum(char **s, int fixed, int *v)
{
	char *p;

	p = *s;
	if(!isdig(p[0]))
		return -1;
	if(!isdig(p[1])){
		if(fixed)
			return -1;
		*v = p[0]-'0';
		*s = p+1;
		return 0;
	}
	*v = (p[0]-'0')*10 + p[1]-'0';
	*s = p+2;
	return 0;
}

static int
getnum3(char **s, int fixed, int *v)
{
	int i, n;

	n = 0;
	for(i = 0; i < 3 && isdig((*s)[i]); i++)
		n = n*10 + (*s)[i]-'0';
	if(i == 0 || fixed && i != 3)
		return -1;
	*v = n;
	*s += i;
	return 0;
}

static int
skip(char **s, char *p, int n)
{
	char *v;

	v = *s;
	while(n > 0){
		if(*p == ' '){
			if(*v != 0 && *v != ' ')
				return -1;
			while(n > 0 && *p == ' '){
				p++;
				n--;
			}
			while(*v == ' ')
				v++;
			continue;
		}
		if(*v != *p)
			return -1;
		v++;
		p++;
		n--;
	}
	*s = v;
	return 0;
}

static int
signedoff(char *s)
{
	int n, x;

	if(s[0] != '+' && s[0] != '-' || !isdig(s[1]))
		return 0;
	x = 0;
	for(n = 1; isdig(s[n]); n++)
		if((x = x*10 + s[n]-'0') > 23)
			return 0;
	return n;
}

static int
zonelen(char *s)
{
	int n;

	if(strncmp(s, "ChST", 4) == 0 || strncmp(s, "MeST", 4) == 0)
		return 4;
	if(strncmp(s, "GMT", 3) == 0)
		return 3 + signedoff(s+3);
	if(s[0] == '+' || s[0] == '-')
		return signedoff(s);
	for(n = 0; n < 6 && s[n] >= 'A' && s[n] <= 'Z'; n++)
		;
	switch(n){
	case 3:
		return 3;
	case 4:
		if(s[3] == 'T' || strncmp(s, "WITA", 4) == 0)
			return 4;
		break;
	case 5:
		if(s[4] == 'T')
			return 5;
		break;
	}
	return 0;
}

static int
getzone(char **s, int c, int *off)
{
	char *v, buf[10];
	int n, hr, mm, ss;

	v = *s;
	if(c >= Iso && c <= Isocolonsec && *v == 'Z'){
		*s = v+1;
		*off = 0;
		return 0;
	}
	switch(c){
	case Iso: case Num: n = 5; break;
	case Isocolon: case Numcolon: n = 6; break;
	case Isoshort: case Numshort: n = 3; break;
	case Isosec: case Numsec: n = 7; break;
	default: n = 9; break;
	}
	if(strlen(v) < n)
		return -1;
	memmove(buf, v, n);
	buf[n] = 0;
	if((c == Isocolon || c == Numcolon || c == Isocolonsec || c == Numcolonsec) && buf[3] != ':')
		return -1;
	if((c == Isocolonsec || c == Numcolonsec) && buf[6] != ':')
		return -1;
	hr = mm = ss = 0;
	v = buf+1;
	if(getnum(&v, 1, &hr) < 0)
		return -1;
	if(*v == ':')
		v++;
	if(*v && getnum(&v, 1, &mm) < 0)
		return -1;
	if(*v == ':')
		v++;
	if(*v && getnum(&v, 1, &ss) < 0)
		return -1;
	if(hr > 24 || mm > 60 || ss > 60){
		werrstr("time zone offset out of range");
		return -1;
	}
	*off = (hr*60 + mm)*60 + ss;
	if(buf[0] == '-')
		*off = -*off;
	else if(buf[0] != '+')
		return -1;
	*s += n;
	return 0;
}

static int
frac(char **s, int n)
{
	int i;

	if((*s)[0] != '.' && (*s)[0] != ',')
		return -1;
	for(i = 1; i < n && i < 10; i++)
		if(!isdig((*s)[i]))
			return -1;
	*s += n;
	return 0;
}

int
tmparse(Tm *tm, char *layout, char *s)
{
	char *l0, *s0, name[8];
	int c, pre, len, nd, year, mon, day, yday, hour, min, sec, off, am, pm, x, m, utc, big;
	Tm *lt;
	vlong t;

	l0 = layout;
	s0 = s;
	year = 0;
	mon = day = yday = off = -1;
	hour = min = sec = 0;
	am = pm = utc = big = 0;
	name[0] = 0;
	for(;;){
		nd = 0;
		c = chunk(layout, &pre, &len, &nd);
		if(skip(&s, layout, pre) < 0)
			goto Bad;
		if(c == 0){
			if(*s != 0){
				werrstr("parsing \"%s\" as \"%s\": extra text \"%s\"", s0, l0, s);
				return -1;
			}
			break;
		}
		layout += pre+len;
		x = 0;
		switch(c){
		case Year:
			if(!isdig(s[0]) || !isdig(s[1]))
				goto Bad;
			year = (s[0]-'0')*10 + s[1]-'0';
			year += year >= 69 ? 1900 : 2000;
			s += 2;
			break;
		case Lyear:
			if(!isdig(s[0]) || !isdig(s[1]) || !isdig(s[2]) || !isdig(s[3]))
				goto Bad;
			year = ((s[0]-'0')*10 + s[1]-'0')*100 + (s[2]-'0')*10 + s[3]-'0';
			s += 4;
			break;
		case Smonth:
			if((mon = lookup(&s, lmonth, 12, 3)) < 0)
				goto Bad;
			break;
		case Lmonth:
			if((mon = lookup(&s, lmonth, 12, 0)) < 0)
				goto Bad;
			break;
		case Nmonth:
		case Zmonth:
			if(getnum(&s, c == Zmonth, &mon) < 0)
				goto Bad;
			if(mon <= 0 || mon > 12)
				goto Range;
			mon--;
			break;
		case Swday:
			if(lookup(&s, lwday, 7, 3) < 0)
				goto Bad;
			break;
		case Lwday:
			if(lookup(&s, lwday, 7, 0) < 0)
				goto Bad;
			break;
		case Day:
		case Uday:
		case Zday:
			if(c == Uday && *s == ' ')
				s++;
			if(getnum(&s, c == Zday, &day) < 0)
				goto Bad;
			break;
		case Uyday:
		case Zyday:
			if(c == Uyday && *s == ' ')
				s++;
			if(c == Uyday && *s == ' ')
				s++;
			if(getnum3(&s, c == Zyday, &yday) < 0)
				goto Bad;
			break;
		case Hour:
			if(getnum(&s, 0, &hour) < 0)
				goto Bad;
			if(hour >= 24)
				goto Range;
			break;
		case H12:
		case Z12:
			if(getnum(&s, c == Z12, &hour) < 0)
				goto Bad;
			if(hour > 12)
				goto Range;
			break;
		case Min:
		case Zmin:
			if(getnum(&s, c == Zmin, &min) < 0)
				goto Bad;
			if(min >= 60)
				goto Range;
			break;
		case Sec:
		case Zsec:
			if(getnum(&s, c == Zsec, &sec) < 0)
				goto Bad;
			if(sec >= 60)
				goto Range;
			if((s[0] == '.' || s[0] == ',') && isdig(s[1])){
				x = chunk(layout, &pre, &len, &nd);
				if(pre == 0 && (x == Frac0 || x == Frac9))
					break;
				for(s++; isdig(*s); s++)
					;
			}
			break;
		case PM:
		case Pm:
			if(strncmp(s, c == PM ? "PM" : "pm", 2) == 0)
				pm = 1;
			else if(strncmp(s, c == PM ? "AM" : "am", 2) == 0)
				am = 1;
			else
				goto Bad;
			s += 2;
			break;
		case TZ:
			if(strncmp(s, "UTC", 3) == 0){
				utc = 1;
				s += 3;
				break;
			}
			if((s[0] == '+' || s[0] == '-') && isdig(s[1]) && isdig(s[2]) && isdig(s[3]) && isdig(s[4]) && !isdig(s[5]) || (m = zonelen(s)) == 0){
				if(getzone(&s, Num, &x) < 0)
					goto Bad;
				off = x;
				break;
			}
			if(m < sizeof tm->zone)
				snprint(name, sizeof name, "%.*s", m, s);
			else
				big = 1;
			s += m;
			break;
		case Frac0:
			if(strlen(s) < nd+1 || frac(&s, nd+1) < 0)
				goto Bad;
			break;
		case Frac9:
			if((s[0] == '.' || s[0] == ',') && isdig(s[1]))
				for(s++; isdig(*s); s++)
					;
			break;
		default:
			if(c >= Iso && c <= Isocolonsec && *s == 'Z'){
				utc = 1;
				s++;
				break;
			}
			if(getzone(&s, c, &x) < 0)
				goto Bad;
			off = x;
			break;
		}
	}
	if(pm && hour < 12)
		hour += 12;
	else if(am && hour == 12)
		hour = 0;
	if(yday >= 0){
		if(yday < 1 || yday > 365 + leap(year)){
			werrstr("parsing \"%s\": day of the year out of range", s0);
			return -1;
		}
		for(m = 0; yday > monthdays(m, year); m++)
			yday -= monthdays(m, year);
		if(mon >= 0 && mon != m || day >= 0 && day != yday){
			werrstr("parsing \"%s\": day of the year does not match the date", s0);
			return -1;
		}
		mon = m;
		day = yday;
	}
	if(mon < 0)
		mon = 0;
	if(day < 0)
		day = 1;
	if(day < 1 || day > monthdays(mon, year)){
		werrstr("parsing \"%s\": day out of range", s0);
		return -1;
	}
	memset(tm, 0, sizeof *tm);
	tm->year = year - 1900;
	tm->mon = mon;
	tm->mday = day;
	tm->hour = hour;
	tm->min = min;
	tm->sec = sec;
	if(utc){
		strcpy(tm->zone, "UTC");
		tm->tzoff = 0;
	}else if(off != -1){
		strcpy(tm->zone, name);
		tm->tzoff = off;
	}else if(name[0] || big){
		strcpy(tm->zone, name);
		tm->tzoff = 0;
		t = tmsec(tm);
		if(t == (long)t){
			lt = localtime(t);
			if(name[0] && strcmp(lt->zone, name) == 0)
				tm->tzoff = lt->tzoff;
		}
	}else
		strcpy(tm->zone, "GMT");
	t = days(year, mon, day);
	tm->wday = ((t%7) + 11) % 7;
	tm->yday = t - days(year, 0, 1);
	return 0;
Range:
	werrstr("parsing \"%s\" as \"%s\": %.*s out of range", s0, l0, len, layout-len);
	return -1;
Bad:
	werrstr("parsing \"%s\" as \"%s\": cannot parse \"%s\"", s0, l0, s);
	return -1;
}
