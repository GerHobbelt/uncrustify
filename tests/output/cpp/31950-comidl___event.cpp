// EventHandling_COM_Event.cpp


// compile with: /c
#define _ATL_ATTRIBUTES    1
#include <atlbase.h>
#include <atlcom.h>

[module(dll, name = "EventSource", uuid = "6E46B59E-89C3-4c15-A6D8-B8A1CEC98830")];

[dual, uuid("00000000-0000-0000-0000-000000000002")]
__interface IEventSource {
   [id(1)] HRESULT MyEvent();
};
[coclass, uuid("00000000-0000-0000-0000-000000000003"), event_source(com)]
class CSource : public IEventSource {
 public:
   __event __interface IEventSource;
   HRESULT FireEvent()
   {
      __raise MyEvent();

      return(S_OK);
   }
};

