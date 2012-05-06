int main(void)
{
   __try
   {
      // guarded body of code
      if (calc(1))
      {
         __leave;
      }
      calc(2);
   }
   __finally
   {
      // __finally block
      blarg(0);
   }
   return(0);
}

