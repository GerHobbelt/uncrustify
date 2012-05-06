// keyword__delegate.cpp

// compile with: /clr:oldSyntax
#using <mscorlib.dll>
using namespace System;

__delegate int GetDayOfWeek();
__gc class MyCalendar {
 public:
   MyCalendar() : m_nDayOfWeek(4) {}
   int MyGetDayOfWeek()
   {
      Console::WriteLine("handler");

      return(m_nDayOfWeek);
   }
   static int MyStaticGetDayOfWeek()
   {
      Console::WriteLine("static handler");

      return(6);
   }
 private:
   int m_nDayOfWeek;
};

int main()
{
   GetDayOfWeek *pGetDayOfWeek;   // declare delegate type
   int          nDayOfWeek;

   // bind delegate to static method
   pGetDayOfWeek = new GetDayOfWeek(0, &MyCalendar::MyStaticGetDayOfWeek);
   nDayOfWeek    = pGetDayOfWeek->Invoke();
   Console::WriteLine(nDayOfWeek);

   // bind delegate to instance method
   MyCalendar *pcal = new MyCalendar();
   pGetDayOfWeek = static_cast<GetDayOfWeek *>(Delegate::Combine(pGetDayOfWeek,
                                                                 new GetDayOfWeek(pcal, &MyCalendar::MyGetDayOfWeek)));
   nDayOfWeek = pGetDayOfWeek->Invoke();
   Console::WriteLine(nDayOfWeek);

   // delegate now bound to two methods; remove instance method
   pGetDayOfWeek = static_cast<GetDayOfWeek *>(Delegate::Remove(pGetDayOfWeek,
                                                                new GetDayOfWeek(pcal, &MyCalendar::MyGetDayOfWeek)));
}

