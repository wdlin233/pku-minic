#include "ast.hpp"
#include "ir.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class IRGenerator {
    public:
        std::unique_ptr<Program> Generate(const CompUnitAST& ast);
    private:
        void visit(const FuncDefAST& func_def);
        void visit(const BlockItemAST& item);
        void visit(const StmtAST& stmt);
        void visit(const DeclAST& decl);
        void visit(const ConstDeclAST& const_decl);
        void visit(const VarDeclAST& var_decl);

        std::unique_ptr<Value> visit(const ExprAST& number);

        Function* current_function = nullptr;
        std::unique_ptr<Program> program;
        BasicBlock* current_bb = nullptr;
        int temp_var_counter = 0;

        std::unordered_map<std::string, std::unique_ptr<Value>> symbol_table;

        std::string new_temp_var_name() {
            return "%" + std::to_string(temp_var_counter++);
        }
        int evaluate_const_expr(const ExprAST& expr);
};