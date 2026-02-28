struct A {
    _Owned int a; // expected-error {{type of 'int' cannot be qualified by '_Owned'}}
    _Owned int* b; // expected-error {{type of 'int' cannot be qualified by '_Owned'}}
};
