def opt_string : Joined<["-"], "opt-string=">, Flags<[CC1Option]>,
  MarshallingInfoString<PreprocessorOpts<"OptString">>;
def rewrite_bsc : Flag<["-"], "rewrite-bsc">, Flags<[CC1Option]>, Group<Action_Group>,
  HelpText<"Rewrite BSC source to C">;
def disable_ownership_check : Flag<["-"], "disable-ownership-check">, Flags<[CC1Option]>,
  HelpText<"Disable ownership check">, MarshallingInfoFlag<LangOpts<"DisableOwnershipCheck">>;
