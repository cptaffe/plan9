/* The decoder's state, which json.h does not show. */
struct Jsondec
{
	Biobuf	*b;
	uchar	*p, *ep;	/* or the bytes left of a string */
	int	peek;	/* the byte read ahead, or -1 */
	vlong	off;	/* of the next byte */
	char	*buf;	/* the token's text */
	long	nbuf;
	uchar	*stk;	/* the open containers: '[' or '{' */
	int	nstk, astk;
	int	state;
	char	*rec;	/* a raw value's text, while it is read */
	long	nrec, arec;
	int	recording;
	char	err[ERRMAX];	/* the first error, which every call after gives */
};

int	_jsonerr(Jsondec*, char*, ...);
#pragma	varargck	argpos	_jsonerr	2
