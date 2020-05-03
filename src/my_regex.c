#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mmrbc.h"
#include "common.h"
#include "my_regex.h"

#ifdef MMRUBY_REGEX_LIBC
  #include <regex.h>
#else
  #include "regex_light.h"
#endif

bool regex_match(char *str, const char *pattern, bool resultRequired, RegexResult result[REGEX_MAX_RESULT_NUM])
{
  int i;
  regex_t regexBuffer;
  regmatch_t match[REGEX_MAX_RESULT_NUM];
  int size;

  if (regcomp(&regexBuffer, pattern, REG_EXTENDED|REG_NEWLINE) != 0){
    FATALP("regcomp failed: /%s/", pattern);
    return false;
  }

  size = sizeof(match) / sizeof(regmatch_t);
  if (regexec(&regexBuffer, str, size, match, 0) != 0){
    DEBUGP("no match: %s", pattern);
    regfree(&regexBuffer);
    return false;
  } else {
    DEBUGP("match!: %s", pattern);
  }

  if (resultRequired) {
    for (i = 0; i < size; i++){
      int startIndex = match[i].rm_so;
      int endIndex = match[i].rm_eo;
      if (startIndex == -1 || endIndex == -1) {
        continue;
      }
      DEBUGP("match[%d] index [start, end] = %d, %d", i, startIndex, endIndex);
      strsafencpy(result[i].value, str + startIndex, endIndex - startIndex, MAX_TOKEN_LENGTH);
      result[i].value[endIndex - startIndex] = '\0';
      DEBUGP("match result: %s", result[i].value);
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
