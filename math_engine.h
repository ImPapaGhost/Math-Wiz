#ifndef MATH_ENGINE_H
#define MATH_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Includes the terminating NUL: expressions contain at most 95 characters. */
#define MW_EXPRESSION_CAPACITY 96u
#define MW_MAX_TOKENS 96u
#define MW_MAX_STACK 32u
#define MW_MAX_PARSE_DEPTH 24u

typedef enum {
    MW_OK = 0,
    MW_ERROR_EMPTY,
    MW_ERROR_SYNTAX,
    MW_ERROR_UNKNOWN_NAME,
    MW_ERROR_TOO_LONG,
    MW_ERROR_TOO_COMPLEX,
    MW_ERROR_DIV_ZERO,
    MW_ERROR_DOMAIN,
    MW_ERROR_OVERFLOW,
} MWStatus;

/* Bytecode is fixed-size and self-contained; copy MWProgram by value if needed.
 * Treat its contents as opaque and pass only successfully compiled programs to
 * mw_evaluate(). No caller-owned storage is retained. */
typedef struct {
    float value;
    uint8_t operation;
} MWInstruction;

typedef struct {
    MWInstruction instructions[MW_MAX_TOKENS];
    uint8_t length;
    uint8_t stack_required;
} MWProgram;

/* Supports + - * / ^, unary +/-, parentheses, x, ans, pi, e, and
 * sin/cos/tan/sqrt/ln/log/abs(argument). Powers associate right to left;
 * -2^2 means -(2^2), and 2^-2 is valid. Multiplication must be explicit.
 * Postfix % always divides its operand by 100: 100+10% = 100.1.
 * error_position is an optional zero-based byte offset. A failed compile
 * invalidates program (length = 0). Names are case-insensitive ASCII. */
MWStatus mw_compile(const char* expression, MWProgram* program, size_t* error_position);

/* degrees controls only the input angles for sin/cos/tan. x and ans are
 * substituted on evaluation. The result is only written on success. */
MWStatus mw_evaluate(const MWProgram* program, float x, float ans, bool degrees, float* result);

/* Returns a short, static, human-readable message. */
const char* mw_status_message(MWStatus status);

#endif
