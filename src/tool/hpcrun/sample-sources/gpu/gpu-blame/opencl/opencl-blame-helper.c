//******************************************************************************
// system include
//******************************************************************************

#include <stdbool.h>												// bool



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>    // hpcrun_malloc
#include <lib/prof-lean/splay-uint64.h>     // splay_visit_t 
#include <lib/prof-lean/stdatomic.h>        // atomic_fetch_add

#include "opencl-event-map.h"               // event_node_t
#include "opencl-active-kernels-map.h"      // active_kernels_*



//******************************************************************************
// macros
//******************************************************************************

#define NEXT(node) node->next
#define SYNC -1



//******************************************************************************
// type definitions
//******************************************************************************

typedef struct Node {
  long time;
  long id;
  bool isStart;
  event_node_t *event_node;
  struct Node *next;
} Node;


typedef struct ak_helper_node {
  long current_time;
  long last_time;
  long ak_size;
} ak_helper_node;



//******************************************************************************
// local data
//******************************************************************************

static Node *node_free_list = NULL;



//******************************************************************************
// linkedlist sort functions: https://www.geeksforgeeks.org/quicksort-on-singly-linked-list/
//******************************************************************************

/* A utility function to print linked list */
static void
printList
(
 struct Node* node
)
{
  while (node != NULL) {
    printf("%ld ", node->time);
    node = node->next;
  }
  printf("\n");
}


static Node*
node_alloc_helper
(
 Node **free_list
)
{
  Node *first = *free_list; 

  if (first) { 
    *free_list = NEXT(first);
  } else {
    first = (Node *) hpcrun_malloc_safe(sizeof(Node));
  }

  memset(first, 0, sizeof(Node));
  return first;
}


static void
node_free_helper
(
 Node **free_list, 
 Node *node 
)
{
  NEXT(node) = *free_list;
  *free_list = node;
}


/* A utility function to insert a node at the beginning of
 * linked list */
static void
push
(
 struct Node** head_ref,
 Node new_data
)
{
  /* allocate node */
  // struct Node *new_node = (Node*)hpcrun_malloc(sizeof(Node));
  struct Node *new_node = node_alloc_helper(&node_free_list);

  /* put in the data */
  *new_node = new_data;

  /* link the old list off the new node */
  new_node->next = (*head_ref);

  /* move the head to point to the new node */
  (*head_ref) = new_node;
}


// Returns the last node of the list
static struct Node*
getTail
(
 struct Node* cur
)
{
  while (cur != NULL && cur->next != NULL)
    cur = cur->next;
  return cur;
}


// Partitions the list taking the last element as the pivot
static struct Node*
partition
(
 struct Node* head, struct Node* end,
 struct Node** newHead,
 struct Node** newEnd
)
{
  struct Node* pivot = end;
  struct Node *prev = NULL, *cur = head, *tail = pivot;

  // During partition, both the head and end of the list
  // might change which is updated in the newHead and
  // newEnd variables
  while (cur != pivot) {
    if (cur->time < pivot->time) {
      // First node that has a value less than the
      // pivot - becomes the new head
      if ((*newHead) == NULL)
        (*newHead) = cur;

      prev = cur;
      cur = cur->next;
    }
    else // If cur node is greater than pivot
    {
      // Move cur node to next of tail, and change
      // tail
      if (prev)
        prev->next = cur->next;
      struct Node* tmp = cur->next;
      cur->next = NULL;
      tail->next = cur;
      tail = cur;
      cur = tmp;
    }
  }

  // If the pivot data is the smallest element in the
  // current list, pivot becomes the head
  if ((*newHead) == NULL)
    (*newHead) = pivot;

  // Update newEnd to the current last node
  (*newEnd) = tail;

  // Return the pivot node
  return pivot;
}


