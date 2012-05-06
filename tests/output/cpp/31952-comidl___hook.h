// evh_server.h
#pragma once

[dual, uuid("00000000-0000-0000-0000-000000000001")]
__interface IEvents {
   [id(1)] HRESULT MyEvent([in] int value);
};

[dual, uuid("00000000-0000-0000-0000-000000000002")]
__interface IEventSource {
   [id(1)] HRESULT FireEvent();
};

class DECLSPEC_UUID ("530DF3AD-6936-3214-A83B-27B63C7997C4")CSource;

