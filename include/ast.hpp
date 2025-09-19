#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <vector>

class BaseAST {
    public:
        virtual ~BaseAST() = default;
};

class FuncDefAST;
class FuncTypeAST;
class BlockAST;
class StmtAST;
class NumberAST;
class LValAST;

class CompUnitAST : public BaseAST {
    public:
        std::unique_ptr<FuncDefAST> func_def;
};

class FuncDefAST : public BaseAST {
    public:
        std::unique_ptr<FuncTypeAST> func_type;
        std::string ident;
        std::unique_ptr<BlockAST> block;
};

class FuncTypeAST : public BaseAST {
    public:
        enum Type {
            TYPE_INT
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

class BlockItemAST : public BaseAST {};

class BlockAST : public BaseAST {
    public:
        std::vector<std::unique_ptr<BlockItemAST>> items;
};

class StmtAST : public BlockItemAST {
    public:
        // 如果无法判断类型，就用 enum 和一个对应值存储
        enum StmtType {
            ASSIGN,
            RETURN
        };

        StmtType type;
        std::unique_ptr<LValAST> lval;
        std::unique_ptr<ExprAST> expression;

        StmtAST(StmtType t) : type(t) {}
};

class DeclAST : public BlockItemAST {};

// ConstDef ::= IDENT "=" ConstInitVal;
class ConstDefAST : public BaseAST {
    public:
        std::string ident;
        std::unique_ptr<ExprAST> init_val;    
};

class ConstDeclAST : public DeclAST {
    public:
        // BType btype; int only now ignoring
        std::vector<std::unique_ptr<ConstDefAST>> const_defs;
};

class LValAST : public ExprAST {
    public:
        std::string ident;
};
