#include "../math_engine.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

static void expect_value(const char* expression, float x, float ans, bool degrees, float expected) {
    MWProgram program;
    size_t position = 999;
    MWStatus status = mw_compile(expression, &program, &position);
    if(status != MW_OK) {
        fprintf(stderr, "Compile failed: %s at %zu (%s)\n", expression, position, mw_status_message(status));
        assert(status == MW_OK);
    }
    float actual = NAN;
    status = mw_evaluate(&program, x, ans, degrees, &actual);
    if(status != MW_OK) {
        fprintf(stderr, "Evaluation failed: %s (%s)\n", expression, mw_status_message(status));
        assert(status == MW_OK);
    }
    const float tolerance = fmaxf(fabsf(expected) * 2.0e-6f, 2.0e-6f);
    if(!isfinite(actual) || fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "%s: expected %.9g, got %.9g\n", expression, (double)expected, (double)actual);
        assert(false);
    }
    ++checks;
}

static void expect_compile_error(const char* expression, MWStatus expected, size_t expected_position) {
    MWProgram program;
    assert(mw_compile("1+1", &program, NULL) == MW_OK);
    size_t position = 999;
    const MWStatus actual = mw_compile(expression, &program, &position);
    if(actual != expected || position != expected_position) {
        fprintf(stderr, "Wrong error: %s -> %s at %zu (expected %s at %zu)\n", expression,
                mw_status_message(actual), position, mw_status_message(expected), expected_position);
        assert(false);
    }
    assert(program.length == 0);
    float result = 42.0f;
    assert(mw_evaluate(&program, 0.0f, 0.0f, false, &result) == MW_ERROR_EMPTY);
    assert(result == 42.0f);
    ++checks;
}

static void expect_eval_error(const char* expression, bool degrees, MWStatus expected) {
    MWProgram program;
    assert(mw_compile(expression, &program, NULL) == MW_OK);
    float result = 42.0f;
    const MWStatus actual = mw_evaluate(&program, 0.0f, 0.0f, degrees, &result);
    if(actual != expected) {
        fprintf(stderr, "Wrong runtime error: %s -> %s (expected %s)\n", expression,
                mw_status_message(actual), mw_status_message(expected));
        assert(false);
    }
    assert(result == 42.0f);
    ++checks;
}

static void test_arithmetic(void) {
    expect_value("2+3*4", 0, 0, false, 14);
    expect_value("(2+3)*4", 0, 0, false, 20);
    expect_value("18/3/2", 0, 0, false, 3);
    expect_value("20-3-2", 0, 0, false, 15);
    expect_value(" 2 +\t3 *\n4\r ", 0, 0, false, 14);
    expect_value(".5+2.", 0, 0, false, 2.5f);
    expect_value("1.25e2+1E-2", 0, 0, false, 125.01f);
    expect_value("1e+2", 0, 0, false, 100);
    expect_value("0.0000000025e10", 0, 0, false, 25);
    expect_value("123456789012345678901234567890e-28", 0, 0, false, 12.345679f);
    expect_value("0e9999999999999", 0, 0, false, 0);
    expect_value("1e-9999999999999", 0, 0, false, 0);
    expect_value("--3+-+2", 0, 0, false, 1);
    expect_value("-2^2", 0, 0, false, -4);
    expect_value("(-2)^2", 0, 0, false, 4);
    expect_value("2^-2", 0, 0, false, 0.25f);
    expect_value("2^3^2", 0, 0, false, 512);
    expect_value("2^-2^2", 0, 0, false, 0.0625f);
    expect_value("(-2)^-3", 0, 0, false, -0.125f);
    expect_value("2*-3+4", 0, 0, false, -2);
    expect_value("50%", 0, 0, false, 0.5f);
    expect_value("100+10%", 0, 0, false, 100.1f);
    expect_value("200*15%", 0, 0, false, 30);
    expect_value("(10+20)%", 0, 0, false, 0.3f);
    expect_value("100%%", 0, 0, false, 0.01f);
    expect_value("-2%^2", 0, 0, false, -0.0004f);
    expect_value("ans*2+x^2", -3, 7, false, 23);
    expect_value("ANS*2+X^2", -3, 7, false, 23);
    expect_value("pi", 0, 0, false, 3.141592653589793f);
    expect_value("e", 0, 0, false, 2.718281828459045f);
    expect_value("sqrt(abs(-81))", 0, 0, false, 9);
    expect_value("ln(e)+log(1000)", 0, 0, false, 4);
    expect_value("SIN(30)+COS(60)", 0, 0, true, 1);
    expect_value("sin(pi/2)", 0, 0, false, 1);
    expect_value("cos(pi)", 0, 0, false, -1);
    expect_value("tan(pi/4)", 0, 0, false, 1);
    expect_value("sin(390)", 0, 0, true, 0.5f);
    expect_value("cos(-360)", 0, 0, true, 1);
    expect_value("tan(45)", 0, 0, true, 1);
    expect_value("sin(30)", 0, 0, false, -0.9880316f);
    expect_value("0^3", 0, 0, false, 0);
    expect_value("sqrt(-0)", 0, 0, false, 0);

    MWProgram program;
    float result = 0;
    assert(mw_compile("1e-40", &program, NULL) == MW_OK);
    assert(mw_evaluate(&program, 0, 0, false, &result) == MW_OK);
    assert(result > 0.0f && fabsf(result / 1.0e-40f - 1.0f) < 1.0e-4f);
    assert(mw_compile("3e38", &program, NULL) == MW_OK);
    assert(mw_evaluate(&program, 0, 0, false, &result) == MW_OK);
    assert(isfinite(result) && result > 2.9e38f);

    /* One compiled graph can be sampled repeatedly without reparsing. */
    assert(mw_compile("x^2+ans", &program, NULL) == MW_OK);
    for(int x = -20; x <= 20; ++x) {
        assert(mw_evaluate(&program, (float)x, 3.0f, false, &result) == MW_OK);
        assert(fabsf(result - (float)(x * x + 3)) < 0.001f);
        ++checks;
    }
}

