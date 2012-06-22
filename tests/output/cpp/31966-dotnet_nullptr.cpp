// mcpp_nullptr.cpp

// compile with: /clr
value class V {};
ref class G {};
void f(System::Object ^)
{
}

int main()
{
// Native pointer.
   int *pN = nullptr;

// Managed handle.
   G ^ pG  = nullptr;
   V ^ pV1 = nullptr;
// Managed interior pointer.
   interior_ptr<V> pV2 = nullptr;
// Reference checking before using a pointer.
   if (pN == nullptr)
   {
   }
   if (pG == nullptr)
   {
   }
   if (pV1 == nullptr)
   {
   }
   if (pV2 == nullptr)
   {
   }
// nullptr can be used as a function argument.
   f(nullptr);   // calls f(System::Object ^)
}

