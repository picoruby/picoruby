#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "my_regex.h"

bool regex_match(char *str, const char *pattern, bool resultRequired, RegexResult result[REGEX_MAX_RESULT_NUM])
{
  int i;
  regex_t regexBuffer;
  regmatch_t match[REGEX_MAX_RESULT_NUM];
  int size;

  if (regcomp(&regexBuffer, pattern, REG_EXTENDED|REG_NEWLINE) != 0){
    printf("regcomp failed\n");
    regfree(&regexBuffer);
    return false;
  }

  size = sizeof(match) / sizeof(regmatch_t);
  if (regexec(&regexBuffer, str, size, match, 0) != 0){
    printf("no match: %s\n", pattern);
    regfree(&regexBuffer);
    return false;
  } else {
    printf("match!: %s\n", pattern);
  }

  if (resultRequired) {
    for (i = 0; i < size; i++){
      int startIndex = match[i].rm_so;
      int endIndex = match[i].rm_eo;
      if (startIndex == -1 || endIndex == -1) {
        continue;
      }
      printf("match[%d] index [start, end] = %d, %d\n", i, startIndex, endIndex);
      strncpy(result[i].value, str + startIndex, endIndex - startIndex);
      result[i].value[endIndex - startIndex] = '\0';
      printf("match result: %s\n", result[i].value);
    }
  }

  regfree(&regexBuffer);
  return true;
}

bool Regex_match2(char *str, const char *pattern)
{
  return regex_match(str, pattern, false, NULL);
}

bool Regex_match3(char *str, const char *pattern, RegexResult result[REGEX_MAX_RESULT_NUM])
{
  return regex_match(str, pattern, true, result);
}
