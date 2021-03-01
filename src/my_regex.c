#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mmrbc.h"
#include "common.h"
#include "my_regex.h"

bool regex_match(char *str, const char *pattern, bool resultRequired, RegexResult *result)
{
  int i;
  regex_t preg;
  int size;

  if (regcomp(&preg, pattern, REG_EXTENDED|REG_NEWLINE) != 0){
    FATALP("regcomp failed: /%s/", pattern);
    return false;
  }

  size = preg.re_nsub + 1;
  regmatch_t pmatch[size]; // number of subexpression + 1

  if (regexec(&preg, str, size, pmatch, 0) != 0){
    DEBUGP("no match: %s", pattern);
    regfree(&preg);
    return false;
  } else {
    DEBUGP("match!: %s", pattern);
  }

  if (resultRequired) {
    for (i = 0; i < size; i++){
      int startIndex = pmatch[i].rm_so;
      int endIndex = pmatch[i].rm_eo;
      if (startIndex == -1 || endIndex == -1) {
        continue;
      }
      DEBUGP("match[%d] index [start, end] = %d, %d", i, startIndex, endIndex);
      strsafencpy((result + i)->value, str + startIndex, endIndex - startIndex, MAX_TOKEN_LENGTH);
      (result + i)->value[endIndex - startIndex] = '\0';
      DEBUGP("match result: %s", (result + i)->value);
    }
  }

  regfree(&preg);
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
