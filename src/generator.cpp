#include "../include/generator.hpp"
#include "sysy.tab.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

std::unique_ptr<Program> IRGenerator::Generate(const CompUnitAST& ast) {
    program = std::make_unique<Program>();
    
    enter_scope(); // enter global scope

    register_builtin_functions();

    for (const auto& func_def : ast.func_defs) {
        if (symbol_tables[0].count(func_def->ident)) {
            std::cerr << "Semantic Error: Redefintion Function." << std::endl;
            exit(1);
        }
        FunctionSymbolInfo info;
        info.ret_type = func_def->func_type->type;
        for (const auto& param : func_def->params) {
            info.params.push_back(param.get());
        }
        symbol_tables[0][func_def->ident] = SymbolInfo{info};
    }
    for (const auto& func : ast.func_defs) {
        visit(*func);
    }

    exit_scope();
    return std::move(program);
}

void IRGenerator::visit(const FuncDefAST& func_def) {
    auto func = std::make_unique<Function>();
    func->name = "@" + func_def.ident;
    func->ret_type = std::make_unique<Type>();
    if (func_def.func_type->type == FuncTypeAST::TYPE_VOID) {
        func->ret_type->kind = Type::VOID;
    } else {
        func->ret_type->kind = Type::INTEGER;
    }
    
    auto bb = std::make_unique<BasicBlock>();
    bb->name = "%entry";

    current_function = func.get();
    current_bb = bb.get();
    name_counters.clear();

    enter_scope();
    
    for (const auto& param : func_def.params) {
        // 1. Function Argument
        auto arg = std::make_unique<Value>(Value::Argument{});
        arg->name = "@" + param->ident;
        arg->type = std::make_unique<Type>();
        arg->type->kind = Type::INTEGER;
        const Value* arg_ptr = arg.get();
        func->params.push_back(std::move(arg));

        // 2. Allocate stack space
        auto alloc = std::make_unique<Value>(Value::Alloc{});
        alloc->name = new_temp_var_name();
        alloc->type = std::make_unique<Type>();
        alloc->type->kind = Type::POINTER;
        const Value* alloc_ptr = alloc.get();
        current_bb->insts.push_back(std::move(alloc));

        // 3. Store argument to stack
        auto store_val = std::make_unique<Value>(Value::SymbolRef{arg_ptr});
        auto store = std::make_unique<Value>(Value::Store{std::move(store_val), alloc_ptr});
        current_bb->insts.push_back(std::move(store));

        // 4. Update symbol table
        SymbolInfo info;
        info.kind = alloc_ptr;
        symbol_tables.back()[param->ident] = info;
    }

    for (const auto& item: func_def.block->items) {
        visit(*item);
    }

    if (!current_bb->has_terminator()) {
        if (current_function->ret_type->kind == Type::VOID) {
            // implicit return
            auto ret_inst = std::make_unique<Value>(Value::Return{std::nullopt});
            current_bb->insts.push_back(std::move(ret_inst));
        } else {
            // UB, undefine behavior
            // return default value 0
            auto ret_val = std::make_unique<Value>(Value::Integer{0});
            auto ret_inst = std::make_unique<Value>(Value::Return{ std::move(ret_val) });
            current_bb->insts.push_back(std::move(ret_inst));
        }
    }

    exit_scope();
    
    func->blocks.push_back(std::move(bb)); // push basic block into basic blocks
    program->functions.push_back(std::move(func));
}

void IRGenerator::visit(const BlockItemAST& item) {
    // BlockItem ::= Decl | Stmt;
    // entry for all declarations and statements 
    if (current_bb->has_terminator()) {
        return;
    }

    if (auto stmt = dynamic_cast<const StmtAST*>(&item)) {
        visit(*stmt);
        return;
    }

    if (auto decl = dynamic_cast<const DeclAST*>(&item)) {
        visit(*decl);
        return;
    }

    assert(false && "Unknown BlockItem type");
}

void IRGenerator::visit(const DeclAST& decl) {
    if (auto const_decl = dynamic_cast<const ConstDeclAST*>(&decl)) {
        visit(*const_decl);
        return;
    }
    if (auto var_decl = dynamic_cast<const VarDeclAST*>(&decl)) {
        visit(*var_decl);
        return;
    }
}

