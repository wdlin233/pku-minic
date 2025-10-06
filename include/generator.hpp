#pragma once

#include "ast.hpp"
#include "ir.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <stack>

struct SymbolInfo {
    // for constant storing its int value
    // for variable storing raw pointer
    std::variant<int, const Value*> kind;
};

using SymbolTable = std::unordered_map<std::string, SymbolInfo>;

class IRGenerator {
    public:
        std::unique_ptr<Program> Generate(const CompUnitAST& ast);
    private:
        void visit(const FuncDefAST& func_def);
        void visit(const BlockItemAST& item);
        void visit(const StmtAST& stmt);
        void visit(const AssignStmtAST& assign_stmt);
        void visit(const ReturnStmtAST& return_stmt);
        void visit(const BlockStmtAST& block_stmt);
        void visit(const ExprStmtAST& expr_stmt);
        void visit(const IfStmtAST& if_stmt);
        void visit(const WhileStmtAST& while_stmt);
        void visit(const BreakStmtAST& break_stmt);
        void visit(const ContinueStmtAST& continue_stmt);
        void visit(const DeclAST& decl);
        void visit(const ConstDeclAST& const_decl);
        void visit(const VarDeclAST& var_decl);

        std::unique_ptr<Value> visit(const ExprAST& number);
        void visit_as_condition(const ExprAST& cond, BasicBlock* true_bb, BasicBlock* false_bb);

        Function* current_function = nullptr;
        std::unique_ptr<Program> program;
        BasicBlock* current_bb = nullptr;
        int temp_var_counter = 0;
        int branch_counter = -1;
        int condition_counter = -1;
        int while_counter = -1;

        // Stack of Symbol Tables
        std::vector<SymbolTable> symbol_tables;
        std::unordered_map<std::string, int> name_counters;
        const SymbolInfo* find_symbol(const std::string sym);

        // Context of Loop, for nested loop structure
        struct LoopContext {
            BasicBlock* entry_bb; // target of continue
            BasicBlock* end_bb; // target of break
        }; 
        std::stack<LoopContext> loop_context_stack;

        std::string new_temp_var_name() {
            return "%" + std::to_string(temp_var_counter++);
        }
        std::string new_branch_name(const std::string& mode) {
            if (mode == "then") branch_counter++;
            return "%" + mode + '_' + std::to_string(branch_counter);
        }
        std::string new_cond_branch_name(const std::string& mode) {
            if (mode == "land_rhs" || mode == "lor_rhs") condition_counter++;
            return "%" + mode + '_' + std::to_string(condition_counter);
        }
        std::string new_while_name(const std::string& mode) {
            if (mode == "while_entry") while_counter++;
            return "%" + mode + '_' + std::to_string(while_counter);
        }
        int evaluate_const_expr(const ExprAST& expr);
        void enter_scope();
        void exit_scope();
};