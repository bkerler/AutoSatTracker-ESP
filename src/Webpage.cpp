/* this class handles the main web page and all interactions
 *
 * N.B. do not edit the html, edit ast.html then use preppage.pl. See README.
 */

#include <ctype.h>

#include "Webpage.h"


/* constructor
 */
Webpage::Webpage()
{
	// ask user how to connect to wifi if we can not attach
	while (!connectWiFi())
	    askWiFi();

	// create server
	resetWatchdog();
	Serial.println ("Creating ethernet server");
	httpServer = new WiFiServer(80);				// http
	httpServer->begin();
	Serial.println (WiFi.localIP());

	// init user message mechanism
	user_message_F = F("Hello+");					// page welcome message
	memset (user_message_s, 0, sizeof(user_message_s));

	// init TLE storage and state
	memset (&tlef, 0, sizeof(tlef));
	tlef.running = false;
}

/* try to connect to wifi using creds we have in EEPROM
 * return whether it connected ok
 */
bool Webpage::connectWiFi()
{
	// start over
	// WiFi.disconnect(true);
	// WiFi.softAPdisconnect(true);
	delay(400);

	// configure
	WiFi.mode(WIFI_STA);
	WiFi.begin (nv->ssid, nv->pw);
	WiFi.config (nv->IP, nv->GW, nv->NM);

	// wait for connect
	uint32_t t0 = millis();
	uint32_t timeout = 15000;					// timeout, millis()
	while (WiFi.status() != WL_CONNECTED) {
	    resetWatchdog();
	    if (millis() - t0 > timeout) {
		Serial.println (F("connect failed, starting as AP"));
		return (false);
	    }
	    delay(100);
	}

	// ok
	return (true);
}

/* remove non-alphanumeric chars and convert to upper case IN PLACE
 */
void Webpage::scrub (char *s)
{
	char *scrub_s;
	for (scrub_s = s; *s != '\0'; s++)
	    if (isalnum(*s))
		*scrub_s++ = toupper(*s);
	*scrub_s = '\0';
}

/* call this occasionally to check for Ethernet activity
 */
void Webpage::checkEthernet()
{
	resetWatchdog();

	// do more of remote fetch if active
	resumeTLEFetch();

	// now check our page
	WiFiClient client = httpServer->available();
	if (!client)
	    return;

	// Serial.println ("client connected");
	uint32_t to = millis();		// init timeout
	char firstline[128];		// first line
	unsigned fll = 0;		// firstline length
	bool eoh = false;		// set when see end of header
	bool fldone = false;		// set when finished collecting first line
	char c, prevc = 0;		// new and previous character read from client

	// read header, collecting first line and discarding rest until find blank line
	while (!eoh && (c = readNextClientChar (client, &to))) {
	    if (c == '\n') {
		if (!fldone) {
		    firstline[fll] = '\0';
		    fldone = true;
		}
		if (prevc == '\n')
		    eoh = true;
	    } else if (!fldone && fll < sizeof(firstline)-1) {
		firstline[fll++] = c;
	    }
	    prevc = c;
	}
	if (c == 0) {
	    // Serial.println ("closing client");
	    client.stop();
	    return;
	}

	// client socket is now at first character after blank line

	// replace trailing ?time cache-buster with blank
	char *q = strrchr (firstline, '?');
	if (q)
	    *q = ' ';

	// what we do next depends on first line
	resetWatchdog();
	// Serial.println (firstline);
	if (strstr (firstline, "GET / ")) {
	    sendMainPage (client);
	} else if (strstr (firstline, "GET /getvalues.txt ")) {
	    sendNewValues (client);
	} else if (strstr (firstline, "POST / ")) {
	    overrideValue (client);
	    sendEmptyResponse (client);
	} else if (strstr (firstline, "POST /reboot ")) {
	    sendEmptyResponse (client);
	    reboot();
	} else if (strstr (firstline, "POST /start ")) {
	    target->setTrackingState(true);
	    sendEmptyResponse (client);
	} else if (strstr (firstline, "POST /stop ")) {
	    target->setTrackingState(false);
	    sendEmptyResponse (client);
	} else {
	    send404Page (client);
	}

	// finished
	// Serial.println ("closing client");
	client.stop();
}

/* given "sat,URL" search the given URL for the given satellite TLE.
 * N.B. in order for our web page to continue to function, this method is just the first step, other
 *   steps are done incrementally by resumeTLEFetch().
 */
void Webpage::startTLEFetch (char *query_text)
{
	// split query at , to get sat name and URL
	char *sat = query_text;
	char *url = strchr (query_text, ',');
	if (!url) {
	    setUserMessage (F("Invalid querySite string: "), query_text, '!');
	    return;
	}
	*url++ = '\0';		// overwrite , with EOS for sat then move to start of url

	// remove leading protocol, if any
	if (strncmp (url, "http://", 7) == 0)
	    url += 7;

	// file name
	char *path = strchr (url, '/');
	if (!path) {
	    setUserMessage (F("Invalid querySite URL: "), url, '!');
	    return;
	}
	*path++ = '\0';		// overwrite / with EOS for server then move to start of path

	// connect
	tlef.remote = new WiFiClient();
	if (!tlef.remote->connect (url, 80)) {
	    setUserMessage (F("Failed to connect to "), url, '!');
	    delete tlef.remote;
	    return;
	}

	// send query to retrieve the file containing TLEs
	// Serial.print(sat); Serial.print(F("@")); Serial.println (url);
	tlef.remote->print (F("GET /"));
	tlef.remote->print (path);
	tlef.remote->print (F(" HTTP/1.0\r\n"));
	tlef.remote->print (F("Content-Type: text/plain \r\n"));
	tlef.remote->print (F("\r\n"));

	// set up so we can resume the search later....
	scrub (sat);
	strncpy (tlef.sat, sat, sizeof(tlef.sat)-1);
	tlef.lineno = 1;
	tlef.running = true;
}

/* called to resume fetching a remote web page, started by startTLEFetch().
 * we are called periodically regardless, do nothing if no fetch is in progress.
 * we know to run based on whether tlef.remote is connected.
 */
