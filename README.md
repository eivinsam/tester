# tester
Somewhat modern C++ unit testing library

Inspired by [doctest](https://github.com/onqtam/doctest). Trades some performance for maintainability and extensibility:
 
Uses `<sstream>` for output - you can read the report from `tester::report` and print it whereever you like
