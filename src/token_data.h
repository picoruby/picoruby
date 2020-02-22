static struct {
  char *string;
} KEYWORDS[] = {
  {"BEGIN"},
  {"class"},
  {"ensure"},
  {"nil"},
  {"self"},
  {"when"},
  {"END"},
  {"def"},
  {"false"},
  {"not"},
  {"super"},
  {"while"},
  {"alias"},
  {"defined?"},
  {"for"},
  {"or"},
  {"then"},
  {"yield"},
  {"and"},
  {"do"},
  {"if"},
  {"redo"},
  {"true"},
  {"__LINE__"},
  {"begin"},
  {"else"},
  {"in"},
  {"rescue"},
  {"undef"},
  {"__FILE__"},
  {"break"},
  {"elsif"},
  {"module"},
  {"retry"},
  {"unless"},
  {"__ENCODING__"},
  {"case"},
  {"end"},
  {"next"},
  {"return"},
  {"until"},
  {NULL}
};

static struct {
  char letter;
} OPERATORS_1[] = {
  {'+'},
  {'-'},
  {'*'},
  {'/'},
  {'%'},
  {'&'},
  {'|'},
  {'^'},
  {'!'},
  {'~'},
  {'>'},
  {'<'},
  {'='},
  {'?'},
  {':'},
  {0}
};

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

