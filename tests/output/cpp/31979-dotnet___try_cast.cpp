// keyword__try_cast.cpp
// compile with: /clr:oldSyntax
#using <mscorlib.dll>
using namespace System;

__gc struct Base {};
__gc struct Derived :     Base {};
__gc struct MoreDerived : Derived {};

int main()
{
   Base *bp = new Derived;

   try
   {
      MoreDerived *mdp = __try_cast<MoreDerived *>(bp);
   }
   catch (System::InvalidCastException *)
   {
      Console::WriteLine("Could not cast 'bp' to MoreDerived*");
   }
}
