#ifndef trace_h
#define trace_h

void trace_init();
void trace_open();
void trace_append(unsigned int cpid);
void trace_close();

int trace_isactive();

#endif // trace_h
