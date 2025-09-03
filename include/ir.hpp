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

    using Kind = std::variant<
        Integer,
        Return
    >;
    Kind kind;

    Value(Integer integer) : kind(std::move(integer)) {}
    Value(Return ret) : kind(std::move(ret)) {}

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