void Webpage::resumeTLEFetch ()
{
	// skip if nothing in progress
	if (!tlef.running)
	    return;

	// init
	const uint32_t tout = millis() + 10000;		// timeout, ms
	uint8_t nfound = 0;				// n good lines found so far
	char *bp = tlef.buf;				// next buf position to use
	tlef.l0 = NULL;					// flag for sendNewValues();

	// read another line, if find sat read two more and finish up
	while (tlef.remote->connected() && nfound < 3 && millis() < tout) {
	    if (tlef.remote->available()) {
		char c = tlef.remote->read();
		if (c == '\r')
		    continue;
		if (c == '\n') {
		    // show some progress
		    char lnbuf[10];
		    setUserMessage (F("Reading line "), itoa(tlef.lineno++, lnbuf, 10), '+');

		    *bp++ = '\0';
		    switch (nfound) {
		    case 0:
			char sl0[50];			// copy enough that surely contains sat
			strncpy (sl0, tlef.buf, sizeof(sl0)-1);
			sl0[sizeof(sl0)-1] = '\0';	// insure EOS
			scrub (sl0);			// scrub the copy so l0 remains complete
			if (strstr (sl0, tlef.sat)) {	// look for scrubbed sat in scrubbed l0
			    // found sat, prepare to collect TLE line 1 in l1
			    nfound++;			// found name
			    tlef.l0 = tlef.buf;		// l0 begins at buf
			    tlef.l1 = bp;		// l1 begins at next buf position
			} else
			    return;			// try next line on next call
			break;
		    case 1:
			if (target->tleValidChecksum(tlef.l1)) {
			    // found TLE line 1, prep for line 2
			    nfound++;			// found l1
			    tlef.l2 = bp;		// l2 begins at next buf position
			} else {
			    nfound = 0;			// no good afterall
			    tlef.l0 = NULL;		// reset flag for sendNewValues()
			}
			break;
		    case 2:
			if (target->tleValidChecksum(tlef.l2)) {
			    // found last line
			    nfound++;			// found l2
			} else {
			    nfound = 0;			// no good afterall
			    tlef.l0 = NULL;		// reset flag for sendNewValues()
			}
			break;
		    default:
			// can't happen ;-)
			break;
		    }
		} else if (bp - tlef.buf < (int)(sizeof(tlef.buf)-1))
		    *bp++ = c;				// add to buf iif room, including EOS
	    } else {
		// static long n;
		// Serial.println (n++);
	    }
	}

	// get here if remote disconnected, found sat or timed out

	if (!tlef.remote->connected())
	    setUserMessage (F("TLE not found!"));
	else if (nfound == 3)
	    setUserMessage (F("Found TLE: "), tlef.l0, '+');
	else
	    setUserMessage (F("Remote site timed out!"));

	// finished regardless
	tlef.remote->stop();
	delete tlef.remote;
	tlef.running = false;
}

/* record a brief F() message to inform the user, it will be sent on the next sendNewValues() sweep
 */
void Webpage::setUserMessage (const __FlashStringHelper *ifsh)
{
	user_message_F = ifsh;
	user_message_s[0] = '\0';
}

/* record a brief message to inform the user, it will be sent on the next sendNewValues() sweep.
 * the message consists of an F() string, then a stack string, then a trailing character, typically
 *  '!' to indicate an alarm, '+' to indicate good progress, or '\0' for no effect.
 */
void Webpage::setUserMessage (const __FlashStringHelper *ifsh, const char *msg, char state)
{
	user_message_F = ifsh;
	strncpy (user_message_s, msg, sizeof(user_message_s)-2);	// room for state and EOS
	user_message_s[strlen(user_message_s)] = state;
}

/* read next character, return 0 if it disconnects or times out.
 * '\r' is discarded completely.
 */
char Webpage::readNextClientChar (WiFiClient client, uint32_t *to)
{
	static const int timeout = 1000;		// client socket timeout, ms
	resetWatchdog();
	while (client.connected()) {
	    if (millis() > *to + timeout) {
		Serial.println ("client timed out");
		return (0);
	    }
	    if (!client.available())
		continue;
	    char c = client.read();
	    *to = millis();
	    if (c == '\r')
		continue;
	    // Serial.write(c);
	    return (c);
	}
	// Serial.println ("client disconnected");
	return (0);
}

/* op has entered manually a value to be overridden.
 * client is at beginning of NAME=VALUE line, parse and send to each subsystem
 * N.B. a few are treated specially.
 */
void Webpage::overrideValue (WiFiClient client)
{
	char c, buf[200];			// must be at least enough for a known-valid TLE
	uint8_t nbuf = 0;			// type must be large enough to count to sizeof(buf)

	// read next line into buf
	uint32_t to = millis();		// init timeout
	while ((c = readNextClientChar (client, &to)) != 0) {
	    if (c == '\n') {
		buf[nbuf++] = '\0';
		break;
	    } else if (nbuf < sizeof(buf)-1)
		buf[nbuf++] = c;
	}
	if (c == 0)
	    return;		// bogus; let caller close

	// break at = into name, value
	char *valu = strchr (buf, '=');
	if (!valu)
	    return;		// bogus; let caller close
	*valu++ = '\0';	// replace = with 0 then valu starts at next char
	// now buf is NAME and valu is VALUE

	Serial.print (F("Override: ")); Serial.print (buf); Serial.print("="); Serial.println (valu);

	if (strcmp (buf, "T_TLE") == 0) {

	    // T_TLE needs two more lines

	    char *l1 = valu;		// TLE target name is valu
	    char *l2 = &buf[nbuf];	// line 2 begins after valu
	    char *l3 = NULL;		// set when find end of line 2

	    // scan for two more lines
	    uint8_t nlines = 1;
	    while (nlines < 3 && (c = readNextClientChar (client, &to)) != 0) {
		if (c == '\n') {
		    buf[nbuf++] = '\0';
		    if (++nlines == 2)
			l3 = &buf[nbuf];	// line 3 starts next
		} else if (nbuf < sizeof(buf)-1)
		    buf[nbuf++] = c;
	    }
	    if (nlines < 3)
		return;	// premature end, let caller close

	    // new target!
	    target->setTLE (l1, l2, l3);

	} else if (strcmp (buf, "IP") == 0) {

	    // op is setting a new IP, save in EEPROM for use on next reboot
	    char *octet = valu;
	    for (uint8_t i = 0; i < 4; i++) {
		int o = atoi(octet);
		if (o < 0 || o > 255)
		    return;				// bogus IP
		nv->IP[i] = o;
		if (i == 3)
		    break;
		octet = strchr (octet, '.');		// find next .
		if (!octet)
		    return;				// bogus format
		octet++;				// point to first char after .
	    }
	    nv->put();
	    setUserMessage (F("Successfully stored new IP address in EEPROM -- reboot to engage+"));

	} else if (strcmp (buf, "querySite") == 0) {

	    // op wants to look up a target at a web site, valu is target,url
	    startTLEFetch (valu);

	} else {

	    // not ours, give to each other subsystem in turn until one accepts
	    if (!circum->overrideValue (buf, valu)
			&& !gimbal->overrideValue (buf, valu)
			&& !target->overrideValue (buf, valu)
			&& !sensor->overrideValue (buf, valu))
		setUserMessage (F("Bug: unknown override -- see Serial Monitor!"));

	}
}

/* inform each subsystem to send its latest values, including ours
 */
void Webpage::sendNewValues (WiFiClient client)
{
	// send plain text header for NAME=VALUE pairs
	sendPlainHeader(client);

	// send user message
	client.print ("op_message=");
	if (user_message_F != NULL)
	    client.print (user_message_F);
	if (user_message_s[0])
	    client.print (user_message_s);
	client.println();

	// send our values
	client.print ("IP=");
	for (uint8_t i = 0; i < 4; i++) {
	    client.print (nv->IP[i]);
	    if (i < 3)
		client.print (F("."));
	}
	client.println (F(""));

	if (tlef.l0) {
	    // set newly fetched name on web page
	    client.print (F("new_TLE="));
	    client.println (tlef.l0);
	    client.println (tlef.l1);
	    client.println (tlef.l2);
	    tlef.l0 = NULL;			// just send once
	}

	client.print (F("uptime="));
	circum->printSexa (client, millis()/1000.0/3600.0);
	circum->printPL (client, Circum::NORMAL);

	// send whatever the other modules want to
	circum->sendNewValues (client);
	gimbal->sendNewValues (client);
	sensor->sendNewValues (client);
	target->sendNewValues (client);

}

/* send the main page, in turn it will send us commands using XMLHttpRequest
 */
