/* wasm has no counter of cycles. */
void
_cycles(unsigned long long *t)
{
	*t = 0;
}