void IRGenerator::visit(const ConstDeclAST& const_decl) {
    for (const auto& def: const_decl.const_defs) {
        if (symbol_tables.back().count(def->ident)) {
            std::cerr << "Semantic Error: Redefinition of constant '" << def->ident << "'" << std::endl;
            exit(1);
        }
        int const_value = evaluate_const_expr(*def->init_val);
        
        SymbolInfo info;
        info.kind = const_value;
        symbol_tables.back()[def->ident] = info;
    }
}

void IRGenerator::visit(const VarDeclAST& var_decl) {
    for (const auto& var_def : var_decl.var_defs) {
        if (symbol_tables.back().count(var_def->ident)) {
            std::cerr << "Semantic Error: Redefinition of variable '" << var_def->ident << "'" << std::endl;
            exit(1);
        }
        std::string base_name = var_def->ident;
        name_counters[base_name]++;
       
        auto alloc_inst = std::make_unique<Value>(Value::Alloc{});
        alloc_inst->name = "@" + base_name + '_' + std::to_string(name_counters[base_name]);  // @x_1 = alloc i32
        alloc_inst->type = std::make_unique<Type>();
        alloc_inst->type->kind = Type::POINTER;

        const Value* var_ptr = alloc_inst.get();
        current_bb->insts.push_back(std::move(alloc_inst));

        SymbolInfo info;
        info.kind = var_ptr;
        symbol_tables.back()[var_def->ident] = info;

        if (var_def->init_val.has_value()) {
            auto init_val = visit(*var_def->init_val.value());
            auto store_inst = std::make_unique<Value>(
                Value::Store{std::move(init_val), var_ptr}
            );
            current_bb->insts.push_back(std::move(store_inst));
        }
    }
}

const SymbolInfo* IRGenerator::find_symbol(const std::string sym) {
    for (auto it = symbol_tables.rbegin(); it != symbol_tables.rend(); it++) {
        if (it->count(sym)) {
            return &it->at(sym);
        }
    }
    return nullptr; // couldn't find
}

int IRGenerator::evaluate_const_expr(const ExprAST& expr) {
    if (auto number = dynamic_cast<const NumberAST*>(&expr)) {
        return number->val;
    }
    
    if (auto binary = dynamic_cast<const BinaryExprAST*>(&expr)) {
        int lhs_val = evaluate_const_expr(*binary->lhs);
        int rhs_val = evaluate_const_expr(*binary->rhs);
        
        switch (binary->op) {
            case '+': return lhs_val + rhs_val;
            case '-': return lhs_val - rhs_val;
            case '*': return lhs_val * rhs_val;
            case '/': return lhs_val / rhs_val;
            case '%': return lhs_val % rhs_val;
            case '>': return lhs_val > rhs_val ? 1 : 0;
            case '<': return lhs_val < rhs_val ? 1 : 0;
            case '&': return lhs_val & rhs_val;
            case '|': return lhs_val | rhs_val;
            case T_LE: return lhs_val <= rhs_val ? 1 : 0;
            case T_GE: return lhs_val >= rhs_val ? 1 : 0;
            case T_EQ: return lhs_val == rhs_val ? 1 : 0;
            case T_NE: return lhs_val != rhs_val ? 1 : 0;
            case T_LAND: return (lhs_val != 0 && rhs_val != 0) ? 1 : 0;
            case T_LOR: return (lhs_val != 0 || rhs_val != 0) ? 1 : 0;
            default:
                assert(false && "Unsupported binary operator in constant expression");
        }
    }
    
    if (auto unary = dynamic_cast<const UnaryExprAST*>(&expr)) {
        int operand_val = evaluate_const_expr(*unary->operand);
        
        switch (unary->op) {
            case '+': return operand_val;
            case '-': return -operand_val;
            case '!': return operand_val == 0 ? 1 : 0;
            default:
                assert(false && "Unsupported unary operator in constant expression");
        }
    }
    
    if (auto lval = dynamic_cast<const LValAST*>(&expr)) {
        const SymbolInfo* info = find_symbol(lval->ident);
        if (!info) {
            std::cerr << "Semantic Error: Use of undefined identifier '" << lval->ident << "' in a constant expression." << std::endl;
            exit(1);
        }
        
        if (std::holds_alternative<int>(info->kind)) {
            return std::get<int>(info->kind);
        } else {
            // 在进行常量求值时, 从符号表里查询到了变量而不是常量.
            std::cerr << "Semantic Error: Variable '" << lval->ident << "' cannot be used in a constant expression." << std::endl;
            exit(1);
        }
    }
    
    assert(false && "Unsupported expression type in constant expression");
    return 0;
}

