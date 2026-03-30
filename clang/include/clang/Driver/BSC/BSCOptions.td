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
def uninit_check : Joined<["-"], "uninit-check=">, Flags<[CC1Option]>,
  HelpText<"Select uninitialized variable check mode">,
  Values<"safeonly,none,all">, NormalizedValuesScope<"LangOptions">,
  NormalizedValues<["UC_SAFE", "UC_NONE", "UC_ALL"]>,
  MarshallingInfoEnum<LangOpts<"UninitCheck">, "UC_SAFE">;
def spatial_check: Joined<["-"], "spatial-check=">, Flags<[CC1Option]>,
  HelpText<"Select spatial check mode">,
  Values<"static,user">, NormalizedValuesScope<"LangOptions">,
  NormalizedValues<["SC_STATIC", "SC_USER"]>,
  MarshallingInfoEnum<LangOpts<"SpatialCheck">, "SC_STATIC">;
def E_Group : OptionGroup<"<E group>">, Group<Diag_Group>, DocFlatten;
def E_value_Group : OptionGroup<"<E (with value) group>">, Group<E_Group>,
                    DocFlatten;
def E_Joined : Joined<["-"], "E">, Group<E_Group>, Flags<[CC1Option, CoreOption]>,
  MetaVarName<"<error>">, HelpText<"Enable the specified error">;