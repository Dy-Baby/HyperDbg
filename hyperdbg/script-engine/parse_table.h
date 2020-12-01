#ifndef PARSE_TABLE_H
#define PARSE_TABLE_H
#include "common.h"
#define RULES_COUNT 56
#define TERMINAL_COUNT 36
#define NONETERMINAL_COUNT 25
#define START_VARIABLE "S"
#define MAX_RHS_LEN 6
#define KEYWORD_LIST_LENGTH 13
#define ONEOPFUNC1_LENGTH 12
#define ONEOPFUNC2_LENGTH 1
extern const struct _TOKEN Lhs[RULES_COUNT];
extern const struct _TOKEN Rhs[RULES_COUNT][MAX_RHS_LEN];
extern const unsigned int RhsSize[RULES_COUNT];
extern const char* NoneTerminalMap[NONETERMINAL_COUNT];
extern const char* TerminalMap[TERMINAL_COUNT];
extern const int ParseTable[NONETERMINAL_COUNT][TERMINAL_COUNT];
extern const char* KeywordList[];
extern const char* OneOpFunc1[];
extern const char* OneOpFunc2[];
#endif