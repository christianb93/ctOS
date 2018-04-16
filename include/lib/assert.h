/*
 * assert.h
 *
 */

#ifndef _ASSERT_H_
#define _ASSERT_H_

#define assert(x) do { if (!(x)) { \
                                      fprintf(stderr, "Assertion %s failed in %s at line %d in function %s\n", #x,  __FILE__, __LINE__, __FUNCTION__); \
                                  } \
                      } while (0)

#endif /* _ASSERT_H_ */