void IRGenerator::visit(const StmtAST& stmt) {
    if (auto assign_stmt = dynamic_cast<const AssignStmtAST*>(&stmt)) {
        visit(*assign_stmt);
        return;
    }
    if (auto return_stmt = dynamic_cast<const ReturnStmtAST*>(&stmt)) {
        visit(*return_stmt);
        return;
    }
    if (auto block_stmt = dynamic_cast<const BlockStmtAST*>(&stmt)) {
        visit(*block_stmt);
        return;
    }
    if (auto expr_stmt = dynamic_cast<const ExprStmtAST*>(&stmt)) {
        visit(*expr_stmt);
        return;
    }
    if (auto if_stmt = dynamic_cast<const IfStmtAST*>(&stmt)) {
        visit(*if_stmt);
        return;
    }
    if (auto while_stmt = dynamic_cast<const WhileStmtAST*>(&stmt)) {
        visit(*while_stmt);
        return;
    }
    if (auto break_stmt = dynamic_cast<const BreakStmtAST*>(&stmt)) {
        visit(*break_stmt);
        return;
    }
    if (auto continue_stmt = dynamic_cast<const ContinueStmtAST*>(&stmt)) {
        visit(*continue_stmt);
        return;
    }

    assert(false && "Unknown statement type");
}

void IRGenerator::visit(const AssignStmtAST& assign_stmt) {
    if (current_bb->has_terminator()) {
        return;
    }
    
    const SymbolInfo* info = find_symbol(assign_stmt.lval->ident);
    if (!info) {
        std::cerr << "Semantic Error: Undefined identifier '" << assign_stmt.lval->ident << "'" << std::endl;
        exit(1);
    }

    if (std::holds_alternative<int>(info->kind)) {
        // 在处理赋值语句时, 赋值语句左侧的 LVal 对应一个常量, 而不是变量.
        std::cerr << "Semantic Error: Cannot assign to constant '" << assign_stmt.lval->ident << "'" << std::endl;
        exit(1);
    } else {
        // variable
        const Value* var_ptr = std::get<const Value*>(info->kind);
        assert(var_ptr->type && var_ptr->type->kind == Type::POINTER && "LVal must resolve to an address");
        auto rval = visit(*assign_stmt.expression);
        auto store_inst = std::make_unique<Value>(
            Value::Store{std::move(rval), var_ptr}
        );
        current_bb->insts.push_back(std::move(store_inst));
    }
}

void IRGenerator::visit(const ReturnStmtAST& return_stmt) {
    if (current_bb->has_terminator()) {
        return;
    }

    std::unique_ptr<Value> ret_val;
    if (return_stmt.expression.has_value()) {
        ret_val = visit(*return_stmt.expression.value());
    } else {
        std::cerr << "Semantic Error: Return stmt is nullopt." << std::endl;
        exit(1);
    }
    auto ret_inst = std::make_unique<Value>(
        Value::Return{ std::move(ret_val) }
    );
    ret_inst->type = std::make_unique<Type>();
    ret_inst->type->kind = Type::INTEGER;

    current_bb->insts.push_back(std::move(ret_inst));
}

void IRGenerator::visit(const BlockStmtAST& block_stmt) {
    enter_scope();
    for (const auto& item : block_stmt.block->items) {
        visit(*item);
    }
    exit_scope();
}

void IRGenerator::visit(const ExprStmtAST& expr_stmt) {
    if (current_bb->has_terminator()) {
        return;
    }

    if (expr_stmt.expression.has_value()) {
        visit(*expr_stmt.expression.value());
    }
}

