def err_unsafe_action : Error<"%0 is forbidden in the safe zone">;
def err_safe_zone_decl : Error<"%select{|'safe'|'unsafe'}0 can only appear before on function or statement or parenthesized expression"> ;
def err_union_member_access_in_safe_zone : Error<"access to union field is unsafe and requires unsafe block">;