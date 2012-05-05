// compiler_intrinsics__assume.cpp

#ifdef DEBUG
#define ASSERT(e)    (((e) || assert(__FILE__, __LINE__))
#else
#define ASSERT(e)    (__assume(e))
#endif

void func1(int i)
{
}

int main(int p)
{
   switch (p)
   {
   case 1:
      func1(1);
      break;

   case 2:
      func1(-1);
      break;

   default:
      __assume(0);
      // This tells the optimizer that the default
      // cannot be reached. As so, it does not have to generate
      // the extra code to check that 'p' has a value
      // not represented by a case arm. This makes the switch
      // run faster.
   }
}
