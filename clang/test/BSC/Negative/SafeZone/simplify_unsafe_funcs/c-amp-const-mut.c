// RUN: %clang_cc1 -fsyntax-only -verify -x c %s

// Test that &_Const and &_Mut are NOT recognized in standard C mode.

void test_amp_const_mut() {
  int a = 1;


  const int *p1 = &_Const a; // expected-error {{use of undeclared identifier '_Const'}} \
                              // expected-error {{expected ';' at end of declaration}}
  int *p2 = &_Mut a; // expected-error {{use of undeclared identifier '_Mut'}} \
                     // expected-error {{expected ';' at end of declaration}}


  const int *p3 = & const a; // expected-error {{expected expression}}


  int *p4 = & mut a; // expected-error {{use of undeclared identifier 'mut'}} \
                     // expected-error {{expected ';' at end of declaration}}

  int mut = 0;
  int *p_mut = &mut; //no error - 'mut' is just an identifier in C mode
}