void IRGenerator::visit(const IfStmtAST& if_stmt) {
    std::unique_ptr<BasicBlock> then_bb = std::make_unique<BasicBlock>();
    then_bb->name = new_branch_name("then");
    std::unique_ptr<BasicBlock> else_bb = nullptr;
    if (if_stmt.else_stmt.has_value()) {
        else_bb = std::make_unique<BasicBlock>();
        else_bb->name = new_branch_name("else");
    }
    std::unique_ptr<BasicBlock> end_bb = std::make_unique<BasicBlock>();
    end_bb->name = new_branch_name("end");

    // Add insts to BasicBlocks
    BasicBlock* then_bb_ptr = then_bb.get();
    BasicBlock* else_bb_ptr = else_bb ? else_bb.get() : end_bb.get();
    BasicBlock* end_bb_ptr = end_bb.get();

    visit_as_condition(*if_stmt.condition, then_bb_ptr, else_bb_ptr);

    current_bb = then_bb_ptr;
    visit(*if_stmt.then_stmt);
    if (!current_bb->has_terminator()) {
        // not terminated by instruction such as return
        auto jump_end = std::make_unique<Value>(Value::Jump{end_bb_ptr});
        current_bb->insts.push_back(std::move(jump_end));
    }

    if (if_stmt.else_stmt.has_value()) {
        current_bb = else_bb.get();
        visit(*if_stmt.else_stmt.value());
        if (!current_bb->has_terminator()) {
            auto jump_end = std::make_unique<Value>(Value::Jump{end_bb_ptr});
            current_bb->insts.push_back(std::move(jump_end));
        }
    }

    current_function->blocks.push_back(std::move(then_bb));
    if (if_stmt.else_stmt.has_value()) current_function->blocks.push_back(std::move(else_bb));
    current_function->blocks.push_back(std::move(end_bb));
    current_bb = end_bb_ptr;
}

void IRGenerator::visit(const WhileStmtAST& while_stmt) {
    std::unique_ptr<BasicBlock> while_entry = std::make_unique<BasicBlock>();
    while_entry->name = new_while_name("while_entry");
    std::unique_ptr<BasicBlock> while_body = std::make_unique<BasicBlock>();
    while_body->name = new_while_name("while_body");
    std::unique_ptr<BasicBlock> while_continue = std::make_unique<BasicBlock>();
    while_continue->name = new_while_name("while_continue");
    std::unique_ptr<BasicBlock> while_end = std::make_unique<BasicBlock>();
    while_end->name = new_while_name("while_end");

    BasicBlock* while_entry_ptr = while_entry.get();
    BasicBlock* while_body_ptr = while_body.get();
    BasicBlock* while_continue_ptr = while_continue.get();
    BasicBlock* while_end_ptr = while_end.get();

    if (!current_bb->has_terminator()) {
        auto jump_while_entry = std::make_unique<Value>(Value::Jump{while_entry_ptr});
        current_bb->insts.push_back(std::move(jump_while_entry));
    }
    
    loop_context_stack.push(LoopContext{while_continue_ptr, while_end_ptr});

    current_bb = while_entry_ptr;
    visit_as_condition(*while_stmt.condition, while_body_ptr, while_end_ptr);

    current_bb = while_body_ptr;
    visit(*while_stmt.while_stmt);
    if (!current_bb->has_terminator()) {
        auto jump_continue = std::make_unique<Value>(Value::Jump{while_continue_ptr});
        current_bb->insts.push_back(std::move(jump_continue));
    }
    
    current_bb = while_continue_ptr;
    if (!current_bb->has_terminator()) {
        auto jump_entry = std::make_unique<Value>(Value::Jump{while_entry_ptr});
        current_bb->insts.push_back(std::move(jump_entry));
    }

    loop_context_stack.pop();
    
    current_function->blocks.push_back(std::move(while_entry));
    current_function->blocks.push_back(std::move(while_body));
    current_function->blocks.push_back(std::move(while_continue));
    current_function->blocks.push_back(std::move(while_end));
    current_bb = while_end_ptr;
}