static void test_rejected_inputs(void) {
    expect_compile_error("", MW_ERROR_EMPTY, 0);
    expect_compile_error(" \n\t", MW_ERROR_EMPTY, 0);
    expect_compile_error("2+", MW_ERROR_SYNTAX, 2);
    expect_compile_error("()", MW_ERROR_SYNTAX, 1);
    expect_compile_error("(2+3", MW_ERROR_SYNTAX, 4);
    expect_compile_error("2+3)", MW_ERROR_SYNTAX, 3);
    expect_compile_error("2(3)", MW_ERROR_SYNTAX, 1);
    expect_compile_error("2pi", MW_ERROR_SYNTAX, 1);
    expect_compile_error("2 3", MW_ERROR_SYNTAX, 2);
    expect_compile_error("1..2", MW_ERROR_SYNTAX, 2);
    expect_compile_error(".", MW_ERROR_SYNTAX, 0);
    expect_compile_error("1e", MW_ERROR_SYNTAX, 2);
    expect_compile_error("1e+", MW_ERROR_SYNTAX, 3);
    expect_compile_error("1e 2", MW_ERROR_SYNTAX, 2);
    expect_compile_error("sin x", MW_ERROR_SYNTAX, 4);
    expect_compile_error("sin()", MW_ERROR_SYNTAX, 4);
    expect_compile_error("sin(1,2)", MW_ERROR_SYNTAX, 5);
    expect_compile_error("sin(1)(2)", MW_ERROR_SYNTAX, 6);
    expect_compile_error("foo(2)", MW_ERROR_UNKNOWN_NAME, 0);
    expect_compile_error("nan", MW_ERROR_UNKNOWN_NAME, 0);
    expect_compile_error("inf", MW_ERROR_UNKNOWN_NAME, 0);
    expect_compile_error("x2", MW_ERROR_UNKNOWN_NAME, 0);
    expect_compile_error("2**3", MW_ERROR_SYNTAX, 2);
    expect_compile_error("3//2", MW_ERROR_SYNTAX, 2);
    expect_compile_error("%2", MW_ERROR_SYNTAX, 0);
    expect_compile_error("1;2", MW_ERROR_SYNTAX, 1);
    expect_compile_error("1e39", MW_ERROR_OVERFLOW, 0);
    expect_compile_error("9e38", MW_ERROR_OVERFLOW, 0);
    expect_compile_error("1e999999999999", MW_ERROR_OVERFLOW, 0);

    expect_eval_error("1/0", false, MW_ERROR_DIV_ZERO);
    expect_eval_error("0/0", false, MW_ERROR_DIV_ZERO);
    expect_eval_error("2/(3-3)", false, MW_ERROR_DIV_ZERO);
    expect_eval_error("0^-1", false, MW_ERROR_DIV_ZERO);
    expect_eval_error("sqrt(-1)", false, MW_ERROR_DOMAIN);
    expect_eval_error("ln(0)", false, MW_ERROR_DOMAIN);
    expect_eval_error("log(-10)", false, MW_ERROR_DOMAIN);
    expect_eval_error("(-2)^.5", false, MW_ERROR_DOMAIN);
    expect_eval_error("0^0", false, MW_ERROR_DOMAIN);
    expect_eval_error("tan(90)", true, MW_ERROR_DOMAIN);
    expect_eval_error("tan(270)", true, MW_ERROR_DOMAIN);
    expect_eval_error("tan(pi/2)", false, MW_ERROR_DOMAIN);
    expect_eval_error("3e38*2", false, MW_ERROR_OVERFLOW);
    expect_eval_error("2^200", false, MW_ERROR_OVERFLOW);

    MWProgram program;
    float result = 42.0f;
    assert(mw_compile("x+ans", &program, NULL) == MW_OK);
    assert(mw_evaluate(&program, INFINITY, 0, false, &result) == MW_ERROR_OVERFLOW);
    assert(mw_evaluate(&program, 0, NAN, false, &result) == MW_ERROR_OVERFLOW);
    assert(result == 42.0f);
    assert(mw_compile(NULL, &program, NULL) == MW_ERROR_SYNTAX);
    assert(program.length == 0);
    assert(mw_compile("1", NULL, NULL) == MW_ERROR_SYNTAX);
    assert(mw_evaluate(NULL, 0, 0, false, &result) == MW_ERROR_SYNTAX);
    assert(mw_evaluate(&program, 0, 0, false, NULL) == MW_ERROR_SYNTAX);
}

