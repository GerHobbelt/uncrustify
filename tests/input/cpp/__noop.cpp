// compiler_intrinsics__noop.cpp

  // compile with or without /DDEBUG
  #include <stdio.h>
  
  #if DEBUG
     #define PRINT   printf_s
  #else
     #define PRINT   __noop
  #endif
  
  int main() {
     PRINT("\nhello\n");
  }
  
