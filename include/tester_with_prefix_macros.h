#pragma once

#include "tester.h"

#define TESTER_PASTE_IMPL(a, b) a ## b
#define TESTER_PASTE(a, b) TESTER_PASTE_IMPL(a, b)

#define TESTER_ASSERTION(expr) ::tester::make_assertion(__FILE__, __LINE__, #expr, [&] { return ::tester::Split{} << expr; })
#define TESTER_CHECK(expr) ::tester::check(TESTER_ASSERTION(expr))
#define TESTER_CHECK_APPROX(expr) ::tester::check_approx(TESTER_ASSERTION(expr))
#define TESTER_CHECK_EACH(expr) ::tester::check_each(TESTER_ASSERTION(expr))
#define TESTER_TEST_CASE(name) static const auto TESTER_PASTE(_test_case_, __COUNTER__) = ::tester::Case(name) << []
#define TESTER_SUBCASE(name) if (auto TESTER_PASTE(_subcase_, __COUNTER__) = ::tester::Subcase(name)) 
#define TESTER_REPEAT(count) for (auto i : ::tester::Repeat(count))
