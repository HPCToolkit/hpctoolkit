
//******************************************************************************
// system includes
//******************************************************************************

#include <stdio.h> 
#include <stdlib.h> 



//******************************************************************************
// local includes
//******************************************************************************

#include "pointer-alias-check-helper.h"



//******************************************************************************
// type definitions
//******************************************************************************

/* Link list node */
typedef struct Node { 
  uint64_t mem;
  bool isStart; 
  struct Node* next; 
} Node; 



//******************************************************************************
// local data
//******************************************************************************

static Node *node_free_list = NULL;



//******************************************************************************
// linkedlist merge-sort functions: https://www.geeksforgeeks.org/merge-sort-for-linked-list/
//******************************************************************************

static Node*
node_alloc_helper
(
 Node **free_list
)
{
  Node *first = *free_list; 

  if (first) { 
    *free_list = first->next;
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
  node->next = *free_list;
  *free_list = node;
}


/* function prototypes */
static struct Node* SortedMerge(struct Node* a, struct Node* b); 
static void FrontBackSplit(struct Node* source, 
    struct Node** frontRef, struct Node** backRef); 

/* sorts the linked list by changing next pointers (not data) */
static void MergeSort(struct Node** headRef) 
{ 
  struct Node* head = *headRef; 
  struct Node* a; 
  struct Node* b; 

  /* Base case -- length 0 or 1 */
  if ((head == NULL) || (head->next == NULL)) { 
    return; 
  } 

  /* Split head into 'a' and 'b' sublists */
  FrontBackSplit(head, &a, &b); 

  /* Recursively sort the sublists */
  MergeSort(&a); 
  MergeSort(&b); 

  /* answer = merge the two sorted lists together */
  *headRef = SortedMerge(a, b); 
} 

/* See https:// www.geeksforgeeks.org/?p=3622 for details of this  
   function */
static struct Node* SortedMerge(struct Node* a, struct Node* b) 
{ 
  struct Node* result = NULL; 

  /* Base cases */
  if (a == NULL) 
    return (b); 
  else if (b == NULL) 
    return (a); 

  /* Pick either a or b, and recur */
  if (a->mem <= b->mem) { 
    result = a; 
    result->next = SortedMerge(a->next, b); 
  } 
  else { 
    result = b; 
    result->next = SortedMerge(a, b->next); 
  } 
  return (result); 
} 

/* UTILITY FUNCTIONS */
/* Split the nodes of the given list into front and back halves, 
   and return the two lists using the reference parameters. 
   If the length is odd, the extra node should go in the front list. 
   Uses the fast/slow pointer strategy. */
static void FrontBackSplit(struct Node* source, 
    struct Node** frontRef, struct Node** backRef) 
{ 
  struct Node* fast; 
  struct Node* slow; 
  slow = source; 
  fast = source->next; 

  /* Advance 'fast' two nodes, and advance 'slow' one node */
  while (fast != NULL) { 
    fast = fast->next; 
    if (fast != NULL) { 
      slow = slow->next; 
      fast = fast->next; 
    } 
  } 

  /* 'slow' is before the midpoint in the list, so split it in two 
     at that point. */
  *frontRef = source; 
  *backRef = slow->next; 
  slow->next = NULL; 
} 

/* Function to print nodes in a given linked list */
static void printList(struct Node* node) 
{ 
  while (node != NULL) { 
    printf("%lu ", node->mem); 
    node = node->next; 
  }
  printf("\n"); 
} 

/* Function to insert a node at the beginging of the linked list */
static void push(struct Node** head_ref, Node new_data) 
{ 
  /* allocate node */
  struct Node* new_node = node_alloc_helper(&node_free_list); 

  /* put in the data */
  *new_node = new_data; 

  /* link the old list off the new node */
  new_node->next = (*head_ref); 

  /* move the head to point to the new node */
  (*head_ref) = new_node; 
} 



//******************************************************************************
// private functions 
//*****************************************************************************

static Node*
transformKernelParamNodesToSortableNodes
(
 kp_node_t *kernel_param_list
)
{
  /* Start with the empty list */
  struct Node* start_head = NULL; 
  struct Node* end_head = NULL; 
  
  // convert kp_node_t nodes to sortable Nodes
  kp_node_t *curr_k = kernel_param_list;
  while (curr_k) {
    uint64_t mem_start = (uint64_t)curr_k->mem;
    uint64_t mem_end = (uint64_t)curr_k->mem + curr_k->size;
    Node start_node = {mem_start, 1, NULL};
    Node end_node = {mem_end, 0, NULL};
    push(&start_head, start_node);
    push(&end_head, end_node);
    curr_k = curr_k->next;
  }

  // appending the end_head after the start_head list
  Node *curr_n = start_head;
  while (curr_n->next) {
    curr_n = curr_n->next;
  }
  curr_n->next = end_head;
  
  return start_head;
}


static void
free_all_sortable_nodes
(
 Node *head
)
{
  Node *curr = head;
  Node *next;
  while (curr) {
    next = curr->next;
    node_free_helper(&node_free_list, curr);
    curr = next;  
  }
}



//******************************************************************************
// interface functions
//******************************************************************************

bool
checkIfMemoryRegionsOverlap
(
 kp_node_t *kernel_param_list
) 
{
  Node *head = transformKernelParamNodesToSortableNodes(kernel_param_list);

  printf("Unsorted Linked List is: \n"); 
  printList(head);
 
  /* Sort the above created Linked List.
  we have a boundary case where if a2 == b1, (where a2 is end of region a and b1 is start of region b)
  we want our algorithm to consider it as an overlap. So, we need to ensure all start nodes are before end nodes in the input list
  and we have a stable sort algorithm that wont swap equal nodes */
  MergeSort(&head); 

  printf("Sorted Linked List is: \n"); 
  printList(head); 
  
  // check if there is overlap
  Node *curr = head;
  Node *next;
  bool overlap = false;

  while (curr && curr->next) {
    next = curr->next;
    if (curr->isStart && next->isStart) { // will this condition cover all overlap scenarios?
      overlap = true;
      break;
    }
    curr = next;
  }

  free_all_sortable_nodes(head);
  return overlap;
}

