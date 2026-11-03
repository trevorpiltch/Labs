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

node_t *reverse_ll(node_t *head) {
  node_t *curr = head;
  node_t *prev = NULL;

  while (curr) {
    node_t *next = curr->next;
    curr->next = prev;
    prev = curr;
    curr = next;
  }

  return prev;
}

void print_ll(node_t *head) {
  node_t *curr = head;

  while (curr) {
    printf("%d\n", curr->value);
    curr = curr->next;
  }
}

int main(void) {
  node_t *head = node_init(0, NULL);
  node_t *second = node_init(1, NULL);
  node_t *third = node_init(2, NULL);
  head->next = second;
  second->next = third;

  node_t *new_head = reverse_ll(head);

  print_ll(new_head);

  while (new_head) {
    node_t* next = new_head->next;
    free(new_head);
    new_head = next;
  }

  return 0;
}
