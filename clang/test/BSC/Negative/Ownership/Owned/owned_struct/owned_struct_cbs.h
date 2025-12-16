struct A {
    owned int a; // expected-error {{type of 'int' cannot be qualified by 'owned'}}
    owned int* b; // expected-error {{type of 'int' cannot be qualified by 'owned'}}
};
