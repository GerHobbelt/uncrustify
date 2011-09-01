// keyword__property.cpp

  // compile with: /clr:oldSyntax
  #using <mscorlib.dll>
  using namespace System;
  
  __gc class MyClass {
  public:
     MyClass() : m_size(0) {}
     __property int get_Size() { return m_size; }
     __property void set_Size(int value) { m_size = value; }
     // compiler generates pseudo data member called Size
  protected:
     int m_size;
  };
  
  int main() {
     MyClass* class1 = new MyClass;
     int curValue;
  
     Console::WriteLine(class1->Size);
     
     class1->Size = 4;   // calls the set_Size function with value==4
     Console::WriteLine(class1->Size);
  
     curValue = class1->Size;   // calls the get_Size function
     Console::WriteLine(curValue);
  }
  
