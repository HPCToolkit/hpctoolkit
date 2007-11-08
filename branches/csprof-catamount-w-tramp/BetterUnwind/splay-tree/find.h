#ifdef __cplusplus
extern "C" {
#endif

int find_enclosing_function_bounds(char *addr, char **start, char **end);
void test_find_enclosing_function_bounds(char *addr);

#ifdef __cplusplus
}
#endif
