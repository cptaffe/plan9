#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

static int
at(uchar *s, int n, int i)
{
	return i < n ? s[i] : -1;
}

static int
alpha(int c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

static int
alnum(int c)
{
	return alpha(c) || c >= '0' && c <= '9';
}

static int
spc(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int
lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c+'a'-'A' : c;
}

static int
word(uchar *s, int n, int i, char *w)
{
	int k;

	for(k = 0; w[k]; k++)
		if(i+k >= n || lower(s[i+k]) != w[k])
			return 0;
	return k;
}

static int
tagname(uchar *s, int n, int i)
{
	int j;

	if(!alpha(at(s, n, i)))
		return 0;
	for(j = i+1; j < n && (alnum(s[j]) || s[j] == '-'); j++)
		;
	return j-i;
}

static int
attr(uchar *s, int n, int i)
{
	int j, k, c, q;

	j = i;
	while(j < n && spc(s[j]))
		j++;
	if(j == i)
		return 0;
	c = at(s, n, j);
	if(!(alpha(c) || c == '_' || c == ':'))
		return 0;
	for(j++; j < n && (alnum(s[j]) || s[j] == ':' || s[j] == '.' || s[j] == '_' || s[j] == '-'); j++)
		;
	k = j;
	while(k < n && spc(s[k]))
		k++;
	if(at(s, n, k) != '=')
		return j-i;
	k++;
	while(k < n && spc(s[k]))
		k++;
	c = at(s, n, k);
	if(c == '\'' || c == '"'){
		q = c;
		for(k++; k < n && s[k] != q; k++)
			;
		if(k >= n)
			return j-i;
		return k+1-i;
	}
	if(c < 0 || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' ||
	   c == '"' || c == '\'' || c == '=' || c == '<' || c == '>' || c == '`')
		return j-i;
	for(; k < n; k++){
		c = s[k];
		if(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' ||
		   c == '"' || c == '\'' || c == '=' || c == '<' || c == '>' || c == '`')
			break;
	}
	return k-i;
}

int
_mdscanhtag(uchar *s, int n, int i)
{
	int j, k;

	j = i;
	if(at(s, n, j) == '/'){
		k = tagname(s, n, j+1);
		if(k == 0)
			return 0;
		j += 1+k;
		while(j < n && spc(s[j]))
			j++;
		if(at(s, n, j) != '>')
			return 0;
		return j+1-i;
	}
	k = tagname(s, n, j);
	if(k == 0)
		return 0;
	j += k;
	while((k = attr(s, n, j)) > 0)
		j += k;
	while(j < n && spc(s[j]))
		j++;
	if(at(s, n, j) == '/')
		j++;
	if(at(s, n, j) != '>')
		return 0;
	return j+1-i;
}

static int
scheme(uchar *s, int n, int i)
{
	int j;

	if(!alpha(at(s, n, i)))
		return 0;
	for(j = i+1; j < n && j-i <= 32 && (alnum(s[j]) || s[j] == '.' || s[j] == '+' || s[j] == '-'); j++)
		;
	if(j-i < 2 || j-i > 32)
		return 0;
	return j-i;
}

int
_mdscanauri(uchar *s, int n, int i)
{
	int j, k, c;

	k = scheme(s, n, i);
	if(k == 0)
		return 0;
	j = i+k;
	if(at(s, n, j) != ':')
		return 0;
	for(j++; j < n; j++){
		c = s[j];
		if(c == '>')
			return j+1-i;
		if(c <= 0x20 || c == '<')
			return 0;
	}
	return 0;
}

int
_mdscanamail(uchar *s, int n, int i)
{
	int j, k, c, l;

	for(j = i; j < n; j++){
		c = s[j];
		if(!(alnum(c) || strchr(".!#$%&'*+/=?^_`{|}~-", c) != nil && c != 0))
			break;
	}
	if(j == i || at(s, n, j) != '@')
		return 0;
	j++;
	for(;;){
		if(!alnum(at(s, n, j)))
			return 0;
		l = j;
		for(k = j; k < n && k-l < 63 && (alnum(s[k]) || s[k] == '-'); k++)
			;
		while(k > j+1 && s[k-1] == '-')
			k--;
		if(s[k-1] == '-')
			return 0;
		j = k;
		c = at(s, n, j);
		if(c == '>')
			return j+1-i;
		if(c != '.')
			return 0;
		j++;
	}
}

int
_mdscantitle(uchar *s, int n, int i)
{
	int j, q;

	q = at(s, n, i);
	if(q == '(')
		q = ')';
	else if(q != '"' && q != '\'')
		return 0;
	for(j = i+1; j < n; j++){
		if(s[j] == '\\' && j+1 < n && _mdispunct(s[j+1]))
			j++;
		else if(s[j] == q)
			return j+1-i;
		else if(q == ')' && s[j] == '(')
			break;
	}
	return 0;
}

int
_mdscanspace(uchar *s, int n, int i)
{
	int j;

	for(j = i; j < n && spc(s[j]); j++)
		;
	return j-i;
}

int
_mdscancomment(uchar *s, int n, int i)
{
	int j, st;

	if(at(s, n, i) != '-' || at(s, n, i+1) != '-')
		return 0;
	st = 0;
	for(j = i+2; j < n; j++){
		switch(st){
		case 0:
			st = s[j] == '-' ? 1 : 0;
			break;
		case 1:
			st = s[j] == '-' ? 2 : 0;
			break;
		case 2:
			if(s[j] == '>')
				return j+1-i;
			st = 0;
			break;
		}
	}
	return 0;
}

int
_mdscancdata(uchar *s, int n, int i)
{
	int j, st;

	if(word(s, n, i, "cdata[") == 0 || s[i] != 'C' || s[i+1] != 'D' || s[i+2] != 'A' || s[i+3] != 'T' || s[i+4] != 'A')
		return 0;
	st = 0;
	for(j = i+6; j < n; j++){
		switch(st){
		case 0:
			st = s[j] == ']' ? 1 : 0;
			break;
		case 1:
			st = s[j] == ']' ? 2 : 0;
			break;
		case 2:
			if(s[j] == '>')
				return j-2-i;
			st = 0;
			break;
		}
	}
	return j-i;
}

int
_mdscanatx(uchar *s, int n, int i)
{
	int j;

	for(j = i; j < n && s[j] == '#' && j-i < 7; j++)
		;
	if(j == i || j-i > 6)
		return 0;
	if(at(s, n, j) == '\r' || at(s, n, j) == '\n')
		return j+1-i;
	if(at(s, n, j) != ' ' && at(s, n, j) != '\t')
		return 0;
	while(j < n && (s[j] == ' ' || s[j] == '\t'))
		j++;
	return j-i;
}

int
_mdscansetext(uchar *s, int n, int i)
{
	int j, c;

	c = at(s, n, i);
	if(c != '=' && c != '-')
		return 0;
	for(j = i; j < n && s[j] == c; j++)
		;
	while(j < n && (s[j] == ' ' || s[j] == '\t'))
		j++;
	if(at(s, n, j) != '\r' && at(s, n, j) != '\n')
		return 0;
	return c == '=' ? 1 : 2;
}

int
_mdscanopenfence(uchar *s, int n, int i)
{
	int j, k, c;

	c = at(s, n, i);
	if(c != '`' && c != '~')
		return 0;
	for(j = i; j < n && s[j] == c; j++)
		;
	if(j-i < 3)
		return 0;
	for(k = j; k < n && s[k] != '\r' && s[k] != '\n'; k++)
		if(c == '`' && s[k] == '`')
			return 0;
	if(k >= n)
		return 0;
	return j-i;
}

int
_mdscanclosefence(uchar *s, int n, int i)
{
	int j, k, c;

	c = at(s, n, i);
	if(c != '`' && c != '~')
		return 0;
	for(j = i; j < n && s[j] == c; j++)
		;
	if(j-i < 3)
		return 0;
	for(k = j; k < n && (s[k] == ' ' || s[k] == '\t'); k++)
		;
	if(at(s, n, k) != '\r' && at(s, n, k) != '\n')
		return 0;
	return j-i;
}

static char *blocktags[] = {
	"address", "article", "aside", "base", "basefont", "blockquote", "body", "caption", "center",
	"col", "colgroup", "dd", "details", "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption",
	"figure", "footer", "form", "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head",
	"header", "hr", "html", "iframe", "legend", "li", "link", "main", "menu", "menuitem", "nav",
	"noframes", "ol", "optgroup", "option", "p", "param", "section", "search", "title", "summary",
	"table", "tbody", "td", "tfoot", "th", "thead", "tr", "track", "ul", nil,
};

int
_mdscanhstart(uchar *s, int n, int i)
{
	int j, k, c;
	char **t;
	static char *t1[] = {"script", "pre", "textarea", "style", nil};

	if(at(s, n, i) != '<')
		return 0;
	j = i+1;
	for(t = t1; *t; t++)
		if((k = word(s, n, j, *t)) > 0){
			c = at(s, n, j+k);
			if(c >= 0 && (spc(c) || c == '>'))
				return 1;
		}
	if(at(s, n, j) == '!' && at(s, n, j+1) == '-' && at(s, n, j+2) == '-')
		return 2;
	if(at(s, n, j) == '?')
		return 3;
	if(at(s, n, j) == '!' && alpha(at(s, n, j+1)))
		return 4;
	if(at(s, n, j) == '!' && j+8 <= n && memcmp(s+j, "![CDATA[", 8) == 0)
		return 5;
	if(at(s, n, j) == '/')
		j++;
	for(t = blocktags; *t; t++)
		if((k = word(s, n, j, *t)) > 0){
			c = at(s, n, j+k);
			if(c >= 0 && (spc(c) || c == '>' || c == '/' && at(s, n, j+k+1) == '>'))
				return 6;
		}
	return 0;
}

int
_mdscanhstart7(uchar *s, int n, int i)
{
	int j, k;

	if(at(s, n, i) != '<')
		return 0;
	k = _mdscanhtag(s, n, i+1);
	if(k == 0)
		return 0;
	for(j = i+1+k; j < n && (s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\f'); j++)
		if(s[j] == '\n')
			return 7;
	if(at(s, n, j) == '\r' || at(s, n, j) == '\n')
		return 7;
	return 0;
}

int
_mdscanhend(uchar *s, int n, int i, int type)
{
	int j, k;
	char **t;
	static char *t1[] = {"script", "pre", "textarea", "style", nil};

	for(j = i; j < n && s[j] != '\n'; j++){
		switch(type){
		case 1:
			if(s[j] == '<' && at(s, n, j+1) == '/')
				for(t = t1; *t; t++)
					if((k = word(s, n, j+2, *t)) > 0 && at(s, n, j+2+k) == '>')
						return 1;
			break;
		case 2:
			if(s[j] == '-' && at(s, n, j+1) == '-' && at(s, n, j+2) == '>')
				return 1;
			break;
		case 3:
			if(s[j] == '?' && at(s, n, j+1) == '>')
				return 1;
			break;
		case 4:
			if(s[j] == '>')
				return 1;
			break;
		case 5:
			if(s[j] == ']' && at(s, n, j+1) == ']' && at(s, n, j+2) == '>')
				return 1;
			break;
		}
	}
	return 0;
}

static int
tspc(int c)
{
	return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

static int
tnl(uchar *s, int n, int j)
{
	if(at(s, n, j) == '\n')
		return 1;
	if(at(s, n, j) == '\r' && at(s, n, j+1) == '\n')
		return 2;
	return 0;
}

static int
tmarker(uchar *s, int n, int j)
{
	int k;

	k = j;
	while(k < n && tspc(s[k]))
		k++;
	if(at(s, n, k) == ':')
		k++;
	if(at(s, n, k) != '-')
		return -1;
	while(k < n && s[k] == '-')
		k++;
	if(at(s, n, k) == ':')
		k++;
	while(k < n && tspc(s[k]))
		k++;
	return k;
}

int
_mdscantstart(uchar *s, int n, int i)
{
	int j, k;

	j = i;
	if(at(s, n, j) == '|')
		j++;
	j = tmarker(s, n, j);
	if(j < 0)
		return 0;
	for(;;){
		if(at(s, n, j) != '|')
			break;
		k = tmarker(s, n, j+1);
		if(k < 0){
			j++;
			break;
		}
		j = k;
	}
	while(j < n && tspc(s[j]))
		j++;
	k = tnl(s, n, j);
	if(k == 0)
		return 0;
	return j+k-i;
}

int
_mdscantcell(uchar *s, int n, int i)
{
	int j;

	for(j = i; j < n; ){
		if(s[j] == '\\' && j+1 < n && (_mdispunct(s[j+1]) && s[j+1] != 0)){
			j += 2;
			continue;
		}
		if(s[j] == '|' || s[j] == '\r' || s[j] == '\n')
			break;
		j++;
	}
	return j-i;
}

int
_mdscantcellend(uchar *s, int n, int i)
{
	int j;

	if(at(s, n, i) != '|')
		return 0;
	for(j = i+1; j < n && tspc(s[j]); j++)
		;
	return j-i;
}

int
_mdscantrowend(uchar *s, int n, int i)
{
	int j, k;

	for(j = i; j < n && tspc(s[j]); j++)
		;
	k = tnl(s, n, j);
	if(k == 0)
		return 0;
	return j+k-i;
}

int
_mdscantask(uchar *s, int n, int i)
{
	int j;

	if(i+3 > n || s[i] != '[' || s[i+2] != ']')
		return 0;
	if(!tspc(s[i+1]) && s[i+1] != 'x' && s[i+1] != 'X')
		return 0;
	for(j = i+3; j < n && tspc(s[j]); j++)
		;
	if(j == i+3)
		return 0;
	return j-i;
}
