#include "math_engine.h"

#include <math.h>

#define MW_PI 3.14159265358979323846f
#define MW_E 2.71828182845904523536f

typedef enum {
    OpNumber,
    OpX,
    OpAns,
    OpAdd,
    OpSubtract,
    OpMultiply,
    OpDivide,
    OpPower,
    OpNegate,
    OpPercent,
    OpSin,
    OpCos,
    OpTan,
    OpSqrt,
    OpLn,
    OpLog,
    OpAbs,
} MWOperation;

typedef struct {
    const char* text;
    MWProgram* program;
    size_t position;
    size_t error_position;
    unsigned depth;
    unsigned stack;
    MWStatus status;
} MWParser;

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static char lower_ascii(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

static bool is_name(char c) {
    c = lower_ascii(c);
    return (c >= 'a' && c <= 'z') || c == '_';
}

static void skip_space(MWParser* parser) {
    char c = parser->text[parser->position];
    while(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        c = parser->text[++parser->position];
    }
}

static void fail(MWParser* parser, MWStatus status, size_t position) {
    if(parser->status == MW_OK) {
        parser->status = status;
        parser->error_position = position;
    }
}

static void emit(MWParser* parser, MWOperation operation, float value) {
    if(parser->status != MW_OK) return;
    if(parser->program->length >= MW_MAX_TOKENS) {
        fail(parser, MW_ERROR_TOO_COMPLEX, parser->position);
        return;
    }
    if(operation <= OpAns) {
        if(parser->stack >= MW_MAX_STACK) {
            fail(parser, MW_ERROR_TOO_COMPLEX, parser->position);
            return;
        }
        ++parser->stack;
    } else if(operation <= OpPower) {
        if(parser->stack < 2) {
            fail(parser, MW_ERROR_SYNTAX, parser->position);
            return;
        }
        --parser->stack;
    } else if(parser->stack < 1) {
        fail(parser, MW_ERROR_SYNTAX, parser->position);
        return;
    }
    if(parser->stack > parser->program->stack_required) {
        parser->program->stack_required = (uint8_t)parser->stack;
    }
    MWInstruction* instruction = &parser->program->instructions[parser->program->length++];
    instruction->operation = (uint8_t)operation;
    instruction->value = value;
}

/* Parse without strtof(), locale dependence, or dynamically allocated storage.
 * Nine significant decimal digits are enough for float calculations. The
 * decimal scale is tracked separately so long mantissas do not overflow before
 * a negative exponent can bring the final value back into range. */
static void parse_number(MWParser* parser) {
    const size_t start = parser->position;
    uint32_t mantissa = 0;
    unsigned significant_digits = 0;
    unsigned digits = 0;
    int scale = 0;
    bool fraction = false;

    while(true) {
        char c = parser->text[parser->position];
        if(c == '.' && !fraction) {
            fraction = true;
            ++parser->position;
            continue;
        }
        if(!is_digit(c)) break;
        ++digits;
        const unsigned digit = (unsigned)(c - '0');
        if(significant_digits == 0 && digit == 0) {
            if(fraction) --scale;
        } else if(significant_digits < 9) {
            mantissa = mantissa * 10u + digit;
            ++significant_digits;
            if(fraction) --scale;
        } else if(!fraction) {
            ++scale;
        }
        ++parser->position;
    }
    if(digits == 0) {
        fail(parser, MW_ERROR_SYNTAX, start);
        return;
    }
    if(lower_ascii(parser->text[parser->position]) == 'e') {
        ++parser->position;
        bool negative = false;
        char c = parser->text[parser->position];
        if(c == '+' || c == '-') {
            negative = c == '-';
            ++parser->position;
        }
        if(!is_digit(parser->text[parser->position])) {
            fail(parser, MW_ERROR_SYNTAX, parser->position);
            return;
        }
        int exponent = 0;
        while(is_digit(parser->text[parser->position])) {
            if(exponent < 10000) {
                exponent = exponent * 10 + (parser->text[parser->position] - '0');
            }
            ++parser->position;
        }
        scale += negative ? -exponent : exponent;
    }

    float value = (float)mantissa;
    if(mantissa != 0) {
        if(scale > 38) {
            fail(parser, MW_ERROR_OVERFLOW, start);
            return;
        }
        if(scale < -54) {
            value = 0.0f;
        } else {
            /* Split small scales to preserve subnormal float literals. */
            if(scale < -30) {
                value *= 1.0e-30f;
                scale += 30;
            }
            value *= powf(10.0f, (float)scale);
        }
    }
    if(!isfinite(value)) {
        fail(parser, MW_ERROR_OVERFLOW, start);
        return;
    }
    emit(parser, OpNumber, value);
}

static bool name_equals(const char* text, size_t length, const char* expected) {
    size_t i = 0;
    while(i < length && expected[i] != '\0') {
        if(lower_ascii(text[i]) != expected[i]) return false;
        ++i;
    }
    return i == length && expected[i] == '\0';
}

static void parse_expression(MWParser* parser);
static void parse_unary(MWParser* parser);

static void parse_primary(MWParser* parser) {
    skip_space(parser);
    const size_t start = parser->position;
    const char c = parser->text[start];
    if(is_digit(c) || c == '.') {
        parse_number(parser);
    } else if(c == '(') {
        ++parser->position;
        parse_expression(parser);
        skip_space(parser);
        if(parser->text[parser->position] != ')') {
            fail(parser, MW_ERROR_SYNTAX, parser->position);
            return;
        }
        ++parser->position;
    } else if(is_name(c)) {
        while(is_name(parser->text[parser->position]) ||
              is_digit(parser->text[parser->position])) {
            ++parser->position;
        }
        const size_t length = parser->position - start;
        const char* name = parser->text + start;
        if(name_equals(name, length, "x")) {
            emit(parser, OpX, 0.0f);
        } else if(name_equals(name, length, "ans")) {
            emit(parser, OpAns, 0.0f);
        } else if(name_equals(name, length, "pi")) {
            emit(parser, OpNumber, MW_PI);
        } else if(name_equals(name, length, "e")) {
            emit(parser, OpNumber, MW_E);
        } else {
            MWOperation operation;
            if(name_equals(name, length, "sin")) operation = OpSin;
            else if(name_equals(name, length, "cos")) operation = OpCos;
            else if(name_equals(name, length, "tan")) operation = OpTan;
            else if(name_equals(name, length, "sqrt")) operation = OpSqrt;
            else if(name_equals(name, length, "ln")) operation = OpLn;
            else if(name_equals(name, length, "log")) operation = OpLog;
            else if(name_equals(name, length, "abs")) operation = OpAbs;
            else {
                fail(parser, MW_ERROR_UNKNOWN_NAME, start);
                return;
            }
            skip_space(parser);
            if(parser->text[parser->position] != '(') {
                fail(parser, MW_ERROR_SYNTAX, parser->position);
                return;
            }
            ++parser->position;
            parse_expression(parser);
            skip_space(parser);
            if(parser->text[parser->position] != ')') {
                fail(parser, MW_ERROR_SYNTAX, parser->position);
                return;
            }
            ++parser->position;
            emit(parser, operation, 0.0f);
        }
    } else {
        fail(parser, MW_ERROR_SYNTAX, start);
        return;
    }
    skip_space(parser);
    while(parser->status == MW_OK && parser->text[parser->position] == '%') {
        ++parser->position;
        emit(parser, OpPercent, 0.0f);
        skip_space(parser);
    }
}

static void parse_power(MWParser* parser) {
    parse_primary(parser);
    skip_space(parser);
    if(parser->status == MW_OK && parser->text[parser->position] == '^') {
        ++parser->position;
        parse_unary(parser);
        emit(parser, OpPower, 0.0f);
    }
}

static void parse_unary(MWParser* parser) {
    if(parser->status != MW_OK) return;
    if(parser->depth >= MW_MAX_PARSE_DEPTH) {
        fail(parser, MW_ERROR_TOO_COMPLEX, parser->position);
        return;
    }
    ++parser->depth;
    skip_space(parser);
    const char c = parser->text[parser->position];
    if(c == '+' || c == '-') {
        ++parser->position;
        parse_unary(parser);
        if(c == '-') emit(parser, OpNegate, 0.0f);
    } else {
        parse_power(parser);
    }
    --parser->depth;
}

static void parse_term(MWParser* parser) {
    parse_unary(parser);
    skip_space(parser);
    while(parser->status == MW_OK) {
        const char c = parser->text[parser->position];
        if(c != '*' && c != '/') break;
        ++parser->position;
        parse_unary(parser);
        emit(parser, c == '*' ? OpMultiply : OpDivide, 0.0f);
        skip_space(parser);
    }
}

static void parse_expression(MWParser* parser) {
    parse_term(parser);
    skip_space(parser);
    while(parser->status == MW_OK) {
        const char c = parser->text[parser->position];
        if(c != '+' && c != '-') break;
        ++parser->position;
        parse_term(parser);
        emit(parser, c == '+' ? OpAdd : OpSubtract, 0.0f);
        skip_space(parser);
    }
}

MWStatus mw_compile(const char* expression, MWProgram* program, size_t* error_position) {
    if(error_position) *error_position = 0;
    if(!program) return MW_ERROR_SYNTAX;
    program->length = 0;
    program->stack_required = 0;
    if(!expression) return MW_ERROR_SYNTAX;

    size_t length = 0;
    while(length < MW_EXPRESSION_CAPACITY && expression[length] != '\0') ++length;
    if(length == MW_EXPRESSION_CAPACITY) {
        if(error_position) *error_position = MW_EXPRESSION_CAPACITY - 1;
        return MW_ERROR_TOO_LONG;
    }
    MWParser parser = {.text = expression, .program = program, .status = MW_OK};
    skip_space(&parser);
    if(parser.text[parser.position] == '\0') return MW_ERROR_EMPTY;
    parse_expression(&parser);
    skip_space(&parser);
    if(parser.status == MW_OK && (parser.text[parser.position] != '\0' || parser.stack != 1)) {
        fail(&parser, MW_ERROR_SYNTAX, parser.position);
    }
    if(parser.status != MW_OK) {
        program->length = 0;
        program->stack_required = 0;
        if(error_position) *error_position = parser.error_position;
    }
    return parser.status;
}

MWStatus mw_evaluate(const MWProgram* program, float x, float ans, bool degrees, float* result) {
    if(!program || !result) return MW_ERROR_SYNTAX;
    if(program->length == 0) return MW_ERROR_EMPTY;
    if(program->length > MW_MAX_TOKENS || program->stack_required > MW_MAX_STACK) {
        return MW_ERROR_TOO_COMPLEX;
    }
    float stack[MW_MAX_STACK];
    unsigned count = 0;
    for(unsigned i = 0; i < program->length; ++i) {
        const MWInstruction instruction = program->instructions[i];
        const MWOperation operation = (MWOperation)instruction.operation;
        float value;
        if(operation <= OpAns) {
            if(count >= MW_MAX_STACK) return MW_ERROR_TOO_COMPLEX;
            value = operation == OpNumber ? instruction.value : (operation == OpX ? x : ans);
            if(!isfinite(value)) return MW_ERROR_OVERFLOW;
            stack[count++] = value;
            continue;
        }
        if(operation <= OpPower) {
            if(count < 2) return MW_ERROR_SYNTAX;
            const float right = stack[--count];
            const float left = stack[count - 1];
            switch(operation) {
            case OpAdd:
                value = left + right;
                break;
            case OpSubtract:
                value = left - right;
                break;
            case OpMultiply:
                value = left * right;
                break;
            case OpDivide:
                if(right == 0.0f) return MW_ERROR_DIV_ZERO;
                value = left / right;
                break;
            case OpPower:
                if(left == 0.0f && right < 0.0f) return MW_ERROR_DIV_ZERO;
                if((left == 0.0f && right == 0.0f) ||
                   (left < 0.0f && floorf(right) != right)) return MW_ERROR_DOMAIN;
                value = powf(left, right);
                break;
            default:
                return MW_ERROR_SYNTAX;
            }
        } else {
            if(count < 1) return MW_ERROR_SYNTAX;
            const float operand = stack[count - 1];
            const float angle = (operation == OpSin || operation == OpCos || operation == OpTan) && degrees ?
                                    fmodf(operand, 360.0f) * (MW_PI / 180.0f) : operand;
            switch(operation) {
            case OpNegate:
                value = -operand;
                break;
            case OpPercent:
                value = operand / 100.0f;
                break;
            case OpSin:
                value = sinf(angle);
                break;
            case OpCos:
                value = cosf(angle);
                break;
            case OpTan:
                if(fabsf(cosf(angle)) < 1.0e-7f) return MW_ERROR_DOMAIN;
                value = tanf(angle);
                break;
            case OpSqrt:
                if(operand < 0.0f) return MW_ERROR_DOMAIN;
                value = sqrtf(operand);
                break;
            case OpLn:
                if(operand <= 0.0f) return MW_ERROR_DOMAIN;
                value = logf(operand);
                break;
            case OpLog:
                if(operand <= 0.0f) return MW_ERROR_DOMAIN;
                value = log10f(operand);
                break;
            case OpAbs:
                value = fabsf(operand);
                break;
            default:
                return MW_ERROR_SYNTAX;
            }
        }
        if(isnan(value)) return MW_ERROR_DOMAIN;
        if(!isfinite(value)) return MW_ERROR_OVERFLOW;
        stack[count - 1] = value;
    }
    if(count != 1) return MW_ERROR_SYNTAX;
    *result = stack[0] == 0.0f ? 0.0f : stack[0];
    return MW_OK;
}

const char* mw_status_message(MWStatus status) {
    switch(status) {
    case MW_OK: return "OK";
    case MW_ERROR_EMPTY: return "Enter an expression";
    case MW_ERROR_SYNTAX: return "Check expression";
    case MW_ERROR_UNKNOWN_NAME: return "Unknown name";
    case MW_ERROR_TOO_LONG: return "Expression too long";
    case MW_ERROR_TOO_COMPLEX: return "Expression too complex";
    case MW_ERROR_DIV_ZERO: return "Cannot divide by zero";
    case MW_ERROR_DOMAIN: return "Outside real domain";
    case MW_ERROR_OVERFLOW: return "Number out of range";
    default: return "Unknown error";
    }
}
