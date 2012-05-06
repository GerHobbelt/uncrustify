// evh_server.cpp

// compile with: /LD
// post-build command: Regsvr32.exe /s evh_server.dll
#define _ATL_ATTRIBUTES    1
#include <atlbase.h>
#include <atlcom.h>
#include "evh_server.h"

[module(dll, name = "EventSource", uuid = "6E46B59E-89C3-4c15-A6D8-B8A1CEC98830")];

[coclass, event_source(com), uuid("530DF3AD-6936-3214-A83B-27B63C7997C4")]
class CSource : public IEventSource {
 public:
   __event __interface IEvents;

   HRESULT FireEvent()
   {
      __raise MyEvent(123);

      return(S_OK);
   }
};
