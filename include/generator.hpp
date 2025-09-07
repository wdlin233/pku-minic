#include "ast.hpp"
#include "ir.hpp"
#include <memory>
#include <string>

class IRGenerator {
    public:
        std::unique_ptr<Program> Generate(const CompUnitAST& ast);
    private:
        void visit(const FuncDefAST& func_def);
        void visit(const StmtAST& stmt);

        std::unique_ptr<Value> visit(const ExprAST& number);

        std::unique_ptr<Program> program;
        Function* current_function = nullptr;
        BasicBlock* current_bb = nullptr;
        int temp_var_counter = 0;

        std::string new_temp_var_name() {
            return "%" + std::to_string(temp_var_counter++);
        }
};