/*
 DMXOutputUsingUSART.ino

 This sketch demonstrates converting Art-Net or sACN packets recieved using an
 Arduino Ethernet Shield to DMX output using serial output to a driver chip.

 This sketch requires software such as LXConsole, QLC+ or DMXWorkshop
 that can send Art-Net or sACN packets to the Arduino.

  The sketch receives incoming UDP packets.  If a received packet is
  Art-Net/sACN DMX output, it uses the AVR microcontroller's USART to output
  the dmx levels as serial data.  A driver chip is used
  to convert this output to a differential signal (actual DMX).

  This is the circuit for a simple unisolated DMX Shield:

 Arduino                    SN 75176 A or MAX 481CPA
 pin                      _______________
 |                        | 1      Vcc 8 |------ +5v
 V                        |              |                 DMX Output
   |                 +----| 2        B 7 |---------------- Pin 2
   |                 |    |              |
2  |----------------------| 3 DE     A 6 |---------------- Pin 3
   |                      |              |
TX |----------------------| 4 DI   Gnd 5 |---+------------ Pin 1
   |                                         |
   |                                        GND
 5 |--------[ 330 ohm ]---[ LED ]------------|



 Created January 7th, 2014 by Claude Heintz
 Current version 1.5
 (see bottom of file for revision history)

 See LXArduinoDMXUSART.h or http://lx.claudeheintzdesign.com/opensource.html for license.
 Art-Net(tm) Designed by and Copyright Artistic Licence Holdings Ltd.

 */

//*********************** includes ***********************

#include "LXArduinoDMXUSART.h"

#include <SPI.h>

/******** Important note about Ethernet library ********
   There are various ethernet shields that use differnt Wiznet chips w5100, w5200, w5500
   It is necessary to use an Ethernet library that supports the correct chip for your shield
   Perhaps the best library is the one by Paul Stoffregen which supports all three chips:
   
   https://github.com/PaulStoffregen/Ethernet

   The Paul Stoffregen version is much faster than the built-in Ethernet library and is
   neccessary if the shield receives more than a single universe of packets.
   
   The Ethernet Shield v2 uses a w5500 chip and will not work with the built-in Ethernet library
   The library manager does have an Ethernet2 library which supports the w5500.  To use this,
   uncomment the line to define ETHERNET_SHIELD_V2
*/
#if defined ( ETHERNET_SHIELD_V2 )
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#else
#include <Ethernet.h>
#include <EthernetUdp.h>
#endif


#include <LXDMXEthernet.h>
#include <LXArtNet.h>
#include <LXSACN.h>

//*********************** defines ***********************

/*
   Enter a MAC address and IP address for your controller below.
   The MAC address can be random as is the one shown, but should
   not match any other MAC address on your network.
   
   If BROADCAST_IP is not defined, ArtPollReply will be sent directly to server
   rather than being broadcast.  (If a server' socket is bound to a specific network
   interface ip address, it will not receive broadcast packets.)
*/

#define USE_DHCP 1
#define USE_SACN 0

//  Uncomment to use multicast, which requires extended Ethernet library
//  see note in LXDMXEthernet.h file about method added to library

//#define USE_MULTICAST 1

#define MAC_ADDRESS 0x90, 0xA2, 0xDA, 0x10, 0x6C, 0xA8
#define IP_ADDRESS 192,168,1,20
#define GATEWAY_IP 192,168,1,1
#define SUBNET_MASK 255,255,255,0
#define BROADCAST_IP 192,168,1,255

// this sketch flashes an indicator led:
#define MONITOR_PIN 5

// the driver direction is controlled by:
#define RXTX_PIN 2

//the Ethernet Shield has an SD card that also communicates by SPI
//set its select pin to output to be safe:
#define SDSELECT_PIN 4


//*********************** globals ***********************

//network addresses
byte mac[] = {  MAC_ADDRESS };
IPAddress ip(IP_ADDRESS);
IPAddress gateway(GATEWAY_IP);
IPAddress subnet_mask(SUBNET_MASK);

#if defined( BROADCAST_IP )
IPAddress broadcast_ip( BROADCAST_IP);
#else
IPAddress broadcast_ip = INADDR_NONE;
#endif

