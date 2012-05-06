// EventHandling_Managed_Event_2.cpp

  // compile with: /clr:oldSyntax /c
  using namespace System;
  [attribute(All, AllowMultiple=true)]
  public __gc class Attr {};
  
  public __delegate void D();
  
  public __gc class X {
  public:
     [method:Attr] __event D* E;
     [returnvalue:Attr] __event void noE();
  };
 