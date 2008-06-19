unsigned long last_sample_addr = 0; 
unsigned long raw = 0; 

void csprof_set_last_sample_addr(long addr)
{
	last_sample_addr = addr;
}

unsigned long csprof_get_last_sample_addr()
{
	return last_sample_addr;
}

void csprof_increment_raw_sample_count()
{
	raw++;
}

unsigned int csprof_get_raw_sample_count()
{
	return raw;
}