static void test_bounds(void) {
    char expression[256];
    /* The maximum-length flat expression remains useful. */
    for(size_t i = 0; i < MW_EXPRESSION_CAPACITY - 1; ++i) expression[i] = i % 2 == 0 ? '1' : '+';
    expression[MW_EXPRESSION_CAPACITY - 1] = '\0';
    expect_value(expression, 0, 0, false, 48);

    memset(expression, '1', sizeof(expression));
    expression[sizeof(expression) - 1] = '\0';
    expect_compile_error(expression, MW_ERROR_TOO_LONG, MW_EXPRESSION_CAPACITY - 1);

    /* A full fixed-size input with no terminator is rejected without over-read. */
    char unterminated[MW_EXPRESSION_CAPACITY];
    memset(unterminated, '1', sizeof(unterminated));
    MWProgram program;
    assert(mw_compile(unterminated, &program, NULL) == MW_ERROR_TOO_LONG);

    const size_t valid_depth = MW_MAX_PARSE_DEPTH - 1;
    memset(expression, '(', valid_depth);
    expression[valid_depth] = '1';
    memset(expression + valid_depth + 1, ')', valid_depth);
    expression[valid_depth * 2 + 1] = '\0';
    expect_value(expression, 0, 0, false, 1);

    memset(expression, '(', MW_MAX_PARSE_DEPTH);
    expression[MW_MAX_PARSE_DEPTH] = '1';
    memset(expression + MW_MAX_PARSE_DEPTH + 1, ')', MW_MAX_PARSE_DEPTH);
    expression[MW_MAX_PARSE_DEPTH * 2 + 1] = '\0';
    expect_compile_error(expression, MW_ERROR_TOO_COMPLEX, MW_MAX_PARSE_DEPTH);

    memset(expression, '-', MW_MAX_PARSE_DEPTH);
    expression[MW_MAX_PARSE_DEPTH] = '1';
    expression[MW_MAX_PARSE_DEPTH + 1] = '\0';
    expect_compile_error(expression, MW_ERROR_TOO_COMPLEX, MW_MAX_PARSE_DEPTH);

    for(size_t i = 0; i < MW_MAX_PARSE_DEPTH * 2; ++i) expression[i] = i % 2 == 0 ? '1' : '^';
    expression[MW_MAX_PARSE_DEPTH * 2] = '1';
    expression[MW_MAX_PARSE_DEPTH * 2 + 1] = '\0';
    expect_compile_error(expression, MW_ERROR_TOO_COMPLEX, MW_MAX_PARSE_DEPTH * 2);

    /* Defensive handling for damaged bytecode: no stack access may escape. */
    float result = 42.0f;
    memset(&program, 0, sizeof(program));
    program.length = 255;
    assert(mw_evaluate(&program, 0, 0, false, &result) == MW_ERROR_TOO_COMPLEX);
    assert(mw_compile("1", &program, NULL) == MW_OK);
    program.instructions[0].operation = 255;
    assert(mw_evaluate(&program, 0, 0, false, &result) == MW_ERROR_SYNTAX);
    assert(mw_compile("1", &program, NULL) == MW_OK);
    program.length = MW_MAX_STACK + 1;
    for(unsigned i = 1; i < program.length; ++i) program.instructions[i] = program.instructions[0];
    assert(mw_evaluate(&program, 0, 0, false, &result) == MW_ERROR_TOO_COMPLEX);
    assert(result == 42.0f);
}

static void test_malformed_fuzz(void) {
    /* Bounded, reproducible hostile inputs exercise rejection and error offsets;
     * run this test under ASan/UBSan as well as the normal host build. */
    static const char alphabet[] = "0123456789e.xans()+-*/^% \t_sqrt,;";
    uint32_t seed = 0x73ab9e21u;
    for(unsigned trial = 0; trial < 10000; ++trial) {
        char expression[MW_EXPRESSION_CAPACITY];
        seed = seed * 1664525u + 1013904223u;
        const size_t length = seed % MW_EXPRESSION_CAPACITY;
        for(size_t i = 0; i < length; ++i) {
            seed = seed * 1664525u + 1013904223u;
            expression[i] = alphabet[seed % (sizeof(alphabet) - 1)];
        }
        expression[length] = '\0';
        MWProgram program;
        size_t position = 999;
        const MWStatus status = mw_compile(expression, &program, &position);
        assert(position <= length);
        if(status == MW_OK) {
            float result = NAN;
            if(mw_evaluate(&program, 0.5f, 2.0f, false, &result) == MW_OK) assert(isfinite(result));
        } else {
            assert(program.length == 0);
        }
        ++checks;
    }
}

int main(void) {
    test_arithmetic();
    test_rejected_inputs();
    test_bounds();
    test_malformed_fuzz();
    printf("Math engine: %u checks passed\n", checks);
    return 0;
}
