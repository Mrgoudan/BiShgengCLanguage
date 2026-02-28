struct A {
    _Owned int a;  // expected-error {{unknown type name '_Owned'}}
    _Owned int* b; // expected-error {{unknown type name '_Owned'}}
};
