#ifndef FIND_H
#define FIND_H
#ifdef __cplusplus
extern "C" {
#endif

int find_enclosing_function_bounds_v(char *addr, char **start, char **end, int verb);
void test_find_enclosing_function_bounds(char *addr);

#ifdef __cplusplus
}
#endif
#define NOISY 1
#define SILENT 0
#define find_enclosing_function_bounds(a,s,e) find_enclosing_function_bounds_v(a,s,e,NOISY)
#else
#endif
