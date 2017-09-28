#pragma once

#include "tester.h"

#define TESTER_PASTE_IMPL(a, b) a ## b
#define TESTER_PASTE(a, b) TESTER_PASTE_IMPL(a, b)

#define TESTER_CHECK_NOEXCEPT(expr) ::tester::check_noexcept({ __FILE__, __LINE__, #expr }, [] { expr; })
#define TESTER_CHECK(expr) ::tester::check({ __FILE__, __LINE__, #expr }, ::tester::split << expr)
#define TESTER_CHECK_APPROX(expr) ::tester::check_approx({ __FILE__, __LINE__, #expr }, ::tester::split << expr)
#define TESTER_CHECK_EACH(expr) ::tester::check_each({ __FILE__, __LINE__, #expr }, ::tester::split << expr)
#define TESTER_CHECK_EACH_APPROX(expr) ::tester::check_each_approx({ __FILE__, __LINE__, #expr }, ::tester::split << expr)
#define TESTER_TEST_CASE(name) static const auto TESTER_PASTE(_test_case_, __COUNTER__) = ::tester::Case(name) << []
