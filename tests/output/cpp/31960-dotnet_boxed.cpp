// keyword__box2.cpp

// compile with: /clr:oldSyntax
#using <mscorlib.dll>
using namespace System;

__value struct V
{
   int i;
};
void Positive(Object *)
{
}                           // expects a managed class

int main()
{
   V v = { 10 };   // allocate and initialize
   Console::WriteLine(v.i);

   // copy to the common language runtime heap
   __box V *pBoxedV = __box(v);

   Positive(pBoxedV); // treat as a managed class

   pBoxedV->i = 20;   // update the boxed version
   Console::WriteLine(pBoxedV->i);
}

