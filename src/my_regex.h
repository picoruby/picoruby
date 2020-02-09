#ifndef MMRBC_REGEX_H_
#define MMRBC_REGEX_H_

#define MAX_RESULT_NUM 1
#define MAX_PATTERN_LENGTH 30

bool Regex_match2(char *str, const char *pattern);

bool Regex_match3(char *str, const char *pattern, char (*result)[MAX_RESULT_NUM]);

#endif
