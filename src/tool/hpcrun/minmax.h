#ifndef MINMAX_H
#define MINMAX_H

# undef MIN
# undef MAX
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
# define MAX(a,b) (((a) > (b)) ? (a) : (b))

#endif // MINMAX_H