void IRGenerator::visit(const BreakStmtAST& break_stmt) {
    if (loop_context_stack.empty()) {
        std::cerr << "Semantic Error: 'break' statement not in loop" << std::endl;
        exit(1);
    }
    if (current_bb->has_terminator()) return;

    const LoopContext& current_loop = loop_context_stack.top();
    auto jump_end = std::make_unique<Value>(Value::Jump{current_loop.end_bb});
    current_bb->insts.push_back(std::move(jump_end));
}

void IRGenerator::visit(const ContinueStmtAST& continue_stmt) {
    if (loop_context_stack.empty()) {
        std::cerr << "Semantic Error: 'continue' statement not in loop" << std::endl;
        exit(1);
    }
    if (current_bb->has_terminator()) return;

    const LoopContext& current_loop = loop_context_stack.top();
    auto jump_entry = std::make_unique<Value>(Value::Jump{current_loop.entry_bb});
    current_bb->insts.push_back(std::move(jump_entry));
}

// UnaryExp ::= PrimaryExp | UnaryOp UnaryExp;
std::unique_ptr<Value> IRGenerator::visit(const ExprAST& expr) {
    if (auto number = dynamic_cast<const NumberAST*>(&expr)) {
        auto val = std::make_unique<Value>(
            Value::Integer{ std::move(number->val) }
        );
        val->name = new_temp_var_name();
        val->type = std::make_unique<Type>();
        val->type->kind = Type::INTEGER;

        return val;
    }
    
    if (auto binary = dynamic_cast<const BinaryExprAST*>(&expr)) {
        auto lhs_val = visit(*binary->lhs);
        auto rhs_val = visit(*binary->rhs);

        if (binary->op == T_LAND) {
            auto lhs_bool = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::NE,
                    std::move(lhs_val),
                    std::make_unique<Value>(Value::Integer{0})
                }
            );
            lhs_bool->name = new_temp_var_name();
            lhs_bool->type = std::make_unique<Type>();
            lhs_bool->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(lhs_bool));
            auto lhs_ref = current_bb->insts.back().get();

            auto rhs_bool = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::NE,
                    std::move(rhs_val),
                    std::make_unique<Value>(Value::Integer{0})
                }
            );
            rhs_bool->name = new_temp_var_name();
            rhs_bool->type = std::make_unique<Type>();
            rhs_bool->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(rhs_bool));
            auto rhs_ref = current_bb->insts.back().get();

            auto result = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::AND,
                    std::make_unique<Value>(Value::SymbolRef{lhs_ref}),
                    std::make_unique<Value>(Value::SymbolRef{rhs_ref})
                }
            );
            result->name = new_temp_var_name();
            result->type = std::make_unique<Type>();
            result->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(result));
            return std::make_unique<Value>(Value::SymbolRef{current_bb->insts.back().get()});
        }

        if (binary->op == T_LOR) {
            auto lhs_bool = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::NE,
                    std::move(lhs_val),
                    std::make_unique<Value>(Value::Integer{0})
                }
            );
            lhs_bool->name = new_temp_var_name();
            lhs_bool->type = std::make_unique<Type>();
            lhs_bool->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(lhs_bool));
            auto lhs_ref = current_bb->insts.back().get();

            auto rhs_bool = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::NE,
                    std::move(rhs_val),
                    std::make_unique<Value>(Value::Integer{0})
                }
            );
            rhs_bool->name = new_temp_var_name();
            rhs_bool->type = std::make_unique<Type>();
            rhs_bool->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(rhs_bool));
            auto rhs_ref = current_bb->insts.back().get();

            auto result = std::make_unique<Value>(
                Value::Binary {
                    Value::Binary::OR,
                    std::make_unique<Value>(Value::SymbolRef{lhs_ref}),
                    std::make_unique<Value>(Value::SymbolRef{rhs_ref})
                }
            );
            result->name = new_temp_var_name();
            result->type = std::make_unique<Type>();
            result->type->kind = Type::INTEGER;
            current_bb->insts.push_back(std::move(result));
            return std::make_unique<Value>(Value::SymbolRef{current_bb->insts.back().get()});
        }

        Value::Binary::Op op;
        switch (binary->op) {
            case '+': op = Value::Binary::ADD; break;
            case '-': op = Value::Binary::SUB; break;
            case '*': op = Value::Binary::MUL; break;
            case '/': op = Value::Binary::DIV; break;
            case '%': op = Value::Binary::MOD; break;
            case '>': op = Value::Binary::GT; break;
            case '<': op = Value::Binary::LT; break;
            case '&': op = Value::Binary::AND; break;
            case '|': op = Value::Binary::OR; break;
            case T_LE: op = Value::Binary::LE; break;
            case T_GE: op = Value::Binary::GE; break;
            case T_EQ: op = Value::Binary::EQ; break;
            case T_NE: op = Value::Binary::NE; break;
            
            default:
                assert(false && "Unsupported binary operator");
        }
        auto bin_inst = std::make_unique<Value>(
            Value::Binary {
                op,
                std::move(lhs_val),
                std::move(rhs_val)
            }
        );
        bin_inst->name = new_temp_var_name();
        bin_inst->type = std::make_unique<Type>();
        bin_inst->type->kind = Type::INTEGER;

        Value* result_ptr = bin_inst.get();
        current_bb->insts.push_back(std::move(bin_inst));

        return std::make_unique<Value>(Value::SymbolRef{result_ptr});
    }

    if (auto unary = dynamic_cast<const UnaryExprAST*>(&expr)) {
        auto operand = visit(*unary->operand);

        switch (unary->op) {
            case '+':
                return operand;
            case '-': {
                auto zero = std::make_unique<Value>(Value::Integer{0});

                auto sub_inst = std::make_unique<Value>(
                    Value::Binary {
                        Value::Binary::SUB,
                        std::move(zero),
                        std::move(operand)
                    }
                );
                sub_inst->name = new_temp_var_name();
                sub_inst->type = std::make_unique<Type>();
                sub_inst->type->kind = Type::INTEGER;

                Value* result_ptr = sub_inst.get();
                current_bb->insts.push_back(std::move(sub_inst));

                return std::make_unique<Value>(Value::SymbolRef{result_ptr});
            }
            case '!': {
                auto zero = std::make_unique<Value>(Value::Integer{0});

                auto eq_inst = std::make_unique<Value>(
                    Value::Binary {
                        Value::Binary::EQ,
                        std::move(operand),
                        std::move(zero)
                    }
                );
                eq_inst->name = new_temp_var_name();
                eq_inst->type = std::make_unique<Type>();
                eq_inst->type->kind = Type::INTEGER;

                Value* result_ptr = eq_inst.get();
                current_bb->insts.push_back(std::move(eq_inst));

                return std::make_unique<Value>(Value::SymbolRef{result_ptr});
            }
                
            
        }
    }

    if (auto lval = dynamic_cast<const LValAST*>(&expr)) {
        const SymbolInfo* info = find_symbol(lval->ident);
        assert(info && "Undefined identifier");

        if (std::holds_alternative<int>(info->kind)) {
            // constant
            int const_val = std::get<int>(info->kind);
            return std::make_unique<Value>(Value::Integer{const_val});
        } else {
            // variable
            const Value* var_ptr = std::get<const Value*>(info->kind);
            assert(var_ptr->type && var_ptr->type->kind == Type::POINTER && "Variable symbol must be an address");
            auto load_inst = std::make_unique<Value>(Value::Load{var_ptr});
            load_inst->name = new_temp_var_name();
            load_inst->type = std::make_unique<Type>();
            load_inst->type->kind = Type::INTEGER;

            const Value* result_ptr = load_inst.get();
            current_bb->insts.push_back(std::move(load_inst));
            
            return std::make_unique<Value>(Value::SymbolRef{result_ptr});
        }
    }

    if (auto call = dynamic_cast<const FuncCallAST*>(&expr)) {
        const SymbolInfo* info = find_symbol(call->ident);
        assert(info && "Undefined function call");
        assert(std::holds_alternative<FunctionSymbolInfo>(info->kind) && "Calling non-function");
        const FunctionSymbolInfo& func_info = std::get<FunctionSymbolInfo>(info->kind);
        
        std::vector<std::unique_ptr<Value>> args;
        for (const auto& param_expr : call->params) {
            std::unique_ptr<Value> arg_val = visit(*param_expr);
            args.push_back(std::move(arg_val));
        }

        Type::Kind call_ret_kind =
            (func_info.ret_type == FuncTypeAST::TYPE_INT) ? Type::INTEGER : Type::VOID;
        auto call_inst = std::make_unique<Value>(
            Value::Call{"@" + call->ident, std::move(args), std::make_unique<Type>(Type{call_ret_kind})}
        );
        if (func_info.ret_type == FuncTypeAST::TYPE_INT) {
            call_inst->name = new_temp_var_name();
            const Value* result_ptr = call_inst.get();
            current_bb->insts.push_back(std::move(call_inst));
            return std::make_unique<Value>(Value::SymbolRef{result_ptr});
        } else {
            current_bb->insts.push_back(std::move(call_inst));
            return nullptr;
        }
    }
    
    assert(false && "Unknown expression type");
    return nullptr;
}

