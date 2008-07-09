static int count    =  0;

void
simple_handler(int EventSet, void *pc, long long ovec, void *context)
{
  count += 1;
}

int
simple_papi_count(void)
{
  return count;
}
