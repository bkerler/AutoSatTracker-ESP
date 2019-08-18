/* this class handles the web page and interactions
 */

#ifndef _WEBPAGE_H
#define _WEBPAGE_H

#include <ESP8266WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

#include "AutoSatTracker-ESP.h"
#include "Sensor.h"
#include "Circum.h"
#include "Gimbal.h"
#include "Target.h"
#include "NV.h"

// persistent state info to fetch a TLE from a remote web site incrementally
typedef struct {
    bool running;			// set while reading a remote file
    char sat[30];			// scrubbed name of satellite we are looking for
    char buf[200];			// long enough for a complete name and TLE: ~30+70+70
    char *l0, *l1, *l2;			// start of each TLE line within buf[], l0==NULL until complete
    WiFiClient *remote;			// remote connection object
    int lineno;				// show line number as progress
} TLEFetch;

class Webpage
{

    public:

	Webpage();
	void checkEthernet();
	void setUserMessage (const __FlashStringHelper *ifsh);
	void setUserMessage (const __FlashStringHelper *ifsh, const char *msg, char state);

    private:

	WiFiServer *httpServer;

	const __FlashStringHelper *user_message_F;
	char user_message_s[100];

	void startTLEFetch (char *query_text);
	void resumeTLEFetch (void);
	TLEFetch tlef;

	bool connectWiFi();
	void askWiFi();
	void sendAskPage (WiFiClient client);
	void scrub (char *s);
	char readNextClientChar (WiFiClient client, uint32_t *to);
	void overrideValue (WiFiClient client);
	void sendMainPage (WiFiClient client);
	void sendNewValues (WiFiClient client);
	void sendPlainHeader (WiFiClient client);
	void sendHTMLHeader (WiFiClient client);
	void sendEmptyResponse (WiFiClient client);
	void send404Page (WiFiClient client);
	void reboot();
};

extern Webpage *webpage;

#endif // _WEBPAGE_H
