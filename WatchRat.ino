// Many thanks to author of https://github.com/jcw/ethercard/  !!! Long Life!!!
// 2011-06-12 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php


#include <EtherCard.h>

#define DEAD_HAND_TIMEOUT  50000000
#define TURNON_TIMEOUT  5000
#define BOOT_TIMEOUT  250000000
#define SYSLOG_PORT 514

#define RELAY_ON 0
#define RELAY_OFF 1
#define Relay_1 4
#define Relay_2 5

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, 0x31 };

byte Ethernet::buffer[700];
Stash stash;
static uint32_t timer;
static uint32_t DeadTimerForFirstRelayServer;
static bool firstServerBooting = false;


static byte myip[] = {10, 254, 3, 254};
static byte netmask[] = {255, 255, 255, 255};
static byte gateway[] = {10, 254, 3, 100};
static byte firstDns[] = {10, 254, 3, 100};

static byte firstRelayServer[] = {10, 254, 3, 100};

static byte syslogServer[] = {10, 254, 3, 200};
static byte syslogSession;

//timing variable
int res = 100; // was 0

// called when a ping comes in (replies to it are automatic)
static void gotPinged (byte* ptr) {
  ether.printIp(">>> ping from: ", ptr);
}

void setup () {
  Serial.begin(115200);

  initialize_ethernet();
  digitalWrite(Relay_1, RELAY_OFF);
  pinMode(Relay_1, OUTPUT);
  digitalWrite(Relay_2, RELAY_OFF);
  pinMode(Relay_2, OUTPUT);

  timer = -9999999; // start timing out right away
  DeadTimerForFirstRelayServer = micros();
}

void loop () {

  word len = ether.packetReceive(); // go receive new packets
  word pos = ether.packetLoop(len); // respond to incoming pings

  const char* reply = ether.tcpReply(syslogSession);
  if (reply != 0) {
    Serial.println("Got a response!");
    Serial.println(reply);
  }


  if (!firstServerBooting) {
    // ping a remote server once every few seconds
    if (micros() - timer >= 5000000) {
      ether.copyIp(ether.hisip, firstRelayServer);
      ether.printIp("Pinging: ", ether.hisip);
      timer = micros();
      ether.clientIcmpRequest(ether.hisip);

    }
    // report whenever a reply to our outgoing ping comes back
    if (len > 0 && ether.packetLoopIcmpCheckReply(ether.hisip)) {
      DeadTimerForFirstRelayServer = micros();
      Serial.print("  ");
      Serial.print((micros() - timer) * 0.001, 3);
      Serial.println(" ms");
    }
    if (DEAD_HAND_TIMEOUT < (micros() - DeadTimerForFirstRelayServer)) {
      Serial.println("Lol, first server died. Time to click-click relay #1!!! Send to syslog");
      firstServerBooting = true;
      //---*syslog*---
      Stash::prepare(PSTR("<000> First server didn't answer on ping. Try to powerOn." "\r\n"));
      ether.copyIp(ether.hisip, syslogServer);
      ether.hisport = SYSLOG_PORT;
      syslogSession = ether.tcpSend();
      //---*syslog*---

      digitalWrite(Relay_1, RELAY_ON);// set the Relay ON
      delay(TURNON_TIMEOUT); // wait for powerup
      digitalWrite(Relay_1, RELAY_OFF);// set the Relay ON
      DeadTimerForFirstRelayServer = micros();
    }
  } else {
    if (BOOT_TIMEOUT < (micros() - DeadTimerForFirstRelayServer) ) {
      firstServerBooting = false;
      DeadTimerForFirstRelayServer = micros();
    }
  }
}


void initialize_ethernet(void) {
  for (;;) { // keep trying until you succeed
    //Reinitialize ethernet module
    //pinMode(5, OUTPUT);  // do notknow what this is for, i ve got something elso on pin5
    //Serial.println("Reseting Ethernet...");
    //digitalWrite(5, LOW);
    //delay(1000);
    //digitalWrite(5, HIGH);
    //delay(500);

    if (ether.begin(sizeof Ethernet::buffer, mymac) == 0)
      Serial.println(F("Failed to access Ethernet controller."));

    if (!ether.staticSetup(myip, gateway, firstDns, netmask))
      Serial.println(F("Failed to setup IP."));

    ether.printIp("IP:  ", ether.myip);
    ether.printIp("GW:  ", ether.gwip);
    ether.printIp("DNS: ", ether.dnsip);

    // call this to report others pinging us
    ether.registerPingCallback(gotPinged);

    //reset init value
    res = 180;
    break;
  }
}
