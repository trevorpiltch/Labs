#include <stdlib.h>
#include "ring_buffer.h"

ring_buffer_t* ring_buffer_init(int k) {
    ring_buffer_t* obj = malloc(sizeof(ring_buffer_t));
    obj->head = 0;
    obj->tail = -1;
    obj->capacity = k;
    obj->size = 0;
    obj->data = malloc(sizeof(int) * k);
    return obj;
}

int ring_buffer_enqueue(ring_buffer_t* obj, int value) {
    if (obj->size == obj->capacity) {
        return 0;
    }

    obj->tail = (obj->tail + 1) % obj->capacity;
    obj->data[obj->tail] = value;
    obj->size++;
    return 1;
}

int ring_buffer_dequeue(ring_buffer_t* obj) {
    if (obj->size == 0) {
        return 0;
    }

    obj->head = (obj->head + 1) % obj->capacity;
    obj->size--;
    return 1;
}

int ring_buffer_front(ring_buffer_t* obj) {
    if (obj->size == 0) {
        return -1;
    }

    return obj->data[obj->head];
}

int ring_buffer_rear(ring_buffer_t* obj) {
    if (obj->size == 0) {
        return -1;
    }

    return obj->data[obj->tail];
}

int ring_buffer_is_empty(ring_buffer_t* obj) {
    return obj->size == 0;
}

int ring_buffer_is_full(ring_buffer_t* obj) {
    return obj->size == obj->capacity;
}

void ring_buffer_free(ring_buffer_t* obj) {
    free(obj->data);
    free(obj);
}
