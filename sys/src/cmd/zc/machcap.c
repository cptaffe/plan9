#include "gc.h"

int
machcap(Node *n)
{
	if(n == Z)
		return 1;
	switch(n->op) {
	case ONEG:
		return n->left != Z && n->left->type != T && typefd[n->left->type->etype];
	case OCOM:
		return 0;
	}
	return 1;
}
