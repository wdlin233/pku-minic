%code requires {
  #include <memory>
  #include <string>
  #include <optional>
  #include "ast.hpp"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <optional>
#include "ast.hpp"
#include "sysy.tab.hpp"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

// 声明全局的 yylloc 变量，它将由 Flex 填充并被 Bison 使用
extern YYLTYPE yylloc;

%}

%define parse.error verbose

// generate int yyparse(std::unique_ptr<std::string> &ast)
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 使用字符串指针而不直接用 string 或者 unique_ptr<string> 以避免内存泄漏等风险
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<std::unique_ptr<ConstDefAST>> *vec_defs;
  std::vector<std::unique_ptr<BlockItemAST>> *vec_items;
  std::vector<std::unique_ptr<VarDefAST>> *vec_var_defs;
}

%locations

// lexer 返回的所有 token 种类的声明
%token INT RETURN CONST IF ELSE
%token T_LE T_GE T_EQ T_NE T_LAND T_LOR
%token <str_val> IDENT
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> FuncDef FuncType Block BlockItem Number 
%type <ast_val> Exp PrimaryExp AddExp MulExp UnaryExp RelExp EqExp LAndExp LOrExp
%type <ast_val> Decl ConstDecl ConstDef ConstInitVal VarDecl VarDef InitVal LVal ConstExp
%type <ast_val> Stmt OpenStmt ClosedStmt OtherStmt
%type <vec_defs> ConstDefList
%type <vec_items> BlockItemList
%type <vec_var_defs> VarDefList

%%

// 此处的 ast 是 yyparse 函数的参数，在 %parse-param 中定义了其为 std::unique_ptr
// CompUnit ::= FuncDef;
CompUnit
  : FuncDef {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<FuncDefAST>(static_cast<FuncDefAST*>($1));
    ast = move(comp_unit);
  }
  ;

// FuncDef ::= FuncType IDENT "(" ")" Block;
FuncDef
  : FuncType IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<FuncTypeAST>(static_cast<FuncTypeAST*>($1));
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BlockAST>(static_cast<BlockAST*>($5));
    $$ = ast;
  }
  ;

// FuncType ::= "int";
FuncType
  : INT {
    $$ = new FuncTypeAST(FuncTypeAST::Type::TYPE_INT);
  }
  ;

// Block ::= "{" {BlockItem} "}";
Block
  : '{' BlockItemList '}' {
    auto ast = new BlockAST();
    ast->items = std::move(*$2);
    delete $2;
    $$ = ast;
  }
  | '{' '}' {
    $$ = new BlockAST();
  }
  ;

BlockItemList
  : BlockItem {
    auto list = new std::vector<std::unique_ptr<BlockItemAST>>();
    list->push_back(std::unique_ptr<BlockItemAST>(static_cast<BlockItemAST*>($1)));
    $$ = list; 
  }
  | BlockItemList BlockItem {
    $1->push_back(std::unique_ptr<BlockItemAST>(static_cast<BlockItemAST*>($2)));
    $$ = $1; 
  }
  ;

// BlockItem ::= Decl | Stmt;
BlockItem
  : Decl { $$ = $1; }
  | Stmt { $$ = $1; }
  ;

// Decl ::= ConstDecl | VarDecl;
Decl
  : ConstDecl { $$ = $1; }
  | VarDecl { $$ = $1; }
  ;

// ConstDecl ::= "const" BType ConstDef {"," ConstDef} ";";
ConstDecl 
  : CONST BType ConstDefList ';' {
    auto ast = new ConstDeclAST();
    ast->const_defs = std::move(*$3);
    delete $3;
    $$ = ast;
  }
  ;

BType 
  : INT {}
  ;

