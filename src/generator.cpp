#include "../include/generator.hpp"
#include <algorithm>
#include <memory>
#include <ostream>
#include <type_traits>
#include <variant>

std::unique_ptr<Program> IRGenerator::Generate(const CompUnitAST& ast) {
    program = std::make_unique<Program>();
    visit(*ast.func_def);
    return std::move(program);
}

void IRGenerator::visit(const FuncDefAST& func_def) {
    auto func = std::make_unique<Function>();
    func->name = "@" + func_def.ident;
    func->ret_type = std::make_unique<Type>();
    func->ret_type->kind = Type::INTEGER;
    
    auto bb = std::make_unique<BasicBlock>();
    bb->name = "%entry";

    current_function = func.get();
    current_bb = bb.get();

    visit(*func_def.block->stmt);

    func->blocks.push_back(std::move(bb)); // push basic block into basic blocks
    program->functions.push_back(std::move(func));
}

void IRGenerator::visit(const StmtAST& stmt) {
    auto ret_val = visit(*stmt.number);

    auto ret_inst = std::make_unique<Value>(
        Value::Return{ std::move(ret_val) }
    );
    ret_inst->type = std::make_unique<Type>();
    ret_inst->type->kind = Type::INTEGER;

    current_bb->insts.push_back(std::move(ret_inst));
}

std::unique_ptr<Value> IRGenerator::visit(const NumberAST& number) {
    auto val = std::make_unique<Value>(
        Value::Integer{ std::move(number.val) }
    );
    val->name = new_temp_var_name();
    val->type = std::make_unique<Type>();
    val->type->kind = Type::INTEGER;

    return val;
}
