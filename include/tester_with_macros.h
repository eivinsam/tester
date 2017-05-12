#pragma once

#include "tester_with_prefix_macros.h"

#define TEST_CASE(name) TESTER_TEST_CASE(name)
#define SUBCASE(name) TESTER_SUBCASE(name)
#define CHECK(expr) TESTER_CHECK(expr)
#define CHECK_EACH(expr) TESTER_CHECK_EACH(expr)
