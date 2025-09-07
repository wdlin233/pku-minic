#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <variant>

struct Value;
struct BasicBlock;
struct Function;
struct Program;
struct Binary;

struct Type {
    enum Kind {
        INTEGER,      
    };
    Kind kind;
};

struct Value {
    std::string name; // temp name, %name
    std::unique_ptr<Type> type;

    struct Integer { 
        int32_t value; 
        Integer(int32_t v) : value(v) {}
    };
    struct Return { 
        std::unique_ptr<Value> value; 
        Return(std::unique_ptr<Value> v) : value(std::move(v)) {}
    };

    struct Binary {
        enum Op {
            EQ, NE, GT, LT, GE, LE,
            ADD, SUB, MUL, DIV, MOD,
            AND, OR, XOR,
            SHL, SHR, SAR,
        };

        Op op;
        
        std::unique_ptr<Value> lhs;
        std::unique_ptr<Value> rhs;
    };

    using Kind = std::variant<
        Integer,
        Return,
        Binary
    >;
    Kind kind;

    Value(Integer integer) : kind(std::move(integer)) {}
    Value(Return ret) : kind(std::move(ret)) {}
    Value(Binary binary) : kind(std::move(binary)) {}

    void Dump(std::ostream& os) const;
};

struct BasicBlock {
    std::string name;
    std::vector<std::unique_ptr<Value>> insts;

    void Dump(std::ostream& os) const;
};

struct Function {
    std::string name;
    std::unique_ptr<Type> ret_type;
    std::vector<std::unique_ptr<BasicBlock>> blocks;

    void Dump(std::ostream& os) const;
};

struct Program {
    std::vector<std::unique_ptr<Function>> functions;

    void Dump(std::ostream& os) const;
};
