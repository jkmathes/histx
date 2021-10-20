%{

extern int yylex();

#include <stdio.h>
#include <stdlib.h>
#include "sds.h"
#include "config.h"

extern sds current_token;
#define YYSTYPE sds
%}

%token L_EQUALS L_TOKEN
%start parse
%%

parse: parseList {;};
parseList: parseList row { ; };
           | row { ; };
row: token L_EQUALS token { add_setting($1, $3); sdsfree($1); sdsfree($3); };
token: L_TOKEN { $$ = current_token; };

%%

int yyerror(char *s) {
    //printf("ERROR: %s\n", s);
    return 0;
}
