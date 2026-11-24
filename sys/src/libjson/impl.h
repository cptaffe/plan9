struct Jsondec
{
	Biobuf	*b;
	uchar	*p, *ep;
	int	peek;
	vlong	off;
	char	*buf;
	long	nbuf;
	uchar	*stk;
	int	nstk, astk;
	int	state;
	char	*rec;
	long	nrec, arec;
	int	recording;
	char	err[ERRMAX];
};

int	_jsonerr(Jsondec*, char*, ...);
#pragma	varargck	argpos	_jsonerr	2
