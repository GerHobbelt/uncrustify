// EventHandling_Managed_Event.cpp

// compile with: /clr:oldSyntax /c
using namespace System;
[event_source(managed)]
public __gc class CPSource {
 public:
   __event void MyEvent(Int16 nValue);
};

