# 遇到的问题/思考记录

### IR 的数据结构是如何定义的？为什么这么定义？

我的理解是 IR 就表示了整个程序经过词法和语法分析后得到的中间表示的结构，是 `Program`, `Function` 和 `BasicBlock` 三个层的嵌套，在最底层或者说最基础的表示都归于 `Value` 这个结构，里面就区分了 `Integer` 和 `Return` 这些我们在 `IRGenerator::vist(const NumberAST& number)` 中解析出来的类型.

那么一个正常的疑问就是，IR 的数据结构表示和 IRGenerator 之间的联系是什么？

例如下面这两个函数

```cpp
void IRGenerator::visit(const FuncDefAST& func_def) {
    auto func = std::make_unique<Function>();
    func->name = "@" + func_def.ident;
    func->ret_type = std::make_unique<Type>();
    func->ret_type->kind = Type::INTEGER;
    
    auto bb = std::make_unique<BasicBlock>();
    bb->name = "%entry";

    current_function = func.get();
    current_bb = bb.get();

    visit(*func_def.block->stmt);

    func->blocks.push_back(std::move(bb)); // push basic block into basic blocks
    program->functions.push_back(std::move(func));
}

std::unique_ptr<Value> IRGenerator::visit(const NumberAST& number) {
    auto val = std::make_unique<Value>(
        Value::Integer{ std::move(number.val) }
    );
    val->name = new_temp_var_name();
    val->type = std::make_unique<Type>();
    val->type->kind = Type::INTEGER;

    return val;
}
```

我们实际上是在一直递归 `vist` 遍历这个 AST 结构，在每个对应的结构处都为其建立对应的 IR 数据结构.

`func->blocks.push_back(std::move(bb))` 以支持后续打印相关的指令.

### 关于添加新的表达式解析

添加新的表达式就是实现新的语法分析，构建其他类型的 AST，然后在生成 IR 时做对应的处理.

语法分析在 `ast.h` 和 `sysy.y` 中. 构建 AST 实际上是在 `sysy.y` 中作语法分析，`ast.cpp` 还保留着原本的 `Dump()` 解析文本输出，感觉不是很优雅.

到构建 IR 不会太复杂，但是生成汇编时需要一个寄存器分配的方法，目前还是让其固定分配.

