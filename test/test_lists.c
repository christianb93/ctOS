/*
 * test_lists.c
 */
#include <stdio.h>
#include "lists.h"
#include "kunit.h"
#include "stdlib.h"

typedef struct _list_item_t {
    int data;
    struct _list_item_t* next;
    struct _list_item_t* prev;
} list_item_t;

/* Testcase 1: set and clear bit in a bit mask
 * which is located in the first byte
 * and test
 */
int testcase1() {
    char bitfield[256];
    bitfield[0] = 0x2;
    BITFIELD_SET_BIT(bitfield, 0);
    ASSERT(1==BITFIELD_GET_BIT(bitfield, 0));
    ASSERT (1== (bitfield[0] & 0x1));
    BITFIELD_CLEAR_BIT(bitfield, 1);
    ASSERT(0==BITFIELD_GET_BIT(bitfield, 1));
    ASSERT (0== (bitfield[0] & 0x2));
    return 0;
}

/* Testcase 2: set and clear bit in a bit mask
 * which is not located in the first byte
 * and test
 */
int testcase2() {
    char bitfield[256];
    bitfield[1] = 0x8;
    BITFIELD_CLEAR_BIT(bitfield, 11);
    BITFIELD_SET_BIT(bitfield, 12);
    ASSERT(0==BITFIELD_GET_BIT(bitfield, 11));
    ASSERT(1==BITFIELD_GET_BIT(bitfield, 12));
    ASSERT(bitfield[1]==0x10);
    return 0;
}

/*
 * Testcase 3:
 * add a new element to the front of an empty list
 */
int testcase3() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item;
    item = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item)
        return 1;
    item->data = 1;
    LIST_ADD_FRONT(head, tail, item);
    ASSERT(0==item->next);
    ASSERT(0==item->prev);
    ASSERT(head==item);
    ASSERT(tail==item);
    return 0;
}

/*
 * Testcase 4:
 * add two elements to the front of an empty list
 */
int testcase4() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item1->data = 1;
    item2->data = 2;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    ASSERT(0==item1->next);
    ASSERT(item2->next==item1);
    ASSERT(head==item2);
    ASSERT(0==item2->prev);
    ASSERT(tail==item1);
    ASSERT(item1->prev == item2);
    return 0;
}

/*
 * Testcase 5:
 * add a new element to the end of an empty list
 */
int testcase5() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item;
    item = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item)
        return 1;
    item->data = 1;
    LIST_ADD_END(head, tail, item);
    ASSERT(0==item->next);
    ASSERT(0==item->prev);
    ASSERT(head==item);
    ASSERT(tail==item);
    return 0;
}

/*
 * Testcase 6:
 * add two elements to the end of an empty list
 */
int testcase6() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item1->data = 1;
    item2->data = 2;
    LIST_ADD_END(head, tail, item1);
    LIST_ADD_END(head, tail, item2);
    ASSERT(tail==item2);
    ASSERT(head==item1);
    ASSERT(item1->next == item2);
    ASSERT(item1->prev == 0);
    ASSERT(item2->next == 0);
    ASSERT(item2->prev == item1);
    return 0;
}

/*
 * Testcase 7
 * Remove the last item from the front of the list
 */
int testcase7() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item;
    item = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item)
        return 1;
    item->data = 1;
    LIST_ADD_FRONT(head, tail, item);
    LIST_REMOVE_FRONT(head, tail);
    ASSERT(head==0);
    ASSERT(tail==0);
    return 0;
}

/*
 * Testcase 8
 * Remove an item from the front of the list when the list has two elements
 */
int testcase8() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_REMOVE_FRONT(head, tail);
    ASSERT(head==item1);
    ASSERT(tail==item1);
    ASSERT(item1->prev==0);
    ASSERT(item1->next==0);
    return 0;
}

/*
 * Testcase 9
 * Remove an item from the front of the list when the list has three elements
 */
int testcase9() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_FRONT(head, tail, item3);
    LIST_REMOVE_FRONT(head, tail);
    ASSERT(head==item2);
    ASSERT(tail==item1);
    ASSERT(item1->prev==item2);
    ASSERT(item1->next==0);
    ASSERT(item2->prev==0);
    ASSERT(item2->next==item1);
    return 0;
}

