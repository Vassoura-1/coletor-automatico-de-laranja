#if !defined(_OTA_SERVER_H_)
#define _OTA_SERVER_H_

#include <ArduinoOTA.h>

//Flag that gets triggered when the OTA updates start
// extern bool otaIsUpdating;

void startOtaServer(void);
void stopOtaServer(void);
void handleOtaRequests(void);

#endif // _OTA_SERVER_H_