void Webpage::sendMainPage (WiFiClient client)
{
	sendHTMLHeader (client);

	// DO NOT HAND EDIT THE FOLLOWING HTML .. use "preppage.pl"
	// DO NOT REMOVE THIS LINE 111111111111111111111111111111
        client.print (F(
            "<!DOCTYPE html> \r\n"
            "<html> \r\n"
            "<head> \r\n"
            "    <meta http-equiv='Content-Type' content='text/html; charset=UTF-8' /> \r\n"
            "    <title>Sat Tracker</title> \r\n"
            " \r\n"
            "    <style> \r\n"
            " \r\n"
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
            "        th { \r\n"
            "            padding: 6px; \r\n"
            "            border: 1px solid brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .even-row { \r\n"
            "            background-color:#F8F8F8; \r\n"
            "        } \r\n"
            " \r\n"
            "        .odd-row { \r\n"
            "            background-color:#D8D8D8; \r\n"
            "        } \r\n"
            " \r\n"
            "        #title-row { \r\n"
            "            text-align: center; \r\n"
            "            padding: 2px; \r\n"
            "            border-bottom: 6px double brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        #title-label { \r\n"
            "            font-size: 18px; \r\n"
            "            font-weight: bold; \r\n"
            "            color: #0066CC; \r\n"
            "        } \r\n"
            " \r\n"
            "        #title-attrib { \r\n"
            "            font-size: 8px; \r\n"
            "            font-weight: bold; \r\n"
            "            color: #0066CC; \r\n"
            "        } \r\n"
            " \r\n"
            "        a { \r\n"
            "            color: #0066CC; \r\n"
            "        } \r\n"
            " \r\n"
            "        #op_message { \r\n"
            "            font-size:16px; \r\n"
            "            display: block; \r\n"
            "            padding: 10px; \r\n"
            "        } \r\n"
            " \r\n"
            "        td { \r\n"
            "            padding: 6px; \r\n"
            "            border: 1px solid #0066CC; \r\n"
            "        } \r\n"
            " \r\n"
            "        .TLE-display { \r\n"
            "            background-color:#D8D8D8; \r\n"
            "            font-family:monospace; \r\n"
            "            resize:none; \r\n"
            "            font-size:inherit; \r\n"
            "            overflow:hidden; \r\n"
            "            border: 1px solid brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .TLE-entry { \r\n"
            "            background-color:#FFF; \r\n"
            "            font-family:monospace; \r\n"
            "            resize:none; \r\n"
            "            font-size:inherit; \r\n"
            "            overflow:hidden; \r\n"
            "            border: 1px solid brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .major-section { \r\n"
            "            border-top: 6px double brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .minor-section { \r\n"
            "            border-top: 4px double brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .override { \r\n"
            "            background-color:#FFF; \r\n"
            "            padding: 0px; \r\n"
            "            font-family:monospace; \r\n"
            "            resize:none; \r\n"
            "            font-size:inherit; \r\n"
            "            width:7em; \r\n"
        ));
        client.print (F(
            "        } \r\n"
            " \r\n"
            "        .group-head { \r\n"
            "            text-align:center; \r\n"
            "            vertical-align:top; \r\n"
            "            border-right: 4px double brown; \r\n"
            "        } \r\n"
            " \r\n"
            "        .datum-label { \r\n"
            "            text-align:left; \r\n"
            "            vertical-align:top; \r\n"
            "            color:black; \r\n"
            "        } \r\n"
            " \r\n"
            "        .datum { \r\n"
            "            font-family:monospace; \r\n"
            "            text-align:right; \r\n"
            "            color:black \r\n"
            "        } \r\n"
            " \r\n"
            "        #tracking { \r\n"
            "            font-size: 14px; \r\n"
            "            font-weight: bold; \r\n"
            "        } \r\n"
            " \r\n"
            " \r\n"
            "    </style> \r\n"
            " \r\n"
            "    <script> \r\n"
            " \r\n"
            "        // labels on track button determine state \r\n"
            "        var tracking_on_label = 'Stop Tracking'; \r\n"
            "        var tracking_off_label = 'Start Tracking'; \r\n"
            " \r\n"
            "        // sky path border \r\n"
            "        var sky_border = 25; \r\n"
            " \r\n"
            "        // handy shortcut \r\n"
            "        function byId (id) { \r\n"
            "            return document.getElementById(id); \r\n"
            "        } \r\n"
            " \r\n"
            "        // separate window for plotting skypath \r\n"
            "        var skypathwin = undefined; \r\n"
            " \r\n"
            "        // called once after DOM is loaded \r\n"
            "        window.onload = function() { \r\n"
            "            createSkyPathWin(); \r\n"
            "            setTimeout (queryNewValues, 1000); \r\n"
            "        } \r\n"
            " \r\n"
            "        // close skyplotwin when main page is closed \r\n"
            "        window.onunload = function() { \r\n"
            "            if (skypathwin) \r\n"
            "                skypathwin.close(); \r\n"
            "        } \r\n"
            " \r\n"
            "        // create separate skypathwin filled with a canvas \r\n"
            "        function createSkyPathWin() { \r\n"
            " \r\n"
            "            // create a new HTML window \r\n"
            "            skypathwin = window.open ('', '_blank', 'width=350,height=350,scrollbars=no'); \r\n"
            "            if (!skypathwin) { \r\n"
            "                alert (\"Please turn off Popup blocker if you want to see the Sky Path plot\"); \r\n"
            "                return; \r\n"
            "            } \r\n"
            "            skypathwin.document.write ('<!DOCTYPE html><html></html>'); \r\n"
            " \r\n"
            "            // fill with a canvas \r\n"
            "            var controls = '<head><title> Sky Path </title></head>'; \r\n"
            "            controls += '<body>'; \r\n"
            "            controls += '  <canvas id=\"skypath\" > </canvas>'; \r\n"
            "            controls += '</body>'; \r\n"
            "            skypathwin.document.documentElement.innerHTML = controls; \r\n"
            " \r\n"
            "            // connect click handler \r\n"
            "            var cvs = skypathwin.document.getElementById ('skypath'); \r\n"
        ));
        client.print (F(
            "            cvs.addEventListener (\"click\", onSkyPathClick); \r\n"
            " \r\n"
            "        } \r\n"
            " \r\n"
            "        // called when user clicks on the sky path, send Az/El as if override \r\n"
            "        function onSkyPathClick(event) { \r\n"
            "            var cvs = skypathwin.document.getElementById ('skypath'); \r\n"
            "            var rect = cvs.getBoundingClientRect(); \r\n"
            "            var cvsw = rect.right - rect.left; \r\n"
            "            var cvsh = rect.bottom - rect.top; \r\n"
            "            var hznr = Math.min(cvsh,cvsw)/2 - sky_border; \r\n"
            "            var skye = event.clientX-rect.left-cvsw/2;                        // right from center \r\n"
            "            var skyn = cvsh/2-(event.clientY-rect.top);                        // up from center \r\n"
            "            var az = (180.0/Math.PI*Math.atan2(skye, skyn)+360) % 360; \r\n"
            "            var el = 90*(1-Math.hypot(skye,skyn)/hznr); \r\n"
            " \r\n"
            "            POSTNV ('T_Az', az); \r\n"
            "            POSTNV ('T_El', el); \r\n"
            "        } \r\n"
            "         \r\n"
            "        // query for new values forever \r\n"
            "        function queryNewValues() { \r\n"
            "            var xhr = new XMLHttpRequest(); \r\n"
            "            xhr.onreadystatechange = function() { \r\n"
            "                if (xhr.readyState==4 && xhr.status==200) { \r\n"
            "                    // response is id=value pairs, one per line, end ! warning + good \r\n"
            "                    // id is in DOM but some require special treatment. \r\n"
            "                    var lines = xhr.responseText.replace(/\\r/g,'').split('\\n'); \r\n"
            "                    for (var i = 0; i < lines.length; i++) { \r\n"
            "                        console.log('getvalues line ' + i + ': ' + lines[i]); \r\n"
            "                        var nv = lines[i].trim().split('='); \r\n"
            "                        if (nv.length != 2) \r\n"
            "                            continue; \r\n"
            "                        var id = byId (nv[0]); \r\n"
            "                        if (nv[0] == 'T_TLE' || nv[0] == 'new_TLE') { \r\n"
            "                            console.log('getvalues line ' + (i+1) + ': ' + lines[i+1]); \r\n"
            "                            console.log('getvalues line ' + (i+2) + ': ' + lines[i+2]); \r\n"
            "                            id.value = nv[1] + '\\n' + lines[i+1] + '\\n' + lines[i+2]; \r\n"
            "                            i += 2; \r\n"
        ));
        client.print (F(
            "                        } else if (nv[0] == 'skypath') { \r\n"
            "                            plotSkyPath (nv[1]); \r\n"
            "                        } else if (nv[0] == 'tracking') { \r\n"
            "                            setTrackingButton (nv[1] == 'On' ? tracking_on_label : tracking_off_label); \r\n"
            "                        } else if (nv[0] == 'IP') { \r\n"
            "                            setNewIP (nv[1]); \r\n"
            "                        } else if (nv[0] == 'GPS_Enable') { \r\n"
            "                            setGPSEnable(nv[1]); \r\n"
            "                        } else if (nv[0] == 'SS_Save') { \r\n"
            "                            setSSSave(nv[1]); \r\n"
            "                        } else { \r\n"
            "                            var l = nv[1].length; \r\n"
            "                            if (nv[1].substr(l-1) == '!') { \r\n"
            "                                // warning \r\n"
            "                                id.innerHTML = nv[1].substr(0,l-1); \r\n"
            "                                id.style.color = 'red'; \r\n"
            "                            } else if (nv[1].substr(l-1) == '+') { \r\n"
            "                                // good news \r\n"
            "                                id.innerHTML = nv[1].substr(0,l-1); \r\n"
            "                                id.style.color = '#297'; \r\n"
            "                            } else { \r\n"
            "                                // normal \r\n"
            "                                id.innerHTML = nv[1]; \r\n"
            "                                id.style.color = 'black'; \r\n"
            "                            } \r\n"
            "                        } \r\n"
            "                    } \r\n"
            " \r\n"
            "                    // repeat after a short breather \r\n"
            "                    setTimeout (queryNewValues, 1000); \r\n"
            "                } \r\n"
            "            } \r\n"
            "            xhr.open('GET', UniqURL('/getvalues.txt'), true); \r\n"
            "            xhr.send(); \r\n"
            "        } \r\n"
            " \r\n"
            "        // handy function to POST a name=value pair \r\n"
            "        function POSTNV (name, value) { \r\n"
            "            var xhr = new XMLHttpRequest(); \r\n"
            "            xhr.open('POST', UniqURL('/'), true); \r\n"
            "            xhr.send(name + '=' + String(value) + '\\r\\n'); \r\n"
            "        } \r\n"
            " \r\n"
            "        // handy function that modifies a URL to be unique so it voids the cache \r\n"
            "        function UniqURL (url) { \r\n"
            "            return (url + '?' + (new Date()).getTime()); \r\n"
        ));
        client.print (F(
            "        } \r\n"
            " \r\n"
            "        // plot a skypath, points are az,el;... \r\n"
            "        function plotSkyPath (points) { \r\n"
            "            // ignore if not built yet \r\n"
            "            if (!skypathwin) \r\n"
            "                return; \r\n"
            " \r\n"
            "            // render to off-screen canvas then blit to void flashing \r\n"
            "            var cvs = skypathwin.document.createElement ('canvas'); \r\n"
            " \r\n"
            "            // track current window size \r\n"
            "            var cvsw = skypathwin.innerWidth - 20;                // ~ 20 to eliminate scroll bars \r\n"
            "            var cvsh = skypathwin.innerHeight - 20; \r\n"
            "            cvs.width = cvsw; \r\n"
            "            cvs.height = cvsh; \r\n"
            "            var ctx = cvs.getContext ('2d'); \r\n"
            " \r\n"
            "            var hznr = Math.min(cvsw,cvsh)/2 - sky_border;        // horizon radius \r\n"
            " \r\n"
            "            // local function to convert az el in degrees to skypath canvas x y \r\n"
            "            function azel2xy (az, el) { \r\n"
            "                var az = Math.PI*az/180;                        // cw from up, rads \r\n"
            "                var zr = hznr*(90-el)/90;                        // radius in pixels \r\n"
            "                return { \r\n"
            "                    x: cvsw/2 + zr*Math.sin(az),                // right \r\n"
            "                    y: cvsh/2 - zr*Math.cos(az),                // up is -  \r\n"
            "                }; \r\n"
            "            } \r\n"
            " \r\n"
            "            // cleaner looking lines if center on pixels \r\n"
            "            ctx.setTransform (1, 0, 0, 1, 0, 0); \r\n"
            "            ctx.translate (0.5, 0.5); \r\n"
            " \r\n"
            "            // reset background \r\n"
            "            ctx.strokeStyle = 'black' \r\n"
            "            ctx.fillStyle = '#EEE' \r\n"
            "            ctx.beginPath(); \r\n"
            "                ctx.rect (0, 0, cvsw-1, cvsh-1); \r\n"
            "            ctx.fill(); \r\n"
            "            ctx.stroke(); \r\n"
            " \r\n"
            "            // draw az and el lines \r\n"
            "            ctx.strokeStyle = '#777' \r\n"
            "            ctx.beginPath(); \r\n"
            "                for (var i = 1; i <= 3; i++) { \r\n"
            "                    ctx.moveTo (cvsw/2+i*hznr/3, cvsh/2); \r\n"
            "                    ctx.arc (cvsw/2, cvsh/2, i*hznr/3, 0, 2*Math.PI); \r\n"
            "                } \r\n"
            "                for (var i = 0; i < 12; i++) { \r\n"
            "                    ctx.moveTo (cvsw/2, cvsh/2); \r\n"
            "                    var xy = azel2xy (i*360/12, 0); \r\n"
            "                    ctx.lineTo (xy.x, xy.y); \r\n"
        ));
        client.print (F(
            "                } \r\n"
            "            ctx.stroke(); \r\n"
            " \r\n"
            "            // label cardinal directions \r\n"
            "            ctx.fillStyle = '#297' \r\n"
            "            ctx.font = '18px Arial'; \r\n"
            "            ctx.fillText ('N', cvsw/2-5, cvsh/2-hznr-4); \r\n"
            "            ctx.fillText ('E', cvsw/2+hznr+4, cvsh/2+5); \r\n"
            "            ctx.fillText ('S', cvsw/2-5, cvsh/2+hznr+20); \r\n"
            "            ctx.fillText ('W', cvsw/2-hznr-sky_border+5, cvsh/2+5); \r\n"
            " \r\n"
            "            // split path into individual points, we always get one even if points is empty \r\n"
            "            var pts = points.replace(/;$/,'').split(/;/); \r\n"
            "            if (pts.length > 1) { \r\n"
            "                // path, plotted N up E right \r\n"
            "                ctx.strokeStyle = '#22A' \r\n"
            "                ctx.lineWidth = 2; \r\n"
            "                ctx.beginPath(); \r\n"
            "                    for (var i = 0; i < pts.length; i++) { \r\n"
            "                        var azel = pts[i].split(/,/); \r\n"
            "                        var xy = azel2xy (azel[0], azel[1]); \r\n"
            "                        if (i == 0) \r\n"
            "                            ctx.moveTo (xy.x, xy.y); \r\n"
            "                        else \r\n"
            "                            ctx.lineTo (xy.x, xy.y); \r\n"
            "                    } \r\n"
            "                ctx.stroke(); \r\n"
            " \r\n"
            "                // label set location \r\n"
            "                // N.B. first location will not be rise if path computed while up \r\n"
            "                ctx.font = '14px Arial'; \r\n"
            "                ctx.fillStyle = '#22A'; \r\n"
            "                var az = pts[pts.length-1].split(/,/)[0]; \r\n"
            "                var el = (az < 90 || az > 270) ? -5 : -12; \r\n"
            "                var xy = azel2xy (az, el); \r\n"
            "                ctx.fillText ('S', xy.x-5, xy.y); \r\n"
            "            } \r\n"
            " \r\n"
            "            // plot target, get loc from text fields \r\n"
            "            var taz = parseFloat(byId('T_Az').innerHTML); \r\n"
            "            var tel = parseFloat(byId('T_El').innerHTML); \r\n"
            "            if (tel >= 0) { \r\n"
            "                var xy = azel2xy (taz, tel); \r\n"
            "                ctx.fillStyle = '#FD0000'; \r\n"
            "                var r = 5; \r\n"
            "                var px = cvsw/2-hznr-sky_border+r+5, py = cvsh/2-hznr-8-r; \r\n"
            "                ctx.beginPath(); \r\n"
            "                    ctx.moveTo (xy.x + r, xy.y); \r\n"
            "                    ctx.arc (xy.x, xy.y, r, 0, 2*Math.PI); \r\n"
        ));
        client.print (F(
            "                    ctx.moveTo (px + r, py); \r\n"
            "                    ctx.arc (px, py, r, 0, 2*Math.PI); \r\n"
            "                ctx.fill(); \r\n"
            "                ctx.fillStyle = 'black' \r\n"
            "                ctx.font = '14px Arial'; \r\n"
            "                ctx.fillText ('Target', px+2*r, py+r); \r\n"
            "            } \r\n"
            " \r\n"
            "            // plot sensor position, get loc from text fields \r\n"
            "            var saz = parseFloat(byId('SS_Az').innerHTML); \r\n"
            "            var sel = parseFloat(byId('SS_El').innerHTML); \r\n"
            "            if (sel >= -10) {        // allow for symbol size \r\n"
            "                var xy = azel2xy (saz, sel); \r\n"
            "                ctx.strokeStyle = '#297' \r\n"
            "                var r = 8; \r\n"
            "                var px = cvsw/2-hznr-sky_border+r+5, py = cvsh/2+hznr+(20-r); \r\n"
            "                ctx.beginPath(); \r\n"
            "                    ctx.moveTo (xy.x + r, xy.y); \r\n"
            "                    ctx.arc (xy.x, xy.y, r, 0, 2*Math.PI); \r\n"
            "                    ctx.moveTo (px + r, py); \r\n"
            "                    ctx.arc (px, py, r, 0, 2*Math.PI); \r\n"
            "                ctx.stroke(); \r\n"
            "                ctx.fillStyle = 'black' \r\n"
            "                ctx.font = '14px Arial'; \r\n"
            "                ctx.fillText ('Sensor', px+2*r, py+r); \r\n"
            "            } \r\n"
            " \r\n"
            "            // display pointing error from text fields \r\n"
            "            // haversine form is better than law of cosines for small separations \r\n"
            "            var tazr = taz*Math.PI/180; \r\n"
            "            var telr = tel*Math.PI/180; \r\n"
            "            var sazr = saz*Math.PI/180; \r\n"
            "            var selr = sel*Math.PI/180; \r\n"
            "            // var sep = 180/Math.PI*acos(sin(telr)*sin(selr) + cos(telr)*cos(selr)*sin(tazr-sazr)); \r\n"
            "            var delel = telr - selr; \r\n"
            "            var delaz = tazr - sazr; \r\n"
            "            var a = Math.sin(delel/2)*Math.sin(delel/2) + \r\n"
            "                            Math.cos(telr) * Math.cos(selr) * Math.sin(delaz/2) * Math.sin(delaz/2); \r\n"
            "            var sep = 2*180/Math.PI*Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); \r\n"
            "            if (!isNaN(sep)) { \r\n"
            "                ctx.fillStyle = 'black' \r\n"
            "                ctx.font = '14px Arial'; \r\n"
            "                ctx.fillText ('Error: ' + sep.toFixed(1) + '\\u00B0', cvsw/2+hznr+sky_border-90, cvsh/2-hznr-8); \r\n"
        ));
        client.print (F(
            "            } \r\n"
            " \r\n"
            "            // blit onto screen \r\n"
            "            var spcvs = skypathwin.document.getElementById ('skypath'); \r\n"
            "            spcvs.width = cvsw; \r\n"
            "            spcvs.height = cvsh; \r\n"
            "            var spctx = spcvs.getContext ('2d'); \r\n"
            "            spctx.drawImage (cvs, 0, 0); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called when op wants to read a TLE from a file \r\n"
            "        function handleFileSelect(file) { \r\n"
            " \r\n"
            "            // get target \r\n"
            "            var target = byId('target_name').value.trim(); \r\n"
            "            if (!target || target.length<1) { \r\n"
            "                alert ('Please enter name of target in file'); \r\n"
            "                return; \r\n"
            "            } \r\n"
            " \r\n"
            "            // use FileReader to read file \r\n"
            "            var reader = new FileReader(); \r\n"
            " \r\n"
            "            // define callback called after reader is triggered \r\n"
            "            reader.onload = function(event) { \r\n"
            "                var fr = event.target;                      // FileReader \r\n"
            "                var text = fr.result;                       // file text as string \r\n"
            " \r\n"
            "                // scan file looking for named target, allowing a very generous match \r\n"
            "                var target_scrubbed = target.replace(/\\W/g,'').toUpperCase(); \r\n"
            "                var lines = text.replace(/\\r/g,'').split(/\\n/); \r\n"
            "                for (i = 0; i < lines.length; i++) { \r\n"
            "                    var line = lines[i].trim(); \r\n"
            "                    var line_scrubbed = line.replace(/\\W/g,'').toUpperCase(); \r\n"
            "                    if (line_scrubbed.indexOf(target_scrubbed) >= 0) { \r\n"
            "                        if (i < lines.length-2) { \r\n"
            "                            var l1 = lines[i+1].trim(); \r\n"
            "                            var l2 = lines[i+2].trim(); \r\n"
            "                            var candidate = line + '\\n' + l1 + '\\n' + l2; \r\n"
            "                            var telok = validateTLE(candidate); \r\n"
            "                            if (telok != null) \r\n"
            "                                alert ('Found \"' + target + '\" in ' + file.name + ' but ' + telok); \r\n"
            "                            else \r\n"
            "                                byId('new_TLE').value = candidate; \r\n"
            "                        } else \r\n"
            "                            alert ('Found \"' + target + '\" in ' + file.name + ' but not followed by a valid TLE'); \r\n"
        ));
        client.print (F(
            "                        return; \r\n"
            "                    } \r\n"
            "                } \r\n"
            "                alert ('Can not find \"' + target + '\" in file ' + file.name); \r\n"
            " \r\n"
            "            }; \r\n"
            " \r\n"
            "            // read file, triggers onload when complete \r\n"
            "            reader.readAsText(file); \r\n"
            "        } \r\n"
            " \r\n"
            " \r\n"
            " \r\n"
            "        // send new value in response to operator typing an override value. \r\n"
            "        function onOvd(event) { \r\n"
            "            if (event.keyCode == 13) { \r\n"
            "                var oid = event.target.id; \r\n"
            "                var nam = oid.replace ('_Ovd', ''); \r\n"
            "                var vid = byId(nam); \r\n"
            "                if (vid) { \r\n"
            "                    var val = event.target.value.trim(); \r\n"
            "                    POSTNV (nam, val); \r\n"
            "                } \r\n"
            "            } \r\n"
            "        } \r\n"
            " \r\n"
            "        // called when op wants to look for a target at a remote web site \r\n"
            "        function querySite(event) { \r\n"
            "            var url = event.target.value; \r\n"
            "            var sat = byId('target_name').value.trim(); \r\n"
            "            console.log(url); \r\n"
            "            POSTNV ('querySite', sat + ',' + url); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called when op wants to upload a new TLE \r\n"
            "        function onUploadTLE() { \r\n"
            "            var tid = byId('new_TLE'); \r\n"
            "            var newtle = tid.value.trim(); \r\n"
            "            var telok = validateTLE(newtle); \r\n"
            "            if (telok != null) \r\n"
            "                alert (telok); \r\n"
            "            else { \r\n"
            "                tid.value = newtle;                // show trimmed version \r\n"
            "                POSTNV ('T_TLE', newtle); \r\n"
            "            } \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to enable GPS \r\n"
            "        function onGPSEnable() { \r\n"
            "            POSTNV ('GPS_Enable', 'true'); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to save Sensor calibration to EEPROM \r\n"
            "        function onSSSave() { \r\n"
            "            POSTNV ('SS_Save', 'true'); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to upload a new IP, either with Set (k==0) or by typing Enter (k==1) \r\n"
            "        function onIP (k,event) { \r\n"
            "            if (k && event.keyCode != 13) \r\n"
            "                return;        // wait for Enter \r\n"
            "            var ip = byId ('IP').value.trim(); \r\n"
            "            var octets = ip.split(/\\./); \r\n"
            "            for (var i = 0; i < octets.length; i++) { \r\n"
        ));
        client.print (F(
            "                var o = octets[i]; \r\n"
            "                if (!o.match(/^\\d+$/)) \r\n"
            "                    break; \r\n"
            "                var v = parseInt(o); \r\n"
            "                if (v < 0 || v > 255) \r\n"
            "                    break; \r\n"
            "            } \r\n"
            "            if (octets.length != 4 || i < octets.length) \r\n"
            "                alert (ip + ': not a valid IP address format'); \r\n"
            "            else \r\n"
            "                POSTNV ('IP', ip); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to display the current IP.  N.B. leave IP text alone if it or Set currently has focus \r\n"
            "        function setNewIP (ip) { \r\n"
            "            var ip_text = byId('IP') \r\n"
            "            var ip_set  = byId('IP-set') \r\n"
            "            var focus = document.activeElement; \r\n"
            "            if (focus != ip_text && focus != ip_set) \r\n"
            "                ip_text.value = ip; \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to set visibility of GPS_Enable \r\n"
            "        function setGPSEnable (whether) { \r\n"
            "            var gid = byId ('GPS_Enable'); \r\n"
            "            gid.style.visibility = (whether == 'true') ? 'visible' : 'hidden'; \r\n"
            "        } \r\n"
            " \r\n"
            "        // called to set visibility of SS_Save \r\n"
            "        function setSSSave (whether) { \r\n"
            "            var sid = byId ('SS_Save'); \r\n"
            "            sid.style.visibility = (whether == 'true') ? 'visible' : 'hidden'; \r\n"
            "        } \r\n"
            " \r\n"
            "        // send command to start tracking \r\n"
            "        function commandStartTracking() { \r\n"
            "            var xhr = new XMLHttpRequest(); \r\n"
            "            xhr.open('POST', UniqURL('/start'), true); \r\n"
            "            xhr.send(); \r\n"
            "        } \r\n"
            " \r\n"
            "        // send command to stop tracking \r\n"
            "        function commandStopTracking() { \r\n"
            "            var xhr = new XMLHttpRequest(); \r\n"
            "            xhr.open('POST', UniqURL('/stop'), true); \r\n"
            "            xhr.send(); \r\n"
            "        } \r\n"
            " \r\n"
            "        // the Run/Stop tracking button was clicked, label determines action \r\n"
            "        function onTracking() { \r\n"
            "            var tb = byId('tracking'); \r\n"
            "            // just issue command, let next getvalues update button appearance \r\n"
            "            if (tb.innerHTML == tracking_off_label) \r\n"
            "                commandStartTracking(); \r\n"
            "            else \r\n"
            "                commandStopTracking(); \r\n"
            "        } \r\n"
            " \r\n"
            "        // given one of tracking_on/off_label, set the tracking button appearance \r\n"
        ));
        client.print (F(
            "        function setTrackingButton (label) { \r\n"
            "            var tb = byId('tracking'); \r\n"
            "            if (label == tracking_off_label) { \r\n"
            "                tb.innerHTML = label; \r\n"
            "                tb.style.color = 'black'; \r\n"
            "            } else if (label == tracking_on_label) { \r\n"
            "                tb.innerHTML = label; \r\n"
            "                tb.style.color = 'red'; \r\n"
            "            } else { \r\n"
            "                tb.innerHTML = '?'; \r\n"
            "                tb.style.color = 'blue'; \r\n"
            "            } \r\n"
            "        } \r\n"
            " \r\n"
            "        // send command to reboot the Ardunio then reload our page after a short while  \r\n"
            "        function onReboot() { \r\n"
            "            if (confirm('Are you sure you want to reboot the Tracker?')) { \r\n"
            " \r\n"
            "                var xhr = new XMLHttpRequest(); \r\n"
            "                xhr.open ('POST', UniqURL('/reboot'), true); \r\n"
            "                xhr.send (); \r\n"
            " \r\n"
            "                byId ('op_message').style.color = 'red'; \r\n"
            " \r\n"
            "                function reloadMessage (n) { \r\n"
            "                    var msg = 'This page will reload in ' + n + ' second' + ((n == 1) ? '' : 's'); \r\n"
            "                    byId ('op_message').innerHTML = msg; \r\n"
            "                    if (n == 0) \r\n"
            "                        location.reload(); \r\n"
            "                    else \r\n"
            "                        setTimeout (function() {reloadMessage(n-1);}, 1000); \r\n"
            "                } \r\n"
            "                reloadMessage(5); \r\n"
            "            } \r\n"
            "        } \r\n"
            " \r\n"
            "        // return why the given text appears not to be a valid TLE, else null \r\n"
            "        function validateTLE (text) { \r\n"
            "            var lines = text.replace(/\\r/g,'').split('\\n'); \r\n"
            "            if (lines.length != 3) \r\n"
            "                return ('TLE must be exactly 3 lines'); \r\n"
            "            var l1 = lines[0].trim(); \r\n"
            "            if (l1.length < 1) \r\n"
            "                return ('Missing name on line 1'); \r\n"
            "            var l2 = lines[1].trim(); \r\n"
            "            if (!TLEChecksum(l2)) \r\n"
            "                return ('Invalid checksum on line 2'); \r\n"
            "            var l3 = lines[2].trim(); \r\n"
            "            if (!TLEChecksum(l3)) \r\n"
            "                return ('Invalid checksum on line 3'); \r\n"
            "            return null;        // ok! \r\n"
            "        } \r\n"
            " \r\n"
            "        // return whether the given line has a valid TLE checksum \r\n"
        ));
        client.print (F(
            "        function TLEChecksum (line) { \r\n"
            "            if (line.length < 69) \r\n"
            "                return false; \r\n"
            "            var scrub = line.replace(/[^\\d-]/g,'').replace(/-/g,'1');        // only digits and - is worth 1 \r\n"
            "            var sl = scrub.length; \r\n"
            "            var sum = 0; \r\n"
            "            for (var i = 0; i < sl-1; i++)                                // last char is checksum itself \r\n"
            "                sum += parseInt(scrub.charAt(i)); \r\n"
            "            return ((sum%10) == parseInt(scrub.charAt(sl-1))); \r\n"
            "        } \r\n"
            " \r\n"
            "        // called when op wants to erase the TLE text entry \r\n"
            "        function onEraseTLE() { \r\n"
            "            byId ('new_TLE').value = ''; \r\n"
            "        } \r\n"
            " \r\n"
            " \r\n"
            "    </script>  \r\n"
            " \r\n"
            "</head> \r\n"
            " \r\n"
            "<body> \r\n"
            " \r\n"
            "    <!-- table floats left, so this actually centers what remains, ie, the skypath canvas --> \r\n"
            "    <center> \r\n"
            " \r\n"
            "    <table> \r\n"
            "        <tr> \r\n"
            "            <td id='title-row' colspan='7' > \r\n"
            "                <table style='border:none;' width='100%'> \r\n"
            "                    <tr> \r\n"
            "                        <td width='25%' style='text-align:left; border:none' > \r\n"
            "                            IP: \r\n"
            "                            <input id='IP' type='text' onkeypress='onIP(1,event)' size='14' > </input> \r\n"
            "                            <button id='IP-set' onclick='onIP(0,event)'>Change</button \r\n"
            "                        </td> \r\n"
            "                        <td width='50%' style='border:none' > \r\n"
            "                            <label id='title-label' title='Version 2018072919' >Autonomous Satellite Tracker</label> \r\n"
            "                            <br> \r\n"
            "                            <label id='title-attrib' > by \r\n"
            "                                <a target='_blank' href='http://www.clearskyinstitute.com/ham'>WB&Oslash;OEW</a> \r\n"
            "                            </label> \r\n"
            "                        </td> \r\n"
            "                        <td width='25%' style='text-align:right; border:none' > \r\n"
            "                            <button id='reboot_b' onclick='onReboot()'> Reboot Tracker </button> \r\n"
            "                            <br> \r\n"
            "                            Up: <label id='uptime' style='padding:10px;'  ></label> \r\n"
            "                        </td> \r\n"
            "                    </tr> \r\n"
        ));
        client.print (F(
            "                    <tr> \r\n"
            "                        <td colspan='3' width='100%' style='text-align:center; border:none'> \r\n"
            "                            <label id='op_message' > Hello </label> \r\n"
            "                        </td> \r\n"
            "                    </tr> \r\n"
            "                </table> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "        <tr class='major-section' > \r\n"
            "            <td colspan='7' style='text-align:left; border-bottom-style:none; font-weight:bold' > \r\n"
            "                <table width='100%' style='border:none'> \r\n"
            "                    <tr> \r\n"
            "                        <td width='33%' style='text-align:left; border:none; font-weight:bold' > \r\n"
            "                            Loaded TLE: \r\n"
            "                        </td> \r\n"
            "                        <td width='33%' style='text-align:center; border:none' > \r\n"
            "                            <button id='tracking' onclick='onTracking()' > </button> \r\n"
            "                        </td> \r\n"
            "                        <td width='33%' style='border-style:none' > \r\n"
            "                        </td> \r\n"
            "                    </tr> \r\n"
            "                </table> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "        <tr> \r\n"
            "            <td colspan='7' style='text-align:center; border:none; padding-bottom:10px' > \r\n"
            "                <textarea id='T_TLE' class='TLE-display' rows='3' cols='69' readonly> \r\n"
            "                </textarea> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "        <tr> \r\n"
            "            <td colspan='7' style='text-align:left; border: none; ' > \r\n"
            "                <table width='100%' style='border:none'> \r\n"
            "                    <tr> \r\n"
            "                        <td style='text-align:left; border:none; font-weight:bold' > \r\n"
            "                            Paste next TLE or find \r\n"
            "                            <input id='target_name' type='text' size='8' > </input> \r\n"
            "                            at \r\n"
            "                            <button onclick='querySite(event)' value='http://amsat.org/amsat/ftp/keps/current/nasa.all'>AMSAT</button> \r\n"
            "                            <button onclick='querySite(event)' value='http://celestrak.com/NORAD/elements/amateur.txt' >Celestrak</button> \r\n"
            "                            or in \r\n"
            "                            <input type='file' id='filesel' onchange='handleFileSelect(this.files[0])' /> \r\n"
        ));
        client.print (F(
            "                            <button onclick='onUploadTLE()'>Upload</button> \r\n"
            "                        </td> \r\n"
            "                        <td style='text-align:right; border:none;' > \r\n"
            "                            <button onclick='onEraseTLE()'>Erase</button> \r\n"
            "                        </td> \r\n"
            "                    </tr> \r\n"
            "                </table> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "        <tr> \r\n"
            "            <td colspan='7' style='text-align:center; border-top-style:none; border-bottom-style:none; padding-bottom:10px' > \r\n"
            "                <textarea id='new_TLE' class='TLE-entry' rows='3' cols='69' > \r\n"
            "                </textarea> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "        <tr class='major-section' > \r\n"
            "            <th class='group-head' > Subsystem </th> \r\n"
            "            <th> Parameter </th> \r\n"
            "            <th> Value </th> \r\n"
            "            <th> Override </th> \r\n"
            "            <th> Parameter </th> \r\n"
            "            <th> Value </th> \r\n"
            "            <th> Override </th> \r\n"
            "        </tr> \r\n"
            " \r\n"
            " \r\n"
            " \r\n"
            "        <tr class='minor-section even-row' > \r\n"
            "            <th rowspan='8' class='group-head' > \r\n"
            "                    Target \r\n"
            "                <br> \r\n"
            "                <label id='T_Status'></label> \r\n"
            "            </th> \r\n"
            " \r\n"
            "            <td class='datum-label' > Azimuth, degrees E of N </td> \r\n"
            "            <td id='T_Az' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='T_Az_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Next Rise in H:M:S</td> \r\n"
            "            <td id='T_NextRise' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Elevation, degrees Up </td> \r\n"
            "            <td id='T_El' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='T_El_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Next Rise Azimuth</td> \r\n"
            "            <td id='T_RiseAz' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > TLE age, days </td> \r\n"
        ));
        client.print (F(
            "            <td id='T_Age' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td id='T_NextTrans_l' class='datum-label' > Next Transit in </td> \r\n"
            "            <td id='T_NextTrans' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Range, km </td> \r\n"
            "            <td id='T_Range' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td id='T_TransAz_l' class='datum-label' > Next Transit Azimuth </td> \r\n"
            "            <td id='T_TransAz' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > Range rate, m/s</td> \r\n"
            "            <td id='T_RangeR' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td id='T_TransEl_l' class='datum-label' > Next Transit Elevation </td> \r\n"
            "            <td id='T_TransEl' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Doppler, kHz @ 144 MHz</td> \r\n"
            "            <td id='T_VHFDoppler' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Next Set in </td> \r\n"
            "            <td id='T_NextSet' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > Doppler, kHz @ 440 MHz</td> \r\n"
            "            <td id='T_UHFDoppler' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Next Set Azimuth</td> \r\n"
            "            <td id='T_SetAz' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Sunlit</td> \r\n"
            "            <td id='T_Sunlit' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td id='T_Up_l' class='datum-label' > Next pass duration </td> \r\n"
            "            <td id='T_Up' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            " \r\n"
            "        <tr class='minor-section even-row' > \r\n"
            "            <th rowspan='4' class='group-head' > \r\n"
            "                    Spatial sensor \r\n"
            "                <br> \r\n"
            "                <label id='SS_Status'></label> \r\n"
        ));
        client.print (F(
            "                <br> \r\n"
            "                <button id='SS_Save' onclick='onSSSave()' > Save Cal </button> \r\n"
            "            </th> \r\n"
            " \r\n"
            "            <td class='datum-label' > Azimuth, degrees E of N </td> \r\n"
            "            <td id='SS_Az' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > System status, 0 .. 3 </td> \r\n"
            "            <td id='SS_SStatus' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Elevation, degrees Up </td> \r\n"
            "            <td id='SS_El' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Gyro status </td> \r\n"
            "            <td id='SS_GStatus' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > Temperature, degrees C </td> \r\n"
            "            <td id='SS_Temp' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Magnetometer status </td> \r\n"
            "            <td id='SS_MStatus' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' ></td> \r\n"
            "            <td id='SS_XXX' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Accelerometer status </td> \r\n"
            "            <td id='SS_AStatus' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            " \r\n"
            " \r\n"
            " \r\n"
            "        <tr class='minor-section even-row' > \r\n"
            "            <th rowspan='4' class='group-head' > \r\n"
            "                    GPS \r\n"
            "                <br> \r\n"
            "                <label id='GPS_Status'></label> \r\n"
            "                <br> \r\n"
            "                <button id='GPS_Enable' onclick='onGPSEnable()' > Enable </button> \r\n"
            "            </th> \r\n"
            " \r\n"
            "            <td class='datum-label' > UTC, H:M:S </td> \r\n"
            "            <td id='GPS_UTC' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='GPS_UTC_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Altitude, m </td> \r\n"
            "            <td id='GPS_Alt' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='GPS_Alt_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
        ));
        client.print (F(
            "                </input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Date, Y M D </td> \r\n"
            "            <td id='GPS_Date' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='GPS_Date_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Mag decl, true - mag </td> \r\n"
            "            <td id='GPS_MagDecl' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > Latitude, degrees +N </td> \r\n"
            "            <td id='GPS_Lat' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='GPS_Lat_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > HDOP, ~1 .. 20 </td> \r\n"
            "            <td id='GPS_HDOP' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Longitude, degrees +E </td> \r\n"
            "            <td id='GPS_Long' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='GPS_Long_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > N Satellites </td> \r\n"
            "            <td id='GPS_NSat' class='datum' > </td> \r\n"
            "            <td></td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            " \r\n"
            " \r\n"
            "        <!-- N.B. beware that some ID's are used in a match in onOvd(event) --> \r\n"
            "        <tr class='minor-section even-row ' > \r\n"
            "            <th rowspan='3' class='group-head' > \r\n"
            "                    Gimbal \r\n"
            "                <br> \r\n"
            "                <label id='G_Status'></label> \r\n"
            "            </th> \r\n"
            " \r\n"
            "            <td class='datum-label' > Servo 1 pulse length, &micro;s </td> \r\n"
            "            <td id='G_Mot1Pos' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot1Pos_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Servo 2 pulse length, &micro;s </td> \r\n"
        ));
        client.print (F(
            "            <td id='G_Mot2Pos' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot2Pos_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr class='odd-row' > \r\n"
            "            <td class='datum-label' > Servo 1 minimum pulse </td> \r\n"
            "            <td id='G_Mot1Min' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot1Min_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Servo 2 minimum pulse </td> \r\n"
            "            <td id='G_Mot2Min' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot2Min_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            "        <tr class='even-row' > \r\n"
            "            <td class='datum-label' > Servo 1 maximum pulse </td> \r\n"
            "            <td id='G_Mot1Max' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot1Max_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            " \r\n"
            "            <td class='datum-label' > Servo 2 maximum pulse </td> \r\n"
            "            <td id='G_Mot2Max' class='datum' > </td> \r\n"
            "            <td> \r\n"
            "                <input id='G_Mot2Max_Ovd' type='text' onkeypress='onOvd(event)' class='override' > \r\n"
            "                </input> \r\n"
            "            </td> \r\n"
            "        </tr> \r\n"
            " \r\n"
            "    </table> \r\n"
            " \r\n"
            "</body> \r\n"
            "</html> \r\n"
        ));
	// DO NOT REMOVE THIS LINE 222222222222222222222222222222
}

