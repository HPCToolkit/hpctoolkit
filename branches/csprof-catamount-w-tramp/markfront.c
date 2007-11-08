#define MARKER 0xdeadbeef

#if 1
static int markfront[] = {
	MARKER, MARKER, MARKER, MARKER
};
#else
int markfront[4];
#endif

void setmarkfront() { 
	int i;
	for (i = 0; i < 4; i++) markfront[i] = MARKER;
}

int markfrontok() 
{
	int i;
	int ret = (1 == 1);
	for (i = 0; i < 4; i++) ret &= (markfront[i] == MARKER);
	return ret;
}
