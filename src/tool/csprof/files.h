#ifndef files_h
#define files_h

void files_set_directory();                   // location from environment variable
void files_set_executable(char *execname);   


void files_trace_name(char *filename, int len);
void files_profile_name(char *filename, int len);
void files_log_name(char *filename, int len);

const char *files_executable_name();

#endif // files_h




