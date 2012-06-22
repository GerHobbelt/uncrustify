// inline_keyword1.cpp

// compile with: /c
inline int max(int a, int b)
{
   if (a > b)
   {
      return(a);
   }
   return(b);
}


__inline int max(int a, int b)
{
   if (a > b)
   {
      return(a);
   }
   return(b);
}


__forceinline int max(int a, int b)
{
   if (a > b)
   {
      return(a);
   }
   return(b);
}
