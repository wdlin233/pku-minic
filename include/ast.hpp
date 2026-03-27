#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>

class BaseAST {
    public:
        virtual ~BaseAST() = default;
};

class DeclAST;
class FuncDefAST;
class FuncTypeAST;
class BlockAST;
class StmtAST;
class NumberAST;
class LValAST;
class ConstDefAST;
class VarDefAST;

class CompUnitAST : public BaseAST {
    public:
    std::vector<std::unique_ptr<DeclAST>> decls;
    std::vector<std::unique_ptr<FuncDefAST>> func_defs;
};

class FuncFParamAST : public BaseAST {
    public:
        enum Type {
            TYPE_INT
        }; 
        Type type;
        std::string ident;
};

class FuncDefAST : public BaseAST {
    public:
        std::unique_ptr<FuncTypeAST> func_type;
        std::string ident;
        std::vector<std::unique_ptr<FuncFParamAST>> params;
        std::unique_ptr<BlockAST> block;
};

class FuncTypeAST : public BaseAST {
    public:
        enum Type {
            TYPE_INT,
            TYPE_VOID
        };
        Type type;
        explicit FuncTypeAST(Type t) : type(t) {};
};

class ExprAST : public BaseAST {};

class NumberAST : public ExprAST {
    public:
        int val;
        explicit NumberAST(int val) : val(val) {};
};

class BinaryExprAST : public ExprAST {
    public:
        int op;     // avoid implicit conversion from int(%token symbol) to char
        std::unique_ptr<ExprAST> lhs;
        std::unique_ptr<ExprAST> rhs;
};

// UnaryOp and UnaryExpr
class UnaryExprAST : public ExprAST {
    public:
        char op;
        std::unique_ptr<ExprAST> operand;
};

class FuncCallAST : public ExprAST {
    public:
        std::string ident; // function name
        std::vector<std::unique_ptr<ExprAST>> params; 
};

class BlockItemAST : public BaseAST {};

class BlockAST : public BaseAST {
    public:
        std::vector<std::unique_ptr<BlockItemAST>> items;
};

class StmtAST : public BlockItemAST {};

class AssignStmtAST : public StmtAST {
    public:
        std::unique_ptr<LValAST> lval;
        std::unique_ptr<ExprAST> expression;
};

class ReturnStmtAST : public StmtAST {
    public:
        std::optional<std::unique_ptr<ExprAST>> expression; // [Exp]
};

class BlockStmtAST : public StmtAST {
    public:
        std::unique_ptr<BlockAST> block;
};

class ExprStmtAST : public StmtAST {
    public:
        std::optional<std::unique_ptr<ExprAST>> expression; // [Exp]
};

class IfStmtAST : public StmtAST {
    public:
        std::unique_ptr<ExprAST> condition;
        std::unique_ptr<StmtAST> then_stmt;
        std::optional<std::unique_ptr<StmtAST>> else_stmt; 
};

class WhileStmtAST : public StmtAST {
    public:
        std::unique_ptr<ExprAST> condition;
        std::unique_ptr<StmtAST> while_stmt;
};

class BreakStmtAST : public StmtAST {};
class ContinueStmtAST : public StmtAST {};

class DeclAST : public BlockItemAST {};

// ConstDecl ::= "const" BType ConstDef {"," ConstDef} ";";
class ConstDeclAST : public DeclAST {
    public:
        // BType btype; int only now ignoring
        std::vector<std::unique_ptr<ConstDefAST>> const_defs;
};

// ConstDef ::= IDENT "=" ConstInitVal;
class ConstDefAST : public BaseAST {
    public:
        std::string ident;
        std::unique_ptr<ExprAST> init_val;    
};

// VarDecl ::= BType VarDef {"," VarDef} ";";
class VarDeclAST : public DeclAST {
    public:
        std::vector<std::unique_ptr<VarDefAST>> var_defs;
};

// VarDef ::= IDENT | IDENT "=" InitVal;
class VarDefAST : public BaseAST {
    public:
        std::string ident;
        std::optional<std::unique_ptr<ExprAST>> init_val;
};

class LValAST : public ExprAST {
    public:
        std::string ident;
};
