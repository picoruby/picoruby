#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "my_regex.h"

bool regex_match(char *str, const char *pattern, char (*result)[MAX_RESULT_NUM])
{
  int i;
  regex_t regexBuffer;
  regmatch_t match[MAX_RESULT_NUM];
  int size;

  if (regcomp(&regexBuffer, pattern, REG_EXTENDED|REG_NEWLINE) != 0){
    // printf("regcomp failed\n");
    regfree(&regexBuffer);
    return false;
  }

  size = sizeof(match) / sizeof(regmatch_t);
  if (regexec(&regexBuffer, str, size, match, 0) != 0){
    // printf("no match\n");
    regfree(&regexBuffer);
    return false;
  }

  for (i = 0; i < size; i++){
    int startIndex = match[i].rm_so;
    int endIndex = match[i].rm_eo;
    if (startIndex == -1 || endIndex == -1){
      continue;
    }
    // printf("index [start, end] = %d, %d\n", startIndex, endIndex);
    strncpy(result[i], str + startIndex, endIndex - startIndex);
    // printf("%s\n", result[i]);
  }

  regfree(&regexBuffer);
  return true;
}

bool Regex_match2(char *str, const char *pattern)
{
  char (*dummy)[MAX_RESULT_NUM];
  return regex_match(str, pattern, dummy);
}

bool Regex_match3(char *str, const char *pattern, char (*result)[MAX_RESULT_NUM])
{
  return regex_match(str, pattern, result);
}
