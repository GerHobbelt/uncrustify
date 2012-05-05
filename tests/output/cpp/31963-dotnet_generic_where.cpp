// generic_classes_2.cpp

// compile with: /clr /c
interface class IItem {};
generic<class ItemType>
where ItemType : IItem
      ref class Stack {};

