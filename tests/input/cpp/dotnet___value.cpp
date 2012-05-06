// keyword__value.cpp
  // compile with: /clr:oldSyntax
  #using <mscorlib.dll>
  
  __value struct V { 
     int m_i;
  };
  
  int main() {
     V v1, v2;
     v1.m_i = 5;
     v2 = v1;   // copies all fields of v1 to v2
     v2.m_i = 6;   // does not affect v1.m_I
  }
  