// here the sorting happens exclusive of the end node
static struct Node*
quickSortRecur
(
 struct Node* head,
 struct Node* end
)
{
  // base condition
  if (!head || head == end)
    return head;

  Node *newHead = NULL, *newEnd = NULL;

  // Partition the list, newHead and newEnd will be
  // updated by the partition function
  struct Node* pivot
    = partition(head, end, &newHead, &newEnd);

  // If pivot is the smallest element - no need to recur
  // for the left part.
  if (newHead != pivot) {
    // Set the node before the pivot node as NULL
    struct Node* tmp = newHead;
    while (tmp->next != pivot)
      tmp = tmp->next;
    tmp->next = NULL;

    // Recur for the list before pivot
    newHead = quickSortRecur(newHead, tmp);

    // Change next of last node of the left half to
    // pivot
    tmp = getTail(newHead);
    tmp->next = pivot;
  }

  // Recur for the list after the pivot element
  pivot->next = quickSortRecur(pivot->next, newEnd);

  return newHead;
}


// The main function for quick sort. This is a wrapper over
// recursive function quickSortRecur()
static void
quickSort
(
 struct Node** headRef
)
{
  (*headRef)
    = quickSortRecur(*headRef, getTail(*headRef));
  return;
}



//******************************************************************************
// private functions 
//******************************************************************************

void __attribute__((unused))
increment_blame_for_active_kernel
(
 active_kernels_entry_t *entry,
 splay_visit_t visit_type,
 void *arg
)
{
  ak_helper_node *akn = (ak_helper_node*) arg;
  long current_time = akn->current_time;
  long last_time = akn->last_time;
  long active_kernels_size = akn->ak_size;
  double new_blame = (double)(current_time - last_time) / active_kernels_size;
  increment_blame_for_entry(entry, new_blame);
  printf("you can expect cpu_idle_cause_metric_id\n");
}


static void
distribute_blame_to_kernels
(
 Node *kernel_nodes
)
{
  // kernel_nodes are sorted set by first value (x's)
  // kernel_nodes : (x11, A1, start), (x12, A1, end), (x21, A2, start), (x22, A2, end), ........, (x01, B, start), (x02, B, end);
  bool inSync = false;
  Node *data = kernel_nodes;
  long last_time;

  while (data) {
    if (data->id == SYNC) {
      if (data->isStart) {
        inSync = true;
        last_time = data->time;
        data = data->next;
        continue;
      }	else {
        ak_helper_node akn = {data->time, last_time, active_kernels_size()};
        active_kernels_forall(splay_inorder, increment_blame_for_active_kernel, &akn);
        data = data->next;
        break;
      }
    }

    if (data->isStart) {
      if (inSync) {
        ak_helper_node akn = {data->time, last_time, active_kernels_size()};
        active_kernels_forall(splay_inorder, increment_blame_for_active_kernel, &akn);
        last_time = data->time;
      }
      active_kernels_insert(data->id, data->event_node);
    } else {
      if (inSync) {
        ak_helper_node akn = {data->time, last_time, active_kernels_size()};
        active_kernels_forall(splay_inorder, increment_blame_for_active_kernel, &akn);
        last_time = data->time;
      }
      active_kernels_delete(data->id);
    }
    data = data->next;
  }
  ak_map_clear();
}


static Node*
transform_event_nodes_to_sortable_nodes
(
 event_node_t *event_node_head
)
{
  struct Node *head = NULL;

  // convert event_node_t nodes to sortable Nodes
  event_node_t *curr = event_node_head;
  while (curr) {
    Node start_node = {curr->event_start_time, (long)curr->event, 1, curr, NULL};
    Node end_node = {curr->event_end_time, (long)curr->event, 0, curr, NULL};
    push(&head, start_node);
    push(&head, end_node);
    curr = atomic_load(&curr->next);
  }
  return head;
}


static void
free_all_sortable_nodes
(
 Node *head
)
{
  Node *curr = head;
  while (curr) {
    node_free_helper(&node_free_list, curr);
    curr = curr->next;	
  }
}



//******************************************************************************
// interface functions 
//******************************************************************************

void
calculate_blame_for_active_kernels
(
 event_node_t *event_list,
 long sync_start,
 long sync_end
)
{
  // also input the sync times and add nodes	
  Node *head = transform_event_nodes_to_sortable_nodes(event_list);

  Node sync_start_node = {sync_start, SYNC, 1, NULL, NULL};
  Node sync_end_node = {sync_end, SYNC, 0, NULL, NULL};
  push(&head, sync_start_node);
  push(&head, sync_end_node);

  quickSort(&head);

  distribute_blame_to_kernels(head);
  free_all_sortable_nodes(head);
}

