#!/bin/bash
# filter allowed C++11 features (supported by MSVC 2010)
grep -v "'nullptr' is incompatible\|'override' keyword\|'auto' type specifier\|'static_assert' declarations\|'decltype' type specifier\|rvalue references are\|lambda expressions are\|consecutive right angle\|local type .* as template argument\|enumeration type in nested name specifier\|enumeration types with a fixed underlying type\|buffer contents are indeterminate" "$1"
