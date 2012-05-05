 
// mcpp_nullptr_4.cpp

// compile with: /clr
using namespace System;
void f(Object ^ x) {
   Console::WriteLine("test");
}

int main() {
   f(nullptr);
}

