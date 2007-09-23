/*
 * Test program for splay code.
 * Of course, this doesn't go into csprof.
 */

#include <stdio.h>
#include <stdlib.h>
#include "find.h"
#include "intervals.h"
#include "splay.h"

struct fcn_data {
    unsigned long addr;
    int is_fcn;
};

static struct fcn_data fd[] = {
    {   0, 1 },
    {  50, 1 },
    { 100, 1 },
    { 120, 0 },
    { 130, 0 },
    { 170, 0 },
    { 200, 1 },
    { 250, 0 },
    { 280, 0 },
    { 300, 1 },
    { 350, 0 },
    { 400, 1 },
    { 500, 1 },
    { 999, 2 },
};
#define MAX  (sizeof(fd)/sizeof(struct fcn_data))

static int
begin_fcn_index(char *addr)
{
    unsigned long ad = (unsigned long)addr;
    int n;

    for (n = MAX - 1;; n--) {
	if (fd[n].addr <= ad && fd[n].is_fcn)
	    return (n);
    }
}

static int
end_fcn_index(char *addr)
{
    unsigned long ad = (unsigned long)addr;
    int n;

    for (n = 0;; n++) {
	if (ad < fd[n].addr && fd[n].is_fcn)
	    return (n);
    }
}

int
find_enclosing_function_bounds(char *addr, char **start, char **end)
{
    *start = (char *)fd[begin_fcn_index(addr)].addr;
    *end = (char *)fd[end_fcn_index(addr)].addr;

    printf("%s: addr = %d, start = %d, end = %d\n",
	   __func__, (int)addr, (int)*start, (int)*end);

    return (0);
}

interval_status
l_build_intervals(char *addr, unsigned int len)
{
    interval_status istat;
    unwind_interval *head, *p;
    int start, end, n;

    start = begin_fcn_index(addr);
    end = end_fcn_index(addr);

    printf("%s: addr = %d, start = %d, end = %d\n",
	   __func__, (int)addr, (int)start, (int)end);

    head = NULL;
    for (n = end - 1; n >= start; n--) {
	p = malloc(sizeof(unwind_interval));
	p->startaddr = fd[n].addr;
	p->endaddr = fd[n + 1].addr;
	p->next = head;
	head = p;
	
	printf("interval node: n = %d, start = %d, end = %d\n",
	       n, (int)p->startaddr, (int)p->endaddr);
    }

    istat.first = head;

    return (istat);
}

void
print_tree(interval_tree_node_t root)
{
    if (root == NULL)
	return;

    print_tree(root->left);
    printf("[%d,%d)  ", (int)root->start, (int)root->end);
    print_tree(root->right);
}

void
search_addr(unsigned long addr)
{
    unwind_interval *ui;

    printf("\n==> addr = %lu\n", addr);
    ui = csprof_addr_to_interval(addr);
    ui = csprof_addr_to_interval(addr);
    printf("==> addr = %lu, start = %lu, end = %lu\n",
	   addr, ui->startaddr, ui->endaddr);
    print_tree((interval_tree_node_t)ui);
    printf("\n");
}

int
main(int argc, char **argv)
{
    unwind_interval *ui;
    unsigned long addr;

    csprof_interval_tree_init();

    for (addr = 10; addr < 450; addr += 10) {
	search_addr(addr);
    }
    printf("--------------------------------------------------\n");

    for (addr = 10; addr < 450; addr += 1) {
	search_addr(addr);
    }

    return (0);
}