ConstDefList 
  : ConstDef {
    auto list = new std::vector<std::unique_ptr<ConstDefAST>>();
    list->push_back(std::unique_ptr<ConstDefAST>(static_cast<ConstDefAST*>($1)));
    $$ = list; 
  }
  | ConstDefList ',' ConstDef {
    $1->push_back(std::unique_ptr<ConstDefAST>(static_cast<ConstDefAST*>($3)));
    $$ = $1; 
  }
  ;

// ConstDef ::= IDENT "=" ConstInitVal;
ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = ast;
  }
  ;

// ConstInitVal ::= ConstExp;
ConstInitVal
  : ConstExp { $$ = $1; }
  ;

// ConstExp ::= Exp;
ConstExp
  : Exp { $$ = $1; }
  ;

// VarDecl ::= BType VarDef {"," VarDef} ";";
VarDecl
  : BType VarDefList ';' {
    auto ast = new VarDeclAST();
    ast->var_defs = std::move(*$2);
    delete $2;
    $$ = ast;
  }
  ;

VarDefList
  : VarDef {
    auto list = new std::vector<std::unique_ptr<VarDefAST>>();
    list->push_back(std::unique_ptr<VarDefAST>(static_cast<VarDefAST*>($1)));
    $$ = list;
  }
  | VarDefList ',' VarDef {
    $1->push_back(std::unique_ptr<VarDefAST>(static_cast<VarDefAST*>($3)));
    $$ = $1;
  }
  ;

// VarDef ::= IDENT | IDENT "=" InitVal;
VarDef 
  : IDENT {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = std::nullopt;
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = make_optional<std::unique_ptr<ExprAST>>(static_cast<ExprAST*>($3));
    $$ = ast;
  }
  ;

// InitVal ::= Exp;
InitVal
  : Exp { $$ = $1; }
  ;

// PrimaryExp ::= "(" Exp ")" | LVal | Number;
PrimaryExp
  : '(' Exp ')' { $$ = $2; }
  | LVal        { $$ = $1; }
  | Number      { $$ = $1; }
  ;

