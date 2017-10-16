// Expose Espressif SDK functionality
extern "C" {
#include "user_interface.h"
}

#include "wifi_structures.h"

uint8_t wifi_snoop_mac[ETH_MAC_LEN];
int rssi_total;
int packet_count;

void get_client_rssi_cb(uint8_t *buf, uint16_t len)
{
  if (len == 12) {
    // unknown
  } else if (len == 128) {
    // beacon broadcast - ignore
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    // NOTE: Doesn't seem to handle stuff when we have really quick networking from eg laptops

    // Is data packet (wireshark filter is wlan.fc.type_subtype & 0x0020 because wifi spec writes the packet bits in the opposite order)
    if ((sniffer->buf[0] & 0x08) == 0x08) {
      struct clientinfo ci = parse_data(sniffer->buf);

      // if it is not the base station broadcasting and the transmitter matches the one we were trying to catch
      if ( memcmp( ci.bssid, ci.transmitter, ETH_MAC_LEN ) && !memcmp( ci.transmitter, wifi_snoop_mac, ETH_MAC_LEN ) ) {
        rssi_total += sniffer->rx_ctrl.rssi;
        packet_count++;
      }
    }
  }
}

// Runs a scan for rssi for all packets from the specific transmission point
// over a certain period. Returns a float of the mean rssi over that time; 1
// if there was a problem with input, 0 if there were not enough packets
// detected.
float run_scan_for_rssi(char *mac_hex, uint16_t seconds)
{
    // bssid/channel probably get lost on disconnect so save them here.
    int32_t channel = WiFi.channel();

    // Convert a hex mac address like ab:CD:6E:DE:FF:FF to 6 bytes raw format
    uint8_t mac_byte;
    for( mac_byte = 0; mac_byte < ETH_MAC_LEN; mac_byte++ )
        wifi_snoop_mac[mac_byte] = 0;

    mac_byte = 0;
    uint8_t mac_high = 1;
    for( uint16_t i = 0; mac_hex[i] > 0; i++ ) {
        if( mac_byte > ETH_MAC_LEN )
            return 1;

        uint8_t cur_byte;
        if( mac_hex[i] >= '0' && mac_hex[i] <= '9' )
            cur_byte = (uint8_t)(mac_hex[i] - '0');
        else if( mac_hex[i] >= 'a' && mac_hex[i] <= 'f' )
            cur_byte = (uint8_t)(mac_hex[i] - 'a' + 10);
        else if( mac_hex[i] >= 'A' && mac_hex[i] <= 'F' )
            cur_byte = (uint8_t)(mac_hex[i] - 'A' + 10);
        else
            continue;

        if( mac_high == 1 ) {
            wifi_snoop_mac[mac_byte] = cur_byte << 4;
            mac_high = 0;
        } else {
            wifi_snoop_mac[mac_byte] |= cur_byte;
            mac_high = 1;
            mac_byte++;
        }
    }
    if( mac_byte != ETH_MAC_LEN )
        return 1;

    // Cache MQTT state as this will be disconnected
    uint8_t cached_fallbactopic = fallbacktopic;
    fallbacktopic = 0;

    packet_count = 0;
    rssi_total = 0;

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);      // Disable AP mode
    wifi_set_channel(channel);

    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(get_client_rssi_cb);
    wifi_promiscuous_enable(1);
    delay(seconds * 1000);    // run scan in background for X seconds
    wifi_promiscuous_enable(0);

    // Reconnect to normal wifi
    WIFI_begin(0);

    // Wait to reconnect so mqtt message goes back correctly...
    uint8_t tries = 50;
    wificheckflag = WIFI_RESTART;
    while (WL_CONNECTED != WiFi.status()) {
        delay(100);
        if( tries-- == 0 )
            break;
    }
    mqtt_reconnect();
    fallbacktopic = cached_fallbactopic;

    return packet_count > 2 ? (float)rssi_total / (float)packet_count : 0;
}
