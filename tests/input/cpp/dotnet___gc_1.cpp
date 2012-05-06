// keyword__gc.cpp

  // compile with: /clr:oldSyntax
  #using <mscorlib.dll>
  using namespace System;
  
  __gc class X {
  public:
     int i;
     int ReturnInt() { return 5; }
  };
  
  int main() {
     // X is a __gc class, so px is a __gc pointer
     X* px;
     px = new X;   // creates a managed object of type X
     Console::WriteLine(px->i);
  
     px->i = 4;   // modifies X::i through px
     Console::WriteLine(px->i);
  
     int n = px->ReturnInt();   // calls X::ReturnInt through px
     Console::WriteLine(n);
  }
  
