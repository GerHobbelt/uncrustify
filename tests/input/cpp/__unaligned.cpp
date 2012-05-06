// unaligned_keyword.cpp
  // compile with: /c
  // processor: x64 IPF
  #include <stdio.h>
  int main() {
     char buf[100];
  
     int __unaligned *p1 = (int*)(&buf[37]);
     int *p2 = (int *)p1;
  
     *p1 = 0;   // ok
  
     __try {
        *p2 = 0;  // throws an exception
     }
     __except(1) {
        puts("exception");
     }
  }
  
