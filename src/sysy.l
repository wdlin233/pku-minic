%option noyywrap
%option nounput
%option noinput
%option yylineno

%{

#include <cstdlib>
#include <string>
#include <iostream>

// include Bison generated hpp
#include "sysy.tab.hpp"

using namespace std;

extern YYLTYPE yylloc;

int current_token_start_column = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylineno; \
    yylloc.first_column = current_token_start_column; \
    yylloc.last_line = yylineno; \
    yylloc.last_column = current_token_start_column + yyleng - 1; \
    current_token_start_column += yyleng;
%}

/* 标识符 */
Identifier      [a-zA-Z_][a-zA-Z0-9_]*

/* 整数字面量 */
Decimal         [1-9][0-9]*
Octal           0[0-7]*
Hexadecimal     0[xX][0-9a-fA-F]+

%%

[ \t\r]+                            { }
\n                                  { current_token_start_column = 1; }
"//"[^\n]*                          { }
"/*"[^*]*"*"+([^*/][^*]*"*"+)*"/"   { }

"int"           { return INT; }
"return"        { return RETURN; }

"("             { return '('; }
")"             { return ')'; }
"{"             { return '{'; }
"}"             { return '}'; }
";"             { return ';'; }
"+"             { return '+'; }
"-"             { return '-'; }
"!"             { return '!'; }
"*"             { return '*'; }
"/"             { return '/'; }
"%"             { return '%'; }

"<="             { return T_LE; }
">="             { return T_GE; }
"=="             { return T_EQ; }
"!="             { return T_NE; }
"&&"             { return T_LAND; }
"||"             { return T_LOR; }

"<"             { return '<'; }
">"             { return '>'; }

{Identifier}    { yylval.str_val = new string(yytext); return IDENT; }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }

.               { 
                    std::cerr << "Lexical error at " << yylloc.first_line << ":" << yylloc.first_column << ": Unexpected character '" << yytext[0] << "'" << std::endl;
                    return yytext[0];
                }

%%