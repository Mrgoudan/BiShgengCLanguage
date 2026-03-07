def rewrite_bsc : Flag<["-"], "rewrite-bsc">, Flags<[CC1Option]>, Group<Action_Group>,
  HelpText<"Rewrite BSC source to C">;
def rewrite_bsc_line : Flag<["-"], "line">, Flags<[CC1Option]>,
  HelpText<"Insert line info when rewrite BSC">;
def dump_bscir : Flag<["-"], "dump-bscir">, Flags<[CC1Option]>,
  HelpText<"Dump BSCIR intermediate representation for debugging">,
  MarshallingInfoFlag<LangOpts<"DumpBSCIR">>;
def nullability_check : Joined<["-"], "nullability-check=">, Flags<[CC1Option]>,
  HelpText<"Select Nullability Check Enable Zone">,
  Values<"safeonly,all">, NormalizedValuesScope<"LangOptions">,
  NormalizedValues<["NC_SAFE", "NC_ALL"]>,
  MarshallingInfoEnum<LangOpts<"NullabilityCheck">, "NC_SAFE">;
def E_Group : OptionGroup<"<E group>">, Group<Diag_Group>, DocFlatten;
def E_value_Group : OptionGroup<"<E (with value) group>">, Group<E_Group>,
                    DocFlatten;
def E_Joined : Joined<["-"], "E">, Group<E_Group>, Flags<[CC1Option, CoreOption]>,
  MetaVarName<"<error>">, HelpText<"Enable the specified error">;