// LVal ::= IDENT;
LVal
  : IDENT {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  ;

/* Stmt ::= LVal "=" Exp ";"
        | [Exp] ";"
        | Block
        | "if" "(" Exp ")" Stmt ["else" Stmt]
        | "return" [Exp] ";"
        ; 
        
open_statement: IF '(' expression ')' statement
        | IF '(' expression ')' closed_statement ELSE open_statement
        ;

closed_statement: non_if_statement
        | IF '(' expression ')' closed_statement ELSE closed_statement
        ;
*/
Stmt
  : OpenStmt { $$ = $1; }
  | ClosedStmt { $$ = $1; }
  ;

OpenStmt
  : IF '(' Exp ')' Stmt {
    auto ast = new IfStmtAST();
    ast->condition = unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    ast->then_stmt = unique_ptr<StmtAST>(static_cast<StmtAST*>($5));
    $$ = ast;
  }
  | IF '(' Exp ')' ClosedStmt ELSE OpenStmt {
    auto ast = new IfStmtAST();
    ast->condition = unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    ast->then_stmt = unique_ptr<StmtAST>(static_cast<StmtAST*>($5));
    ast->else_stmt = unique_ptr<StmtAST>(static_cast<StmtAST*>($7));
    $$ = ast;
  }
  ;

ClosedStmt
  : IF '(' Exp ')' ClosedStmt ELSE ClosedStmt {
    auto ast = new IfStmtAST();
    ast->condition = unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    ast->then_stmt = unique_ptr<StmtAST>(static_cast<StmtAST*>($5));
    ast->else_stmt = unique_ptr<StmtAST>(static_cast<StmtAST*>($7));
    $$ = ast;
  }
  | OtherStmt { $$ = $1; }
  ;

OtherStmt
  : LVal '=' Exp ';' {
    auto ast = new AssignStmtAST();
    ast->lval = unique_ptr<LValAST>(static_cast<LValAST*>($1));
    ast->expression = unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new ExprStmtAST();
    ast->expression = unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    $$ = ast;
  }
  | ';' {
    auto ast = new ExprStmtAST();
    ast->expression = std::nullopt;
    $$ = ast;
  }
  | Block {
    auto ast = new BlockStmtAST();
    ast->block = unique_ptr<BlockAST>(static_cast<BlockAST*>($1));
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new ReturnStmtAST();
    ast->expression = unique_ptr<ExprAST>(static_cast<ExprAST*>($2));
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new ReturnStmtAST();
    ast->expression = std::nullopt;
    $$ = ast;
  }
  ;

// Exp ::= LOrExp;
Exp
  : LOrExp { $$ = $1; }
  ;

// LOrExp ::= LAndExp | LOrExp "||" LAndExp;
LOrExp 
  : LAndExp { $$ = $1; }
  | LOrExp T_LOR LAndExp { 
    auto expr = new BinaryExprAST();
    expr->op = T_LOR;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  ;

// LAndExp ::= EqExp | LAndExp "&&" EqExp;
LAndExp
  : EqExp { $$ = $1; }
  | LAndExp T_LAND EqExp { 
    auto expr = new BinaryExprAST();
    expr->op = T_LAND;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  ;

// EqExp ::= RelExp | EqExp ("==" | "!=") RelExp;
EqExp
  : RelExp { $$ = $1; }
  | EqExp T_EQ RelExp { 
    auto expr = new BinaryExprAST();
    expr->op = T_EQ;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  | EqExp T_NE RelExp {
    auto expr = new BinaryExprAST();
    expr->op = T_NE;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
   }
  ;

// RelExp ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
RelExp
  : AddExp { $$ = $1; }
  | RelExp '<' AddExp {
    auto expr = new BinaryExprAST();
    expr->op = '<';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
   }
  | RelExp '>' AddExp {
    auto expr = new BinaryExprAST();
    expr->op = '>';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
   }
  | RelExp T_LE AddExp {
    auto expr = new BinaryExprAST();
    expr->op = T_LE;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
   }
  | RelExp T_GE AddExp {
    auto expr = new BinaryExprAST();
    expr->op = T_GE;
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
   }
  ;

// AddExp ::= MulExp | AddExp ("+" | "-") MulExp;
AddExp
  : MulExp { $$ = $1; }
  | AddExp '+' MulExp { 
    auto expr = new BinaryExprAST();
    expr->op = '+';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  | AddExp '-' MulExp { 
    auto expr = new BinaryExprAST();
    expr->op = '-';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  ;

// MulExp ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
MulExp
  : UnaryExp { $$ = $1; }
  | MulExp '*' UnaryExp { 
    auto expr = new BinaryExprAST();
    expr->op = '*';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  | MulExp '/' UnaryExp { 
    auto expr = new BinaryExprAST();
    expr->op = '/';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  | MulExp '%' UnaryExp { 
    auto expr = new BinaryExprAST();
    expr->op = '%';
    expr->lhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($1));
    expr->rhs = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($3));
    $$ = expr; 
  }
  ;

// UnaryExp ::= PrimaryExp | UnaryOp UnaryExp;
UnaryExp
  : PrimaryExp { $$ = $1; }
  | '+' UnaryExp {
    auto expr = new UnaryExprAST();
    expr->op = '+';
    expr->operand = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($2));
    $$ = expr;
  }
  | '-' UnaryExp {
    auto expr = new UnaryExprAST();
    expr->op = '-';
    expr->operand = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($2));
    $$ = expr;
  }
  | '!' UnaryExp {
    auto expr = new UnaryExprAST();
    expr->op = '!';
    expr->operand = std::unique_ptr<ExprAST>(static_cast<ExprAST*>($2));
    $$ = expr;
  }
  ;

Number
  : INT_CONST {
    $$ = new NumberAST($1);
  }
  ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "Error at: " << yylloc.first_line << ":" << yylloc.first_column << ":" << s << endl;
}
