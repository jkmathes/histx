%{
#include <stdlib.h>
#include <stdio.h>
#include "sds.h"
#include "config.h"
#include "config.y.h"

sds current_token;
%}

%option noyywrap

%%

[tT][rR][uU][eE] { current_token = sdsnew("true"); return L_TOKEN; }
[fF][aA][lL][sS][eE] { current_token = sdsnew("false"); return L_TOKEN; }
([^= \t\n\r]+) { current_token = sdsnew(yytext); return L_TOKEN; }
"\=" { return L_EQUALS; }
[ \t\n\r]+ { ; }
