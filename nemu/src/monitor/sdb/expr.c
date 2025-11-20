/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include "memory/paddr.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
    TK_NOTYPE = 256,
    TK_EQ,
    TK_NEQ,
    TK_AND,
    TK_DEC,
    TK_HEX,
    TK_PLUS,
    TK_MINUS,
    TK_MULTIPLY,
    TK_DIVIDE,
    TK_LPAR,
    TK_RPAR,
    TK_REG,   // register
    TK_DEREF, // dereference
};

static struct rule {
    const char *regex;
    int token_type;
} rules[] = {
    {" +", TK_NOTYPE},             // spaces
    {"\\+", TK_PLUS},              // plus
    {"-", TK_MINUS},               // minus
    {"\\*", TK_MULTIPLY},          // multiply or dereference
    {"/", TK_DIVIDE},              // divide
    {"\\(", TK_LPAR},              // left parenthesis
    {"\\)", TK_RPAR},              // right parenthesis
    {"0[xX][0-9a-fA-F]+", TK_HEX}, // hex number
    {"[0-9]+", TK_DEC},            // decimal number
    {"\\$[a-zA-Z0-9]+", TK_REG},   // register
    {"==", TK_EQ},                 // equal
    {"!=", TK_NEQ},                // not equal
    {"&&", TK_AND},                // logical and
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
    int i;
    char error_msg[128];
    int ret;

    for (i = 0; i < NR_REGEX; i++) {
        ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
        if (ret != 0) {
            regerror(ret, &re[i], error_msg, 128);
            panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
        }
    }
}

typedef struct token {
    int type;
    char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e) {
    int position = 0;
    int i;
    regmatch_t pmatch;

    nr_token = 0;

    while (e[position] != '\0') {
        /* Try all rules one by one. */
        for (i = 0; i < NR_REGEX; i++) {
            if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                char *substr_start = e + position;
                int substr_len = pmatch.rm_eo;

                // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
                //     i, rules[i].regex, position, substr_len, substr_len, substr_start);

                position += substr_len;

                switch (rules[i].token_type) {
                    case TK_NOTYPE:
                        break;
                    case TK_DEC:
                    case TK_HEX:
                    case TK_REG:
                        tokens[nr_token].type = rules[i].token_type;
                        if (substr_len >= 32) {
                            substr_len = 31;
                        }
                        strncpy(tokens[nr_token].str, substr_start, substr_len);
                        tokens[nr_token].str[substr_len] = '\0';
                        nr_token++;
                        break;
                    default:
                        tokens[nr_token].type = rules[i].token_type;
                        nr_token++;
                        break;
                }

                break;
            }
        }

        if (i == NR_REGEX) {
            printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
            return false;
        }
    }

    return true;
}

static bool check_parentheses(int p, int q) {
    if (tokens[p].type != TK_LPAR || tokens[q].type != TK_RPAR) {
        return false;
    }
    int cur = 0;
    for (int i = p + 1; i < q; i++) {
        if (tokens[i].type == TK_LPAR) {
            cur++;
        } else if (tokens[i].type == TK_RPAR) {
            cur--;
        }
        if (cur == 0 && i < q) {
            return false;
        }
        if (cur < 0) {
            return false;
        }
    }
    return cur == 0;
}

word_t eval(int p, int q, bool *success) {
    if (*success == false) {
        return 0;
    }
    if (p > q) {
        *success = false;
        printf("Bad expression\n");
        return 0;
    } else if (p == q) {
        word_t res = 0;
        if (tokens[p].type == TK_DEC) {
            res = strtoul(tokens[p].str, NULL, 10);
        } else if (tokens[p].type == TK_HEX) {
            res = strtoul(tokens[p].str, NULL, 16);
        } else if (tokens[p].type == TK_REG) {
            res = isa_reg_str2val(tokens[p].str + 1, success);
            if (!success) {
                printf("Unknown register: %s\n", tokens[p].str);
            }
        } else {
            *success = false;
            printf("Invalid single token type at pos %d\n", p);
        }
        return res;
    } else if (check_parentheses(p, q)) {
        /* The expression is surrounded by a matched pair of parentheses.
    	* If that is the case, just throw away the parentheses.
    	*/
        return eval(p + 1, q - 1, success);
    } else {
        // level: () > deref > * / > + - > == != > &&
        int op = -1;
        int min_priority = 1000;
        int parens_layer = 0;

        for (int i = p; i <= q; i++) {
            int type = tokens[i].type;
            int priority = 0;

            if (type == '(') {
                parens_layer++;
                continue;
            } else if (type == ')') {
                parens_layer--;
                continue;
            }

            if (parens_layer > 0) {
                continue;
            }
            if (type == TK_DEC || type == TK_HEX || type == TK_REG) {
                continue;
            }

            if (type == TK_EQ || type == TK_NEQ) {
                priority = 1;
            } else if (type == TK_PLUS || type == TK_MINUS) {
                priority = 4;
            } else if (type == TK_MULTIPLY || type == TK_DIVIDE) {
                priority = 5;
            } else if (type == TK_DEREF) {
                priority = 10;
            }

            if (priority <= min_priority) {
                min_priority = priority;
                op = i;
            }

            if (type == TK_DEREF) {
                if (priority < min_priority) {
                    min_priority = priority;
                    op = i;
                }
            } else {
                if (priority <= min_priority) {
                    min_priority = priority;
                    op = i;
                }
            }
        }

        if (op == -1) {
            if (tokens[p].type == TK_DEREF) {
                word_t addr = eval(p + 1, q, success);
                return paddr_read(addr, 4);
            }
            *success = false;
            printf("Cannot find dominant operator in range %d-%d\n", p, q);
            return 0;
        }

        if (tokens[op].type == TK_DEREF) {
            word_t val = eval(op + 1, q, success);
            return paddr_read(val, 4);
        }

        word_t val1 = eval(p, op - 1, success);
        word_t val2 = eval(op + 1, q, success);

        switch (tokens[op].type) {
            case TK_PLUS:
                return val1 + val2;

            case TK_MINUS:
                return val1 - val2;
            case TK_MULTIPLY:
                return val1 * val2;
            case TK_DIVIDE:
                if (val2 == 0) {
                    printf("Division by zero\n");
                    *success = false;
                    return 0;
                }
                return val1 / val2;
            case TK_EQ:
                return val1 == val2;
            case TK_NEQ:
                return val1 != val2;
                // default: TODO();
        }
    }
    return 0;
}

word_t expr(char *e, bool *success) {
    if (!make_token(e)) {
        *success = false;
        return 0;
    }

    // find dereference token
    for (int i = 0; i < nr_token; i++) {
        if (tokens[i].type == TK_MULTIPLY) {
            bool isDeref = false;
            if (i == 0) {
                isDeref = true;
            } else {
                int type = tokens[i - 1].type;
                if (type == TK_PLUS || type == TK_MINUS || type == TK_MULTIPLY
                    || type == TK_DIVIDE || type == TK_LPAR || type == TK_EQ
                    || type == TK_NEQ || type == TK_AND || type == TK_DEREF) {
                    isDeref = true;
                }
            }
            if (isDeref) {
                tokens[i].type = TK_DEREF;
            }
        }
    }

    *success = true;
    int res = eval(0, nr_token - 1, success);

    return res;
}
