// generic_classes_3.cpp

// compile with: /clr /c
namespace A {
ref class MyClass {};
}

namespace B {
generic<typename ItemType>
ref class MyClass2 { };
}

namespace C {
using namespace A;
using namespace B;

ref class Test {
   static void F()
   {
      MyClass ^ m1       = gcnew MyClass();       // OK
      MyClass2<int> ^ m2 = gcnew MyClass2<int>(); // OK
   }
};
}

