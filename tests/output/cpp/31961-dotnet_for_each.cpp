// for_each_string.cpp   -- MANAGED C++

// compile with: /clr
using namespace System;

ref struct MyClass
{
   property String ^ MyStringProperty;
};

int main()
{
   String ^ MyString = gcnew String("abcd");

   for each (Char c in MyString)
   {
      Console::Write(c);
   }

   Console::WriteLine();

   MyClass ^ x         = gcnew MyClass();
   x->MyStringProperty = "Testing";

   for each (Char c in x->MyStringProperty)
   {
      Console::Write(c);
   }
}

