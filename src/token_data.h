static struct {
  char *string;
} OPERATORS_2[] = {
  {"**"},
  {"<<"},
  {">>"},
  {"&&"},
  {"||"},
  {"::"},
  {">="},
  {"<="},
  {"=="},
  {"!="},
  {"=~"},
  {"!~"},
  {".."},
  {"=>"},
  {"+="},
  {"-="},
  {"*="},
  {"/="},
  {"%="},
  {"&="},
  {"|="},
  {"^="},
  {NULL}
};

static struct {
  char *string;
} OPERATORS_3[] = {
  {"<=>"},
  {"==="},
  {"..."},
  {"**="},
  {"<<="},
  {">>="},
  {"&&="},
  {"||="},
  {NULL}
};

static struct {
  char letter;
} PARENS[] = {
  {'('},
  {')'},
  {'['},
  {']'},
  {'{'},
  {'}'},
  {0}
};

static struct {
  char letter;
} COMMAS[] = {
  {','},
  {0}
};

static struct {
  char letter;
} SEMICOLONS[] = {
  {';'},
  {0}
};

