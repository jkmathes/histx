#ifndef HISTX_CONFIG_H
#define HISTX_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

extern FILE *yyin;
int yyparse();
int yyerror(char *);

char *get_setting(char *key);
bool add_setting(char *key, char *value);
bool load_config(char *config_file);
void destroy_config();

#endif //HISTX_CONFIG_H
