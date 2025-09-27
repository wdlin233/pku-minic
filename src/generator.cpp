#include "../include/generator.hpp"
#include "sysy.tab.hpp"
#include <algorithm>
#include <cassert>
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
}

void IRGenerator::visit(const ConstDeclAST& const_decl) {
    for (const auto& def: const_decl.const_defs) {
        int const_value = evaluate_const_expr(*def->init_val);
        symbol_table[def->ident] = std::make_unique<Value>(const_value);
    }
}

void IRGenerator::visit(const VarDeclAST& var_decl) {
    for (const auto& var_def : var_decl.var_defs) {
        if (var_def->init_val.has_value()) {
            auto init_val = visit(*var_def->init_val.value());
            symbol_table[var_def->ident] = std::move(init_val);
        } else {
            symbol_table[var_def->ident] = 0;
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
        if (it != symbol_table.end()) {
            return it->second->get_int_value();
        }
        assert(false && "Undefined constant identifier");
    }
    
    assert(false && "Unsupported expression type in constant expression");
    return 0;
}

void IRGenerator::visit(const StmtAST& stmt) {
    switch (stmt.type) {
        case StmtAST::ASSIGN: {
            auto rval = evaluate_const_expr(*stmt.expression);
            symbol_table[stmt.lval->ident] = std::make_unique<Value>(rval);
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
        int const_val = symbol_table.at(lval->ident)->get_int_value();

        return std::make_unique<Value>(Value::Integer{const_val});
    }
    
    assert(false && "Unknown expression type");
    return nullptr;
}
