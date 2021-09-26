//
// Created by Jack Matheson on 9/19/21.
//

#include <stdio.h>
#include <stdbool.h>
#include "index.h"

bool test_histx() {
    char *r = create_hash("jack is testing");
    printf("Result [%s]\n", r);
    return true;
}