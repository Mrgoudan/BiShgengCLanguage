# B - 语法

本章对毕昇 C 语言的语法使用 EBNF（Extended Backus-Naur Form，扩展巴科斯范式）进行规范化描述。由于毕昇 C 语言扩展自 C 语言，本节不再列出 C 语言已有的语法的 EBNF 表示，**仅给出毕昇 C 语言扩展的语法的 EBNF 表示**，关于 C 语言已有语法的 EBNF 表示可查阅[C11 标准规范](https://www.iso-9899.info/n1570.html#A.)。本章的组织结构与 C11 标准规范的附录 A 保持一致，用户可将两者对比阅读，能够清晰直观地了解到毕昇 C 扩展的 EBNF 表示的内容。

在本章中使用的语法符号中，采用与 C11 标准规范中类似的表示方式：

- 使用前下划线（_）加小写字母表示语法类别（即非终结符），前下划线加大写字母或没有前下划线则表示字面量和字符集合（即终结符）；
- 在代码块的第一行，非终结符后接冒号（:）表示非终结符的具体定义（即产生式规则）；
- 除了以“one of”开头外，其他的可选定义都列在不同的行中，可选的非终结符使用后缀“_opt”表示；
- 对于向 C11 已有的语法类别中新增语法，使用“....”表示省略已有的语法规则。

## Lexical grammar

### Lexical elements

无新增或修改。

### Keywords

`_keyword`的产生式规则有如下变化：

1. 新增 18 个关键字，为`_keyword`新增 18 条产生式：

```text
_keyword : one of
    ....         _Impl         This
    _Async       nullptr       this
    _Await       _Owned        _Trait
    _Borrow      _Private      _Unsafe
    constexpr    _Public       _Nonnull
    _Safe        _Nullable     _Const
    _Mut
```

### Identifiers

无新增或修改。

### Universal character names

无新增或修改。

### Constants

无新增或修改。

### String literals

无新增或修改。

### Punctuators

`_punctuator`的产生式规则有如下变化：

1. 新增 5 个运算符和分隔符，为`_punctuator`新增 5 条产生式。

```text
_punctuator : one of
    ....          ::
    &_Const       &_Mut
    &_Const *     &_Mut *
```

### Header names

无新增或修改。

### Preprocessing numbers

无新增或修改。

### Pointer literals

新增`_pointer-literal`，表示空指针字面量：

```text
_pointer-literal :
    nullptr
```

## Phrase structure grammar

### Expressions

`_primary-expression`的产生式规则有如下变化：

1. 支持泛型特性，修改了`_primary-expression`的第 1 条产生式；
2. `this`属于`_primary-expression`，为`_primary-expression`新增 1 条产生式。

```text
_primary-expression :
    ....
    _identifier _template-declaration_opt
    this
```

`_postfix-expression`的产生式规则有如下变化：

1. 支持泛型函数，修改了`_postfix-expression`的第 4 和第 5 条产生式；
2. 允许调用静态成员函数和实例成员函数，为`_postfix-expression`新增 2 条产生式。

```text
_postfix-expression :
    ....
    _postfix-expression . _identifier _template-declaration_opt
    _postfix-expression -> _identifier _template-declaration_opt
    _nested-name-specifier
    _postfix-expression _identifier _template-declaration_opt
```

`_unary-expression`的产生式规则有如下变化：

1. 允许使用`_Await`关键字修饰表达式，为`_unary-expression`新增 1 条产生式。

```text
_unary-expression :
    ....
    _Await _unary-expression
```

`_unary-operator`的产生式规则有如下变化：

1. 新增 4 种一元运算符，为`_unary-operator`新增 4 条产生式。

```text
_unary-operator : one of
    ....         &_Const *     &_Mut
    &_Const       &_Mut *
```

### Declarations

新增`_template-parameter`，表示泛型形参：

```text
_template-parameter :
    _identifier
    _parameter-declaration
```

新增`_template-parameter-list`，表示泛型形参列表：

```text
_template-parameter-list :
    _template-parameter
    _template-parameter-list , _template-parameter
```

新增`_template-argument`，表示泛型实参：

```text
_template-argument :
    _constant-expression
    _type-name
```

新增`_template-argument-list`，表示泛型实参列表：

```text
_template-argument-list :
    _template-argument
    _template-argument-list , _template-argument
```

新增`_template-declaration`，表示泛型形参声明或泛型实参声明：

```text
_template-declaration :
    < _template-parameter-list >
    < _template-argument-list >
```

新增`_constexpr-specifier`，表示常量表达式限定符：

```text
_constexpr-specifier :
    constexpr
```

`_declaration-specifiers`的产生式规则有如下变化：

1. `_constexpr-specifier`可修饰声明，为`_declaration-specifiers`新增 1 条产生式。

```text
_declaration-specifiers :
    ....
    _constexpr-specifier _declaration-specifiers_opt
```

`_initializer`的产生式规则有如下变化：

1. 支持通过`typedef`和等号定义类型别名的新语法，为`_initializer`新增 1 条产生式。

```text
_initializer :
    ....
    _type-name
```

`_type-specifier`的产生式规则有如下变化：

1. 新增 2 种类型限定符，为`_type-specifier`新增 2 条产生式。

```text
_type-specifier :
    ....
    This
    _trait-name
```

`_struct-or-union-specifier`的产生式规则有如下变化：

1. 允许定义泛型结构体和联合体，修改了`_struct-or-union-specifier`的第 1 和第 2 条产生式。

```text
_struct-or-union-specifier :
    _struct-or-union _identifier_opt _template-declaration_opt { _struct-declaration-list }
    _struct-or-union _identifier _template-declaration_opt
```

`_type-qualifier`的产生式规则有如下变化：

1. 新增 5 种类型说明符，为`_type-qualifier`新增 5 条产生式。

```text
_type-qualifier :
    ....
    _Borrow
    _Owned
    _ArrayElem
    _Nonnull
    _Nullable
```

新增`_safe-specifier`，表示安全限定符：

```text
_safe-specifier :
    _Safe
    _Unsafe
```

新增`_access-specifier`，表示访问限定符：

```text
_access-specifier :
    _Public
    _Private
```

`_function-specifier`的产生式规则有如下变化：

1. 新增 3 种函数限定符，为`_function-specifier`新增 2 条产生式。

```text
_function-specifier :
    ....
    _Async
    _safe-specifier
```

新增`_nested-name-specifier`，表示嵌套限定符：

```text
_nested-name-specifier :
    _type-name ::
```

`_direct-declarator`的产生式规则有如下变化：

1. 支持泛型函数定义，修改了`_direct-declarator`的第 1 条产生式；
2. 支持为类型声明和定义扩展成员函数，为`_direct-declarator`新增 2 条产生式。

```text
_direct-declarator :
    ....
    _identifier _template-declaration_opt
    _nested-name-specifier
    _direct-declarator _identifier _template-declaration_opt ( _parameter-type-list )
```

`_typedef-name`的产生式规则有如下变化：

1. 支持对泛型类型定义别名，修改了`_typedef-name`的第 1 条产生式。

```text
_typedef-name :
    _identifier _template-declaration_opt
```

### Statements

`_compound-statement`的产生式规则有如下变化：

1. 安全限定符可修饰代码块，修改了`_compound-statement`的第 1 条产生式。

```text
_compound-statement :
    _safe-specifier_opt { _block-item-list_opt }
```

`_selection-statement`的产生式规则有如下变化：

1. 常量表达式限定符可修饰`if`语句的条件表达式，修改了`_selection-statement`的第 1 和 第 2 条产生式。

```text
_selection-statement :
    if _constexpr-specifier_opt ( _expression ) _statement
    if _constexpr-specifier_opt ( _expression ) _statement else _statement
```

### External definitions

新增`_function-declaration`，表示函数声明：

```text
_function-declaration :
    _declaration-specifiers _pointer_opt _identifier ( _parameter-type-list ) ;
```

新增`_function-declaration-list`，表示函数声明列表：

```text
_function-declaration-list :
    _function-declaration
    _function-declaration-list _function-declaration
```

新增`_trait-name`的定义，表示`_Trait`的名称：

```text
_trait-name :
    _Trait _identifier _template-declaration_opt
```

新增`_trait-definition`，表示`_Trait`的定义：

```text
_trait-definition :
    _trait-name { _function-declaration-list } ;
```

新增`_impl-declaration`，表示实现`_Trait`：

```text
_impl-declaration :
    _Impl _trait-name for _type-name ;
```

新增`_dtor-definition`，表示析构函数定义：

```text
_dtor-definition :
    ~ _identifier ( _declaration ) _compound-statement
```

新增`_member-declaration`，表示`_Owned struct`成员种类：

```text
_member-declaration :
    _declaration
    _dtor-definition
    _function-definition
```

新增`_member-specification`，表示`_Owned struct`成员变量或成员函数：

```text
_member-specification :
    _member-declaration _member-specification_opt
    _access-specifier : _member-specification_opt
```

新增`_owned-struct-declaration`，表示`_Owned struct`类型声明：

```text
_owned-struct-declaration :
    _Owned struct _identifier _template-declaration_opt { _member-specification_opt } ;
```

`_external-declaration`的产生式规则有如下变化：

1. `_Trait`定义、实现`_Trait`和`_Owned struct`类型声明都属于外部声明，为`_external-declaration`新增 3 条产生式。

```text
_external-declaration :
    ....
    _trait-definition
    _impl-declaration
    _owned-struct-declaration
```

## Preprocessing directives

无新增或修改。
