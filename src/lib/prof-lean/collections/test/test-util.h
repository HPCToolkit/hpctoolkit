#include <stdio.h>
#include <stdlib.h>


#define ASSERT_IMPL(CONDITION, TERMINATE)                                     \
  do {                                                                        \
    if (!(CONDITION)) {                                                       \
      fprintf(stderr, "%s %d %s", __FILE__, __LINE__, #CONDITION);            \
      if (TERMINATE) {                                                        \
        abort();                                                              \
      }                                                                       \
    }                                                                         \
  } while (0)                                                                 \


#define ASSERT(CONDITION) ASSERT_IMPL(CONDITION, 1)
#define EXPECT(CONDITION) ASSERT_IMPL(CONDITION, 0)


#define FAIL() ASSERT(false)
#define ADD_FAILURE() EXPECT(false, false)

#define SUCCESS() ASSERT(true, true)

#define ASSERT_FALSE(A) ASSERT((A) == 0)
#define EXPECT_FALSE(A) EXPECT((A) == 0)

#define ASSERT_TRUE(A) ASSERT((A) != 0)
#define EXPECT_TRUE(A) EXPECT((A) != 0)

#define ASSERT_EQ(A, B) ASSERT((A) == (B))
#define EXPECT_EQ(A, B) EXPECT((A) == (B))

#define ASSERT_NE(A, B) ASSERT((A) != (B))
#define EXPECT_NE(A, B) EXPECT((A) != (B))

#define ASSERT_LT(A, B) ASSERT((A) < (B))
#define EXPECT_LT(A, B) EXPECT((A) < (B))

#define ASSERT_LE(A, B) ASSERT((A) <= (B))
#define EXPECT_LE(A, B) EXPECT((A) <= (B))

#define ASSERT_GT(A, B) ASSERT((A) > (B))
#define EXPECT_GT(A, B) EXPECT((A) > (B))

#define ASSERT_GE(A, B) ASSERT((A) >= (B))
#define EXPECT_GE(A, B) EXPECT((A) >= (B))


typedef struct test_entry_t
{
  struct test_entry_t *next;
  void (*fn)();
  const char *test_suite_name;
  const char *test_name;
} test_entry_t;
test_entry_t *tests_head = NULL, *tests_tail = NULL;


void
register_test
(
 void (*test)(),
 const char *test_suite_name,
 const char *test_name
)
{
  test_entry_t *test_entry
    = (test_entry_t *) malloc(sizeof(struct test_entry_t));
  test_entry->fn = test;
  test_entry->next = NULL;
  test_entry->test_suite_name = test_suite_name;
  test_entry->test_name = test_name;

  if (tests_tail == NULL) {
    tests_head = tests_tail = test_entry;
  } else {
    tests_tail->next = test_entry;
    tests_tail = test_entry;
  }
}


#define TEST(TestSuiteName, TestName)                       \
void TestSuiteName ## _ ## TestName ();                     \
                                                            \
__attribute__ ((constructor))                               \
void TestSuiteName ## _ ## TestName ## _ ## register() {    \
  register_test(TestSuiteName ## _ ## TestName,             \
                #TestSuiteName, #TestName);                 \
}                                                           \
void TestSuiteName ## _ ## TestName ()


#define TEST_MAIN()                                         \
int main(int argc, char const *argv[])                      \
{                                                           \
  size_t i = 0;                                             \
  test_entry_t *cur = tests_head;                           \
  while (cur)                                               \
  {                                                         \
    cur->fn();                                              \
    printf("Test #%zu passed.\n\t%s\n\t%s\n\n", i + 1,      \
      cur->test_suite_name, cur->test_name);                \
                                                            \
    cur = cur->next;                                        \
    ++i;                                                    \
  }                                                         \
                                                            \
  return 0;                                                 \
}
