def AsyncFunction : SubsetSubject<Function,
                             [{S->isAsyncSpecified()}], "async functions">;

def FunctionLikeMacro : InheritableAttr {
  let Spellings = [GNU<"function_like_macro">];
  let Subjects = SubjectList<[Function]>;
  let Documentation = [Undocumented];
}

def Operator : InheritableAttr {
  let Spellings = [GNU<"operator">];
  let Subjects = SubjectList<[Function]>;
  let Args = [IntArgument<"OperatorKind">];
  let Documentation = [Undocumented];
}

def EnsureInit : InheritableParamAttr {
  let Spellings = [GNU<"ensure_init">];
  let Subjects = SubjectList<[ParmVar]>;
  let Documentation = [Undocumented];
}

