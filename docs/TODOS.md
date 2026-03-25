# TODOS

- [x] 实现处理语法和语义错误（语义分析）
- [x] IR 的数据结构是如何定义的？为什么这么定义？
- [ ] 在 Koopa IR 数据结构的基础上将此结构转化为 raw program，使用 libkoopa 接口将其转换为其他形式的 Koopa IR 程序，即 SysY AST -> Raw Program -> libkoopa -> 内存/文本 Koopa IR
- [x] 闭包
- [x] 参数的指针
- [x] 删除 `ast.cpp`
- [x] `RISCVGenerator::visit(const Value& value, std::ostream& os)` 的逻辑，让其寄存器分配
- [ ] 整理 current_stask_offset 的相关逻辑
- [ ] 更优雅的 `void IRGenerator::visit(const StmtAST& stmt)`
- [ ] 将 `symbol_tables` 改为使用 Stack
- [ ] 改掉固定 t0 分配