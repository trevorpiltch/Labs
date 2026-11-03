#include <stdio.h>
#include <stdlib.h>

typedef struct node_t {
  int value;
  struct node_t *next;
} node_t;

node_t *node_init(int value, node_t *next) {
  node_t *new_node = malloc(sizeof(node_t));
  new_node->value = value;
  new_node->next = next;
  return new_node;
}

int has_cycle(node_t *head) {
  node_t *slow = head;
  node_t *fast = head;

  while (slow && fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;

    if (slow == fast) {
      return 1;
    }
  }

  return 0;
}

int main(void) {
  node_t *head = node_init(0, NULL);
  node_t *second = node_init(1, NULL);
  node_t *third = node_init(2, NULL);

  head->next = second;
  second->next = third;
  third->next = head;

  if (has_cycle(head) == 1) {
    printf("[SUCCESS]\n");
  }
  else {
    printf("[FAIL]\n");
  }

  free(head);
  free(second);
  free(third);

  return 0;
}
