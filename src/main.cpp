#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include "../include/ast.hpp"
#include "../include/generator.hpp"
#include "../include/ir.hpp"

using namespace std;

extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

int main(int argc, const char *argv[]) {
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  yyin = fopen(input, "r");
  assert(yyin);

  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);

  ast->Dump();
  cout << endl;

  IRGenerator generator;
  CompUnitAST* comp_unit_ast = static_cast<CompUnitAST*>(ast.get()); 
  auto koopa_program = generator.Generate(*comp_unit_ast);

  koopa_program->Dump(std::cout);

  return 0;
}