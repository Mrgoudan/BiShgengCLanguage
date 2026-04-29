def err_unsafe_action : Error<"%0 is forbidden in the safe zone">;
def err_safe_zone_decl : Error<"%select{|'_Safe'|'_Unsafe'}0 can only appear before on function or statement or parenthesized expression"> ;
def err_union_member_access_in_safe_zone : Error<"access to union field is _Unsafe and requires _Unsafe block">;
def err_safe_zone_case_in_nested_braces : Error<
  "'case' label inside a nested '{ }' in the safe zone">;
def note_safe_zone_case_in_nested_braces : Note<
  "remove the nested '{ }', or wrap the switch in '_Unsafe { ... }'">;