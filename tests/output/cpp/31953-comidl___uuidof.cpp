// expre_uuidof.cpp

// compile with: ole32.lib
#include "stdio.h"
#include "windows.h"

[emitidl];
[module(name = "MyLib")];
[export]
struct stuff
{
   int i;
};

int main()
{
   LPOLESTR lpolestr;

   StringFromCLSID(__uuidof(MyLib), &lpolestr);
   wprintf_s(L"%s", lpolestr);
   CoTaskMemFree(lpolestr);
}

