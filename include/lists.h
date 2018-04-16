/*
 * lists.h
 * This header file contains macros used to manipulate lists and bit fields
 */

#ifndef _LISTS_H_
#define _LISTS_H_

/*
 * A bitfield is of type char[]. The following macros can be used
 * to access an individual bit in this structure
 */
#define BITFIELD_GET_BIT(field, bit)   (((*(field + (bit / 8))) >> (bit % 8)) & 0x1)
#define BITFIELD_CLEAR_BIT(field, bit)   do {(field[bit/8]) &= (~(1 << (bit % 8))); } while (0)
#define BITFIELD_SET_BIT(field, bit)   do {(field[bit/8]) |= (1 << (bit % 8)); } while (0)


/*
 * A list has a head and a tail which need to be pointers to a structure of the type managed
 * in the list. The only assumption about this structure which we make is that it has a field next
 * and a field prev.
 * Initially, tail and head must both be null.
 * The next pointer goes from head to tail, i.e. tail->next = 0
 * The prev pointer goes from tail to head, i.e. head->prev = 0
 * The following operations are supported
 * LIST_ADD_FRONT: add an element at the head of the list, i.e. the element becomes the new head
 * LIST_ADD_END: add an element at the end of a list, i.e. the element becomes the new tail
 * LIST_REMOVE_FRONT: remove the current head from the list
 * LIST_REMOVE: remove a specified item from the list
 * LIST_FOREACH: iterate through all elements of the list
 * LIST_ADD_AFTER: add an element after another element to the list
 */

#define LIST_ADD_FRONT(head, tail, item) do { \
                                                 if ((head)==0) \
                                                 { \
                                                     (head) = (item); \
                                                     (tail) = (item); \
                                                     (item)->next = 0; \
                                                     (item)->prev = 0; \
                                                 } \
                                                 else \
                                                 { \
                                                     (head)->prev = (item); \
                                                     (item)->next = (head); \
                                                     (item)->prev = 0; \
                                                     (head) = (item); \
                                                 } \
                                             } while (0)

#define LIST_ADD_END(head, tail, item) do { \
                                                 if ((tail)==0) \
                                                 { \
                                                     (tail) = (item); \
                                                     (head) = (item); \
                                                     (item)->next = 0; \
                                                     (item)->prev = 0; \
                                                 } \
                                                 else \
                                                 { \
                                                     (tail)->next = (item); \
                                                     (item)->prev = (tail); \
                                                     (item)->next = 0; \
                                                     (tail) = (item); \
                                                 } \
                                             } while (0)

#define LIST_ADD_AFTER(head, tail, position, item) do { \
                                                        item->prev = position; \
                                                        item->next = position->next; \
                                                        if (position->next) { \
                                                            position->next->prev = item; \
                                                        } \
                                                        position->next = item; \
                                             } while (0)

#define LIST_REMOVE_FRONT(head, tail) do { \
                                            if ((head)->next) { \
                                                ((head)->next)->prev = 0; \
                                                (head)=(head)->next; \
                                            } \
                                            else {\
                                                (head) = 0; \
                                                (tail) = 0; \
                                            }\
                                         } while (0)

#define LIST_REMOVE_END(head, tail) do { \
                                            if ((tail)->prev) { \
                                                ((tail)->prev)->next = 0; \
                                                (tail)=(tail)->prev; \
                                            } \
                                            else {\
                                                (head) = 0; \
                                                (tail) = 0; \
                                            }\
                                         } while (0)


#define LIST_REMOVE(head, tail, item) do { \
                                             if ((item)==(head)) \
                                                 LIST_REMOVE_FRONT(head, tail); \
                                             else if ((item)==(tail)) \
                                                 LIST_REMOVE_END(head, tail); \
                                             else { \
                                                 ((item)->next)->prev = (item)->prev; \
                                                 ((item)->prev)->next = (item)->next; \
                                             } \
                                         } while (0)

#define LIST_FOREACH(head, item) for (item=head;item;item=item->next)

#endif /* _LISTS_H_ */
