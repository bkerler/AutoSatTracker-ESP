/* ask op for local wifi info
 */

#include "Webpage.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>


/* set up an Access Point to ask operator for local wifi info, save in NV
 */
void Webpage::askWiFi()
{
	// AP info
	IPAddress ip(192,168,10,10);
	IPAddress gw(192,168,10,10);
	IPAddress nm(255,255,255,0);
	const char *ssid = "SatTrack";

	// start AP
	resetWatchdog();
	WiFi.disconnect(true);
	delay(200);
	WiFi.mode(WIFI_AP);
	if (!WiFi.softAPConfig(ip, gw, nm)) {
	    Serial.println ("Can not configure softAP");
	    return;
	}
	if (!WiFi.softAP (ssid)) {
	    Serial.println ("Can not set AP ssid");
	    return;
	}
	delay(500);
	Serial.print (F("AP IP: ")); Serial.println (WiFi.softAPIP());

	// start HTTP server 
	resetWatchdog();
	WiFiServer remoteServer(80);
	remoteServer.begin();

	// provide clients with an IP pointing back to us
	DNSServer dns;
	dns.start(53, "*", WiFi.softAPIP());

	// repeat until get station info

	int n_info = 0;
	while (n_info < 5) {
	    n_info = 0;

	    // listen for a connection
	    WiFiClient remoteClient;
	    Serial.println ("waiting for client");
	    dns.processNextRequest();
	    do {
		resetWatchdog();
		delay(100);
		remoteClient = remoteServer.available();
	    } while (!remoteClient);
	    Serial.print (F("client connected from ")); Serial.println (remoteClient.remoteIP());

	    // send the WiFI setup page
	    sendAskPage (remoteClient);

	    // look for GET down to blank line
	    char c, line[1024];
	    size_t linel = 0;
	    char *GET = NULL;
	    uint32_t to = millis();
	    while ((c = readNextClientChar (remoteClient, &to)) != 0) {
		if (!GET) {
		    if (c == '\n') {
			line[linel] = '\0';
			linel = 0;
			if (strstr (line, "GET "))
			    GET = line;
		    } else if (linel < sizeof(line)-1)
			line[linel++] = c;
		} else {
		    if (c == '\n') {
			if (linel == 1)	// just \r\n
			    break;
			linel = 0;
		    } else {
			linel++;
		    }
		}
	    }
	    remoteClient.stop();
	    Serial.println (line);

	    // abort if no GET
	    if (!GET) {
		Serial.println (line);
		Serial.println ("No GET");
		break;
	    }

	    // crack GET for desired info
	    char *wifi_id = strstr (line, "id=");
	    char *wifi_pw = strstr (line, "pw=");
	    char *wifi_ip = strstr (line, "ip=");
	    char *wifi_nm = strstr (line, "nm=");
	    char *wifi_gw = strstr (line, "gw=");

	    // TODO: check for special characters

	    // save if looks reasonable
	    char *eos;
	    if (wifi_id) {
		wifi_id += 3;
		eos = strchr (wifi_id, '&');
		if (eos) {
		    *eos = '\0';
		    strncpy (nv->ssid, wifi_id, sizeof(nv->ssid));
		    nv->put();
		    Serial.println(nv->ssid);
		    n_info++;
		}
	    }
	    if (wifi_pw) {
		wifi_pw += 3;
		eos = strchr (wifi_pw, '&');
		if (eos) {
		    *eos = '\0';
		    strncpy (nv->pw, wifi_pw, sizeof(nv->pw));
		    nv->put();
		    Serial.println(nv->pw);
		    n_info++;
		}
	    }
	    if (wifi_ip) {
		wifi_ip += 3;
		eos = strchr (wifi_ip, '&');
		if (eos) {
		    *eos = '\0';
		    nv->IP.fromString (wifi_ip);
		    nv->put();
		    Serial.println(nv->IP.toString());
		    n_info++;
		}
	    }
	    if (wifi_nm) {
		wifi_nm += 3;
		eos = strchr (wifi_nm, '&');
		if (eos) {
		    *eos = '\0';
		    nv->NM.fromString (wifi_nm);
		    nv->put();
		    Serial.println(nv->NM.toString());
		    n_info++;
		}
	    }
	    if (wifi_gw) {
		wifi_gw += 3;
		eos = strchr (wifi_gw, ' ');
		if (eos) {
		    *eos = '\0';
		    nv->GW.fromString (wifi_gw);
		    nv->put();
		    Serial.println(nv->GW.toString());
		    n_info++;
		}
	    }
	}

	// shut down AP
	WiFi.softAPdisconnect(true);
	delay(200);
}

/* transmit the page that lets operator enter wifi info
 */
void Webpage::sendAskPage(WiFiClient client)
{
        sendHTMLHeader (client);

        // DO NOT HAND EDIT THE FOLLOWING HTML .. use "preppage.pl"
        // DO NOT REMOVE THIS LINE 333333333333333333333333333333
        client.print (F(
            "<!DOCTYPE html> \r\n"
            "<html> \r\n"
            "<head> \r\n"
            "    <meta http-equiv='Content-Type' content='text/html; charset=UTF-8' /> \r\n"
            " \r\n"
            "    <style> \r\n"
            "        body { \r\n"
            "            background-color:#888; \r\n"
            "            font-family:sans-serif; \r\n"
            "            font-size:13px; \r\n"
            "        } \r\n"
            " \r\n"
            "        table { \r\n"
            "            border-collapse: collapse; \r\n"
            "            border: 3px solid brown; \r\n"
            "            background-color:#F8F8F8; \r\n"
            "            float:left; \r\n"
            "        } \r\n"
            " \r\n"
            "        td { \r\n"
            "            padding: 4px; \r\n"
            "        } \r\n"
            " \r\n"
            "        th { \r\n"
            "            padding: 6px; \r\n"
            "            border: 1px solid brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        input { \r\n"
            "            width : 90%; \r\n"
            "        } \r\n"
            " \r\n"
            "    </style> \r\n"
            " \r\n"
            "</head> \r\n"
            "<body> \r\n"
            " \r\n"
            "    <form>  \r\n"
            "    <table> \r\n"
            "        <tr> \r\n"
            "            <th colspan='2' > \r\n"
            "                Enter local WiFi information: \r\n"
            "            </th> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td> \r\n"
            "                SSID: \r\n"
            "            </td> \r\n"
            "            <td> \r\n"
            "                <input name='id'></input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td> \r\n"
            "                Password: \r\n"
            "            </td> \r\n"
            "            <td> \r\n"
            "                <input name='pw'></input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td> \r\n"
            "                IP: \r\n"
            "            </td> \r\n"
            "            <td> \r\n"
            "                <input name='ip'></input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td> \r\n"
            "                Network mask: \r\n"
            "            </td> \r\n"
            "            <td> \r\n"
            "                <input name='nm'></input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td> \r\n"
            "                Gateway: \r\n"
            "            </td> \r\n"
            "            <td> \r\n"
            "                <input name='gw'></input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr> \r\n"
            "            <td colspan='2' > \r\n"
            "                <button type='submit'>Send</button> \r\n"
            "            </td> \r\n"
            "    </table> \r\n"
            "    </form> \r\n"
            "</body> \r\n"
            "</html> \r\n"
        ));
        // DO NOT REMOVE THIS LINE 444444444444444444444444444444
}
