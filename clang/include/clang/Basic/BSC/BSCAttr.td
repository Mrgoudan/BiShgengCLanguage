def AsyncFunction : SubsetSubject<Function,
                             [{S->isAsyncSpecified()}], "async functions">;

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

