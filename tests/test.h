/* test.h — Mini-cadre de tests : CHECK() compte les vérifications ratées. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>

extern int tests_run;
extern int tests_failed;

#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        fprintf(stderr, "%s:%d: échec : %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

void test_piece(void);
void test_tetris(void);
void test_scores(void);
void test_save(void);
void test_gfx(void);

#endif /* TEST_H */
