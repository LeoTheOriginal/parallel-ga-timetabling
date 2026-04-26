/*
 * Drukuje precyzyjny rozmiar ProblemData oraz sub-struktur.
 * Kompilacja: gcc -I src plots/sizeof_problemdata.c -o /tmp/sizeof_pd
 *             /tmp/sizeof_pd
 */
#include <stdio.h>
#include "types.h"

int main(void) {
    printf("sizeof(Room)         = %zu B\n", sizeof(Room));
    printf("sizeof(Teacher)      = %zu B\n", sizeof(Teacher));
    printf("sizeof(StudentGroup) = %zu B\n", sizeof(StudentGroup));
    printf("sizeof(Course)       = %zu B\n", sizeof(Course));
    printf("sizeof(Event)        = %zu B\n", sizeof(Event));
    printf("sizeof(ProblemData)  = %zu B = %.2f KB = %.3f MB\n",
        sizeof(ProblemData),
        sizeof(ProblemData) / 1024.0,
        sizeof(ProblemData) / (1024.0 * 1024.0));
    return 0;
}
