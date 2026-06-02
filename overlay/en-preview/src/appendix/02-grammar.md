# B - Grammar

This chapter provides a formal description of the BiSheng C language grammar using EBNF (Extended Backus-Naur Form). Since BiSheng C is an extension of the C language, this section does not list the EBNF representation of grammar that already exists in C; **it gives only the EBNF representation of the grammar that BiSheng C extends**. For the EBNF representation of existing C grammar, refer to the [C11 standard specification](https://www.iso-9899.info/n1570.html#A.). This chapter is organized consistently with Annex A of the C11 standard specification, so that users can read the two side by side and gain a clear, intuitive understanding of the content that the BiSheng C extension EBNF representation covers.

The grammar symbols used in this chapter follow a notation similar to that of the C11 standard specification:

- A leading underscore (_) plus a lowercase letter denotes a grammar category (i.e. a nonterminal); a leading underscore plus an uppercase letter, or no leading underscore, denotes a literal or a character set (i.e. a terminal);
- On the first line of a code block, a nonterminal followed by a colon (:) denotes the specific definition of that nonterminal (i.e. the production rule);
- Except for those beginning with "one of", all alternative definitions are listed on separate lines, and an optional nonterminal is denoted by the suffix "_opt";
- For grammar added to an existing C11 grammar category, "...." is used to indicate the omission of the existing grammar rules.

## Lexical grammar

### Lexical elements

No additions or modifications.

### Keywords

The production rules of `_keyword` change as follows:

1. 18 keywords are added, adding 18 productions to `_keyword`:

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

No additions or modifications.

### Universal character names

No additions or modifications.

### Constants

No additions or modifications.

### String literals

No additions or modifications.

### Punctuators

The production rules of `_punctuator` change as follows:

1. 5 operators and separators are added, adding 5 productions to `_punctuator`.

```text
_punctuator : one of
    ....          ::
    &_Const       &_Mut
    &_Const *     &_Mut *
```

### Header names

No additions or modifications.

### Preprocessing numbers

No additions or modifications.

### Pointer literals

Adds `_pointer-literal`, denoting the null-pointer literal:

```text
_pointer-literal :
    nullptr
```

## Phrase structure grammar

### Expressions

The production rules of `_primary-expression` change as follows:

1. To support the generics feature, the 1st production of `_primary-expression` is modified;
2. `this` belongs to `_primary-expression`, adding 1 production to `_primary-expression`.

```text
_primary-expression :
    ....
    _identifier _template-declaration_opt
    this
```

The production rules of `_postfix-expression` change as follows:

1. To support generic functions, the 4th and 5th productions of `_postfix-expression` are modified;
2. To allow calling static member functions and instance member functions, 2 productions are added to `_postfix-expression`.

```text
_postfix-expression :
    ....
    _postfix-expression . _identifier _template-declaration_opt
    _postfix-expression -> _identifier _template-declaration_opt
    _nested-name-specifier
    _postfix-expression _identifier _template-declaration_opt
```

The production rules of `_unary-expression` change as follows:

1. To allow using the `_Await` keyword to qualify an expression, 1 production is added to `_unary-expression`.

```text
_unary-expression :
    ....
    _Await _unary-expression
```

The production rules of `_unary-operator` change as follows:

1. 4 unary operators are added, adding 4 productions to `_unary-operator`.

```text
_unary-operator : one of
    ....         &_Const *     &_Mut
    &_Const       &_Mut *
```

### Declarations

Adds `_template-parameter`, denoting a generic parameter:

```text
_template-parameter :
    _identifier
    _parameter-declaration
```

Adds `_template-parameter-list`, denoting a generic parameter list:

```text
_template-parameter-list :
    _template-parameter
    _template-parameter-list , _template-parameter
```

Adds `_template-argument`, denoting a generic argument:

```text
_template-argument :
    _constant-expression
    _type-name
```

Adds `_template-argument-list`, denoting a generic argument list:

```text
_template-argument-list :
    _template-argument
    _template-argument-list , _template-argument
```

Adds `_template-declaration`, denoting a generic parameter declaration or a generic argument declaration:

```text
_template-declaration :
    < _template-parameter-list >
    < _template-argument-list >
```

Adds `_constexpr-specifier`, denoting the constant-expression specifier:

```text
_constexpr-specifier :
    constexpr
```

The production rules of `_declaration-specifiers` change as follows:

1. `_constexpr-specifier` can qualify a declaration, adding 1 production to `_declaration-specifiers`.

```text
_declaration-specifiers :
    ....
    _constexpr-specifier _declaration-specifiers_opt
```

The production rules of `_initializer` change as follows:

1. To support the new syntax for defining type aliases via `typedef` and an equals sign, 1 production is added to `_initializer`.

```text
_initializer :
    ....
    _type-name
```

The production rules of `_type-specifier` change as follows:

1. 2 type specifiers are added, adding 2 productions to `_type-specifier`.

```text
_type-specifier :
    ....
    This
    _trait-name
```

The production rules of `_struct-or-union-specifier` change as follows:

1. To allow defining generic structs and unions, the 1st and 2nd productions of `_struct-or-union-specifier` are modified.

```text
_struct-or-union-specifier :
    _struct-or-union _identifier_opt _template-declaration_opt { _struct-declaration-list }
    _struct-or-union _identifier _template-declaration_opt
```

The production rules of `_type-qualifier` change as follows:

1. 5 type qualifiers are added, adding 5 productions to `_type-qualifier`.

```text
_type-qualifier :
    ....
    _Borrow
    _Owned
    _ArrayElem
    _Nonnull
    _Nullable
```

Adds `_safe-specifier`, denoting the safety specifier:

```text
_safe-specifier :
    _Safe
    _Unsafe
```

Adds `_access-specifier`, denoting the access specifier:

```text
_access-specifier :
    _Public
    _Private
```

The production rules of `_function-specifier` change as follows:

1. 3 function specifiers are added, adding 2 productions to `_function-specifier`.

```text
_function-specifier :
    ....
    _Async
    _safe-specifier
```

Adds `_nested-name-specifier`, denoting the nested-name specifier:

```text
_nested-name-specifier :
    _type-name ::
```

The production rules of `_direct-declarator` change as follows:

1. To support generic function definitions, the 1st production of `_direct-declarator` is modified;
2. To support extending member functions for type declarations and definitions, 2 productions are added to `_direct-declarator`.

```text
_direct-declarator :
    ....
    _identifier _template-declaration_opt
    _nested-name-specifier
    _direct-declarator _identifier _template-declaration_opt ( _parameter-type-list )
```

The production rules of `_typedef-name` change as follows:

1. To support defining aliases for generic types, the 1st production of `_typedef-name` is modified.

```text
_typedef-name :
    _identifier _template-declaration_opt
```

### Statements

The production rules of `_compound-statement` change as follows:

1. A safety specifier can qualify a code block, modifying the 1st production of `_compound-statement`.

```text
_compound-statement :
    _safe-specifier_opt { _block-item-list_opt }
```

The production rules of `_selection-statement` change as follows:

1. The constant-expression specifier can qualify the condition expression of an `if` statement, modifying the 1st and 2nd productions of `_selection-statement`.

```text
_selection-statement :
    if _constexpr-specifier_opt ( _expression ) _statement
    if _constexpr-specifier_opt ( _expression ) _statement else _statement
```

### External definitions

Adds `_function-declaration`, denoting a function declaration:

```text
_function-declaration :
    _declaration-specifiers _pointer_opt _identifier ( _parameter-type-list ) ;
```

Adds `_function-declaration-list`, denoting a function declaration list:

```text
_function-declaration-list :
    _function-declaration
    _function-declaration-list _function-declaration
```

Adds the definition of `_trait-name`, denoting the name of a `_Trait`:

```text
_trait-name :
    _Trait _identifier _template-declaration_opt
```

Adds `_trait-definition`, denoting the definition of a `_Trait`:

```text
_trait-definition :
    _trait-name { _function-declaration-list } ;
```

Adds `_impl-declaration`, denoting the implementation of a `_Trait`:

```text
_impl-declaration :
    _Impl _trait-name for _type-name ;
```

Adds `_dtor-definition`, denoting a destructor definition:

```text
_dtor-definition :
    ~ _identifier ( _declaration ) _compound-statement
```

Adds `_member-declaration`, denoting the kinds of `_Owned struct` members:

```text
_member-declaration :
    _declaration
    _dtor-definition
    _function-definition
```

Adds `_member-specification`, denoting a member variable or member function of an `_Owned struct`:

```text
_member-specification :
    _member-declaration _member-specification_opt
    _access-specifier : _member-specification_opt
```

Adds `_owned-struct-declaration`, denoting an `_Owned struct` type declaration:

```text
_owned-struct-declaration :
    _Owned struct _identifier _template-declaration_opt { _member-specification_opt } ;
```

The production rules of `_external-declaration` change as follows:

1. A `_Trait` definition, a `_Trait` implementation, and an `_Owned struct` type declaration are all external declarations, adding 3 productions to `_external-declaration`.

```text
_external-declaration :
    ....
    _trait-definition
    _impl-declaration
    _owned-struct-declaration
```

## Preprocessing directives

No additions or modifications.
