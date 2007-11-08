#define MARKER 0xdeadbeef

#if 0
int markrear[] = {
	MARKER, MARKER, MARKER, MARKER
};
#else
int markrear[4];
#endif

void setmarkrear() { 
	int i;
	for (i = 0; i < 4; i++) markrear[i] = MARKER;
}

int markrearok() 
{
	int i;
	int ret = (1 == 1);
	for (i = 0; i < 4; i++) ret &= (markrear[i] == MARKER);
	return ret;
}