/* send HTTP header for plain text content
 */
void Webpage::sendPlainHeader (WiFiClient client)
{
	client.print (F(
	    "HTTP/1.0 200 OK \r\n"
	    "Content-Type: text/plain \r\n"
	    "Connection: close \r\n"
	    "\r\n"
	));
}

/* send HTTP header for html content
 */
void Webpage::sendHTMLHeader (WiFiClient client)
{
	client.print (F(
	    "HTTP/1.0 200 OK \r\n"
	    "Content-Type: text/html \r\n"
	    "Connection: close \r\n"
	    "\r\n"
	));
}

/* send empty response
 */
void Webpage::sendEmptyResponse (WiFiClient client)
{
	client.print (F(
	    "HTTP/1.0 200 OK \r\n"
	    "Content-Type: text/html \r\n"
	    "Connection: close \r\n"
	    "Content-Length: 0 \r\n"
	    "\r\n"
	));
}

/* send back error 404 when requested page not found.
 * N.B. important for chrome otherwise it keeps asking for favicon.ico
 */
void Webpage::send404Page (WiFiClient client)
{
	Serial.println ("Sending 404");
	client.print (F(
	    "HTTP/1.0 404 Not Found \r\n"
	    "Content-Type: text/html \r\n"
	    "Connection: close \r\n"
	    "\r\n"
	    "<html> \r\n"
	    "<body> \r\n"
	    "<h2>404: Not found</h2>\r\n \r\n"
	    "</body> \r\n"
	    "</html> \r\n"
	));
}

/* reboot
 */
void Webpage::reboot()
{
	resetWatchdog();
	Serial.println("rebooting");
	delay(5000);
	ESP.restart();
}
