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

([^= \t\n\r]+) { current_token = sdsnew(yytext); return L_TOKEN; }
"\=" { return L_EQUALS; }
[ \t\n\r]+ { ; }
