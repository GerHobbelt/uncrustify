// EventHandlingRef_raise.cpp
  struct E {
     __event void func1();
     void func1(int) {}
  
     void func2() {}
  
     void b() {
        __raise func1();
     }
  };
  
  int main() {
     E e;
     __raise e.func1();
  }
 