#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
compiler=${CC:-cc}
if [ "${SANITIZE:-0}" = "1" ]; then
    set -- -fsanitize=address,undefined -fno-omit-frame-pointer
else
    set --
fi
for suite in engine graph app; do
    case "$suite" in
        engine) sources="math_engine.c" ;;
        graph) sources="math_engine.c math_graph.c" ;;
        app) sources="math_engine.c math_graph.c math_app.c" ;;
    esac
    # Source names are fixed above; deliberate shell splitting passes each file.
    "$compiler" -std=c11 -Wall -Wextra -Werror -pedantic "$@" $sources \
        "tests/test_${suite}.c" -lm -o "build/tests/test_${suite}"
    "build/tests/test_${suite}"
done
