// keyword__identifier.cpp

// compile with: /clr
#using <identifier_template.dll>

int main()
{
   __identifier(template) ^ pTemplate = gcnew __identifier(template) ();
   pTemplate->Run();
}
