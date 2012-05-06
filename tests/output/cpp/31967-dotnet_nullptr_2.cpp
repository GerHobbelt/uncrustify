// mcpp_nullptr_1.cpp

// compile with: /clr
class MyClass {
 public:
   int i;
};

int main()
{
   MyClass *pMyClass = nullptr;

   if (pMyClass == nullptr)
   {
      System::Console::WriteLine("pMyClass == nullptr");
   }

   if (pMyClass == 0)
   {
      System::Console::WriteLine("pMyClass == 0");
   }

   pMyClass = 0;
   if (pMyClass == nullptr)
   {
      System::Console::WriteLine("pMyClass == nullptr");
   }

   if (pMyClass == 0)
   {
      System::Console::WriteLine("pMyClass == 0");
   }
}
