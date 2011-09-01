// keyword__sealed.cpp
  // compile with: /clr:oldSyntax
  
  #using <mscorlib.dll>
  extern "C" int printf_s(const char*, ...);
  
  __gc struct I
  {
      __sealed virtual void f()
      { 
          printf_s("I::f()\n"); 
      }
      virtual void g()
      {
          printf_s("I::g()\n");
      }
  };
  
  __gc struct A : I 
  {
      void f() // C3248 sealed function
      { 
          printf_s("A::f()\n"); 
      }   
      void g()
      {
          printf_s("A::g()\n");
      }
  };
  
  int main()
  {
      A* pA = new A;
  
      pA->f();
      pA->g();
  }