void IRGenerator::visit_as_condition(const ExprAST& cond, BasicBlock* true_bb, BasicBlock* false_bb) {
    if (auto binary = dynamic_cast<const BinaryExprAST*>(&cond); binary && binary->op == T_LAND) {
        auto rhs_bb = std::make_unique<BasicBlock>();
        rhs_bb->name = new_cond_branch_name("land_rhs");
        BasicBlock* rhs_bb_ptr = rhs_bb.get();
        current_function->blocks.push_back(std::move(rhs_bb));

        visit_as_condition(*binary->lhs, rhs_bb_ptr, false_bb);

        current_bb = rhs_bb_ptr;
        visit_as_condition(*binary->rhs, true_bb, false_bb);
        return;
    }
    
    if (auto binary = dynamic_cast<const BinaryExprAST*>(&cond); binary && binary->op == T_LOR) {
        auto rhs_bb = std::make_unique<BasicBlock>();
        rhs_bb->name = new_cond_branch_name("lor_rhs");
        BasicBlock* rhs_bb_ptr = rhs_bb.get();
        current_function->blocks.push_back(std::move(rhs_bb));

        visit_as_condition(*binary->lhs, true_bb, rhs_bb_ptr);

        current_bb = rhs_bb_ptr;
        visit_as_condition(*binary->rhs, true_bb, false_bb);
        return;
    }

    // other situation
    auto cond_val = visit(cond);
    auto br_inst = std::make_unique<Value>(
        Value::Branch{std::move(cond_val), true_bb, false_bb}
    );
    current_bb->insts.push_back(std::move(br_inst));
}

