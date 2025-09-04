#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include "../include/ast.hpp"
#include "../include/generator.hpp"
#include "../include/ir.hpp"

using namespace std;

extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

int main(int argc, const char *argv[]) {
  if (argc < 5) {
    std::cerr << "Usage: " << argv[0] << " -koopa <input_file> -o <output_file>" << std::endl;
    return 1;
  }

  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  yyin = fopen(input, "r");
  assert(yyin);

  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);

  // ast->Dump();
  // cout << endl;

  IRGenerator generator;
  CompUnitAST* comp_unit_ast = static_cast<CompUnitAST*>(ast.get()); 
  auto koopa_program = generator.Generate(*comp_unit_ast);

  std::ofstream output_file(output);
  if (!output_file.is_open()) {
    std::cerr << "Error: Could not open output file " << output << std::endl;
    return 1;
  }

  koopa_program->Dump(output_file);

  std::cout << "Successfully generated Koopa IR to " << output << std::endl;

  return 0;
}