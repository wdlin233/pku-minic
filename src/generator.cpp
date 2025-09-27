#include "../include/generator.hpp"
#include "sysy.tab.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
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

    for (const auto& item: func_def.block->items) {
        visit(*item);
    }
    
    func->blocks.push_back(std::move(bb)); // push basic block into basic blocks
    program->functions.push_back(std::move(func));
}

void IRGenerator::visit(const BlockItemAST& item) {
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
        int const_value = evaluate_const_expr(*def->init_val);
        
        SymbolInfo info;
        info.kind = const_value;
        symbol_table[def->ident] = info;
    }
}

void IRGenerator::visit(const VarDeclAST& var_decl) {
    for (const auto& var_def : var_decl.var_defs) {
        auto alloc_inst = std::make_unique<Value>(Value::Alloc{});
        alloc_inst->name = "@" + var_def->ident;  // @x = alloc i32
        alloc_inst->type = std::make_unique<Type>();
        alloc_inst->type->kind = Type::INTEGER;

        const Value* var_ptr = alloc_inst.get();
        current_bb->insts.push_back(std::move(alloc_inst));

        SymbolInfo info;
        info.kind = var_ptr;
        symbol_table[var_def->ident] = info;

        if (var_def->init_val.has_value()) {
            auto init_val = visit(*var_def->init_val.value());
            auto store_inst = std::make_unique<Value>(
                Value::Store{std::move(init_val), var_ptr}
            );
            current_bb->insts.push_back(std::move(store_inst));
        }
    }
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
        auto it = symbol_table.find(lval->ident);
        if (it == symbol_table.end()) {
            std::cerr << "Semantic Error: Use of undefined identifier '" << lval->ident << "' in a constant expression." << std::endl;
            exit(1);
        }
        
        const SymbolInfo& info = it->second;
        if (std::holds_alternative<int>(info.kind)) {
            return std::get<int>(info.kind);
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
    switch (stmt.type) {
        case StmtAST::ASSIGN: {
            auto it = symbol_table.find(stmt.lval->ident);
            if (it == symbol_table.end()) {
                std::cerr << "Semantic Error: Undefined identifier '" << stmt.lval->ident << "'" << std::endl;
                exit(1);
            }

            const SymbolInfo& info = it->second;
            if (std::holds_alternative<int>(info.kind)) {
                // 在处理赋值语句时, 赋值语句左侧的 LVal 对应一个常量, 而不是变量.
                std::cerr << "Semantic Error: Cannot assign to constant '" << stmt.lval->ident << "'" << std::endl;
                exit(1);
            } else {
                // variable
                const Value* var_ptr = std::get<const Value*>(info.kind);
                auto rval = visit(*stmt.expression);
                auto store_inst = std::make_unique<Value>(
                    Value::Store{std::move(rval), var_ptr}
                );
                current_bb->insts.push_back(std::move(store_inst));
            }
            break;

        }
        case StmtAST::RETURN: {
            auto ret_val = visit(*stmt.expression);

            auto ret_inst = std::make_unique<Value>(
                Value::Return{ std::move(ret_val) }
            );
            ret_inst->type = std::make_unique<Type>();
            ret_inst->type->kind = Type::INTEGER;

            current_bb->insts.push_back(std::move(ret_inst));
            break;
        }
    }
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
        assert(symbol_table.count(lval->ident) && "Undefined constant variable");
        const SymbolInfo& info = symbol_table.at(lval->ident);

        if (std::holds_alternative<int>(info.kind)) {
            // constant
            int const_val = std::get<int>(info.kind);
            return std::make_unique<Value>(Value::Integer{const_val});
        } else {
            // variable
            const Value* var_ptr = std::get<const Value*>(info.kind);
            auto load_inst = std::make_unique<Value>(Value::Load{var_ptr});
            load_inst->name = new_temp_var_name();
            load_inst->type = std::make_unique<Type>();
            load_inst->type->kind = Type::INTEGER;

            const Value* result_ptr = load_inst.get();
            current_bb->insts.push_back(std::move(load_inst));
            
            return std::make_unique<Value>(Value::SymbolRef{result_ptr});
        }

    }
    
    assert(false && "Unknown expression type");
    return nullptr;
}