void IRGenerator::enter_scope() {
    symbol_tables.emplace_back();
}

void IRGenerator::exit_scope() {
    symbol_tables.pop_back();
}

void IRGenerator::register_builtin_functions() {
    struct BuiltinSpec {
        const char* name;
        FuncTypeAST::Type ret_type;
        std::vector<Type::Kind> param_types;
    };

    const std::vector<BuiltinSpec> builtins = {
        {"getint", FuncTypeAST::TYPE_INT, {}},
        {"getch", FuncTypeAST::TYPE_INT, {}},
        {"getarray", FuncTypeAST::TYPE_INT, {Type::POINTER}},
        {"putint", FuncTypeAST::TYPE_VOID, {Type::INTEGER}},
        {"putch", FuncTypeAST::TYPE_VOID, {Type::INTEGER}},
        {"putarray", FuncTypeAST::TYPE_VOID, {Type::INTEGER, Type::POINTER}},
        {"starttime", FuncTypeAST::TYPE_VOID, {}},
        {"stoptime", FuncTypeAST::TYPE_VOID, {}},
    };

    for (const auto& spec : builtins) {
        FunctionSymbolInfo info;
        info.ret_type = spec.ret_type;
        for (size_t i = 0; i < spec.param_types.size(); ++i) {
            info.params.push_back(nullptr);
        }
        symbol_tables[0][spec.name] = SymbolInfo{info};

        FuncDecl decl;
        decl.name = "@" + std::string(spec.name);
        decl.param_types = spec.param_types;
        decl.ret_type = (spec.ret_type == FuncTypeAST::TYPE_INT) ? Type::INTEGER : Type::VOID;
        program->decls.push_back(std::move(decl));
    }
}