// buffer
unsigned char packetBuffer[ARTNET_BUFFER_MAX];

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP eUDP;

// LXDMXEthernet instance ( created in setup so its possible to get IP if DHCP is used )
LXDMXEthernet* interface;

// sACN uses multicast, Art-Net uses broadcast. Both can be set to unicast (use_multicast = 0)
#if defined ( USE_MULTICAST )
uint8_t use_multicast = USE_SACN;
#else
uint8_t use_multicast = 0;
#endif

//  used to toggle on and off the LED when DMX is Received
int monitorstate = LOW;


/* setup initializes Ethernet and opens the UDP port
   it also sends an Art-Net Poll Reply packet telling other
   Art-Net devices that it can transmit DMX from the network  */

//  ***** blinkLED  *******************************************
//
//  toggles the monitor LED on and off as an indicator

void blinkLED() {
  if ( monitorstate == LOW ) {
    monitorstate = HIGH;
  } else {
    monitorstate = LOW;
  }
  digitalWrite(MONITOR_PIN, monitorstate);
}

//  ***** setup  *******************************************
//
//  initializes Ethernet and opens the UDP port
//  it also sends an Art-Net Poll Reply packet telling other
//  Art-Net devices that it can transmit DMX from the network

void setup() {
  pinMode(MONITOR_PIN, OUTPUT);  //status LED
  blinkLED();
  #if defined(SDSELECT_PIN)
    pinMode(SDSELECT_PIN, OUTPUT);
  #endif

  if ( USE_DHCP ) {                                          // Initialize Ethernet
    Ethernet.begin(mac);                                     // DHCP
  } else {
  	Ethernet.begin(mac, ip, gateway, gateway, subnet_mask);   // Static
  }
  
  if ( USE_SACN ) {                       // Initialize Interface (defaults to first universe)
    interface = new LXSACN();
    //interface->setUniverse(1);	         // for different universe, change this line and the multicast address below
  } else {
    interface = new LXArtNet(Ethernet.localIP(), Ethernet.subnetMask());
    use_multicast = 0;
    //((LXArtNet*)interface)->setSubnetUniverse(0, 0);  //for different subnet/universe, change this line
  }
  
  if ( use_multicast ) {                  // Start listening for UDP on port
  #if defined ( USE_MULTICAST )
    eUDP.beginMulticast(IPAddress(239,255,0,1), interface->dmxPort());
  #endif
  } else {
    eUDP.begin(interface->dmxPort());
  }

  LXSerialDMX.setDirectionPin(RXTX_PIN);
  LXSerialDMX.startOutput();
  
  if ( ! USE_SACN ) {
   ((LXArtNet*)interface)->setNodeName("ArtNet2USART");
  	((LXArtNet*)interface)->send_art_poll_reply(&eUDP);
  }
  blinkLED();
}


/************************************************************************

  The main loop checks for and reads packets from UDP ethernet socket
  connection.  When a packet is recieved, it is checked to see if
  it is valid Art-Net and the art DMXReceived function is called, sending
  the DMX values to the output.

*************************************************************************/

void loop() {
	if ( USE_DHCP ) {
		uint8_t dhcpr = Ethernet.maintain();
		if (( dhcpr == 4 ) || (dhcpr == 2)) {	//renew/rebind success
			if ( ! USE_SACN ) {
				((LXArtNet*)interface)->setLocalIP(Ethernet.localIP(), Ethernet.subnetMask());
			}
		}
	}

	uint8_t result = interface->readDMXPacket(&eUDP);

  if ( result == RESULT_DMX_RECEIVED ) {
     for (int i = 1; i <= interface->numberOfSlots(); i++) {
        LXSerialDMX.setSlot(i , interface->getSlot(i));
     }
     blinkLED();
  }
}

/*
    Revision History
    v1.0 initial release January 2014
    v1.1 added monitor LED to code and circuit
    v1.2 8/14/15 changed library support and clarified code
    v1.3 moved control of options to "includes" and "defines"
    v1.4 uses LXArtNet class which encapsulates Art-Net functionality
    v1.5 revised as example file for library
*/
