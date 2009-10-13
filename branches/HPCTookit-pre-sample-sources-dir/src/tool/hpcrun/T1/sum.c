/*
 * A library that defines several functions that all churn cycles.
 * This is a way to create several intervals, so that sampling calls
 * dl_iterate_phdr() many times.
 *
 * Compile twice, with/without LIBSUM_TWO defined.
 *
 * $Id$
 */

#ifdef LIBSUM_TWO
#define MAKE_SUM(num)  SUM_HELP(2, num)
#else
#define MAKE_SUM(num)  SUM_HELP(1, num)
#endif

#define SUM_HELP(label, suffix)		\
double sum_ ## label ## _ ## suffix (long num) {  \
    double sum = 0.0;			\
    long k;				\
    for (k = 1; k <= num; k++) {	\
	sum += (double)k;		\
    }					\
    return (sum);			\
}

MAKE_SUM(0)
MAKE_SUM(1)
MAKE_SUM(2)
MAKE_SUM(3)
MAKE_SUM(4)
MAKE_SUM(5)
MAKE_SUM(6)
MAKE_SUM(7)
MAKE_SUM(8)
MAKE_SUM(9)
MAKE_SUM(10)
MAKE_SUM(11)
MAKE_SUM(12)
MAKE_SUM(13)
MAKE_SUM(14)
MAKE_SUM(15)
MAKE_SUM(16)
MAKE_SUM(17)
MAKE_SUM(18)
MAKE_SUM(19)
MAKE_SUM(20)
MAKE_SUM(21)
MAKE_SUM(22)
MAKE_SUM(23)
MAKE_SUM(24)
MAKE_SUM(25)
MAKE_SUM(26)
MAKE_SUM(27)
MAKE_SUM(28)
MAKE_SUM(29)
MAKE_SUM(30)
MAKE_SUM(31)
MAKE_SUM(32)
MAKE_SUM(33)
MAKE_SUM(34)
MAKE_SUM(35)
MAKE_SUM(36)
MAKE_SUM(37)
MAKE_SUM(38)
MAKE_SUM(39)
MAKE_SUM(40)
MAKE_SUM(41)
MAKE_SUM(42)
MAKE_SUM(43)
MAKE_SUM(44)
MAKE_SUM(45)
MAKE_SUM(46)
MAKE_SUM(47)
MAKE_SUM(48)
MAKE_SUM(49)
MAKE_SUM(50)
MAKE_SUM(51)
MAKE_SUM(52)
MAKE_SUM(53)
MAKE_SUM(54)
MAKE_SUM(55)