/*
 * Testcase 10
 * Iterate through a list with ten elements
 */
int testcase10() {
    list_item_t* head = 0;
    int i;
    int checksum;
    int sum = 0;
    list_item_t* tail = 0;
    list_item_t* item;
    for (i = 0; i < 10; i++) {
        item = (list_item_t*) malloc(sizeof(list_item_t));
        if (0 == item)
            return 1;
        item->data = i;
        sum += i;
        LIST_ADD_END(head,tail, item);
    }
    i = 0;
    checksum = 0;
    LIST_FOREACH(head, item) {
        ASSERT(item->data == i);
        checksum += item->data;
        i++;
    }
    ASSERT(checksum==sum);
    ASSERT(i==10);
    return 0;
}

/*
 * Testcase 11
 * Remove an item from the front of the list when the list has three elements
 * using LIST_REMOVE
 */
int testcase11() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_FRONT(head, tail, item3);
    LIST_REMOVE(head, tail, item3);
    ASSERT(head==item2);
    ASSERT(tail==item1);
    ASSERT(item1->prev==item2);
    ASSERT(item1->next==0);
    ASSERT(item2->prev==0);
    ASSERT(item2->next==item1);
    return 0;
}

/*
 * Testcase 12
 * Remove an item from the tail of the list when the list has three elements
 * using LIST_REMOVE
 */
int testcase12() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_FRONT(head, tail, item3);
    LIST_REMOVE(head, tail, item1);
    ASSERT(head==item3);
    ASSERT(tail==item2);
    ASSERT(item2->prev==item3);
    ASSERT(item2->next==0);
    ASSERT(item3->prev==0);
    ASSERT(item3->next==item2);
    return 0;
}

/*
 * Testcase 13
 * Remove an item from the mid of the list when the list has three elements
 * using LIST_REMOVE
 */
int testcase13() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_FRONT(head, tail, item3);
    LIST_REMOVE(head, tail, item2);
    ASSERT(head==item3);
    ASSERT(tail==item1);
    ASSERT(item1->prev==item3);
    ASSERT(item1->next==0);
    ASSERT(item3->prev==0);
    ASSERT(item3->next==item1);
    return 0;
}

/*
 * Testcase 14
 * Tested function: LIST_ADD_AFTER
 * Testcase: add an item in the middle of a list
 */
int testcase14() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_AFTER(head, tail, item2, item3);
    ASSERT(item2->next==item3);
    ASSERT(item1->prev==item3);
    ASSERT(item3->next==item1);
    ASSERT(item3->prev==item2);
    return 0;

}

/*
 * Testcase 15
 * Tested function: LIST_ADD_AFTER
 * Testcase: add an item after the tail of a list
 */
int testcase15() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    list_item_t* item3;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    item3 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item3)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_FRONT(head, tail, item2);
    LIST_ADD_AFTER(head, tail, item1, item3);
    ASSERT(item2->next==item1);
    ASSERT(item2->prev==0);
    ASSERT(item1->next==item3);
    ASSERT(item1->prev==item2);
    ASSERT(item3->prev==item1);
    ASSERT(item3->next==0);
    return 0;

}


/*
 * Testcase 16
 * Tested function: LIST_ADD_AFTER
 * Testcase: add an item after the head of a list
 * which only has one element
 */
int testcase16() {
    list_item_t* head = 0;
    list_item_t* tail = 0;
    list_item_t* item1;
    list_item_t* item2;
    item1 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item1)
        return 1;
    item2 = (list_item_t*) malloc(sizeof(list_item_t));
    if (0 == item2)
        return 1;
    LIST_ADD_FRONT(head, tail, item1);
    LIST_ADD_AFTER(head, tail, item1, item2);
    ASSERT(item2->next==0);
    ASSERT(item2->prev==item1);
    ASSERT(item1->next==item2);
    ASSERT(item1->prev==0);
    return 0;

}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    END;
}

