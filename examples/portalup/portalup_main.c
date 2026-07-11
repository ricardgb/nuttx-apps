/****************************************************************************
 * apps/examples/portalup/portalup_main.c
 *
 * Boot helper for the Pico 2 W access device.  Spawned from board bringup:
 *
 *   - connects the CDC-ACM console + CDC-NCM composite USB device;
 *   - brings up the WiFi station (wlan0) that netinit associates + DHCPs but
 *     leaves DOWN;
 *   - brings up the USB NIC (eth0) with a static IP and a DHCP server so a
 *     host plugged into USB gets an address;
 *   - starts the mDNS responder so the board is reachable by name.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/boardctl.h>

#include "netutils/netlib.h"
#include "netutils/dhcpd.h"

#define WLAN_IFNAME  "wlan0"
#define ETH_IFNAME   "eth0"
#define ETH_IPADDR   "192.168.7.1"
#define ETH_NETMASK  "255.255.255.0"

int main(int argc, FAR char *argv[])
{
  struct in_addr addr;
  int i;

#ifdef CONFIG_USBDEV_COMPOSITE
  /* Connect the CDC-ACM console + CDC-NCM composite.  This registers eth0
   * (the USB NIC) and, once the host opens it, /dev/ttyACM0 (the console).
   */

  {
    struct boardioc_usbdev_ctrl_s ctrl;
    FAR void *handle;

    ctrl.usbdev   = BOARDIOC_USBDEV_COMPOSITE;
    ctrl.action   = BOARDIOC_USBDEV_CONNECT;
    ctrl.instance = 0;
    ctrl.config   = 0;
    ctrl.handle   = &handle;

    if (boardctl(BOARDIOC_USBDEV_CONTROL, (uintptr_t)&ctrl) < 0)
      {
        printf("portalup: composite connect failed\n");
      }

    sleep(1);   /* let eth0 register before configuring it */
  }

  /* USB NIC: static IP + DHCP server for the plugged-in host. */

  addr.s_addr = inet_addr(ETH_IPADDR);
  netlib_set_ipv4addr(ETH_IFNAME, &addr);
  addr.s_addr = inet_addr(ETH_NETMASK);
  netlib_set_ipv4netmask(ETH_IFNAME, &addr);
  netlib_ifup(ETH_IFNAME);

#ifdef CONFIG_NETUTILS_DHCPD
  if (dhcpd_start(ETH_IFNAME) < 0)
    {
      printf("portalup: dhcpd_start(%s) failed\n", ETH_IFNAME);
    }
#endif

  printf("portalup: %s (USB NIC) up at %s/24 + dhcpd\n",
         ETH_IFNAME, ETH_IPADDR);
#endif /* CONFIG_USBDEV_COMPOSITE */

  /* netinit associates wlan0 and gets a DHCP lease, but the interface can
   * come up DOWN; bring it up (retry until it has an address).
   */

  for (i = 0; i < 30; i++)
    {
      netlib_ifup(WLAN_IFNAME);

      addr.s_addr = 0;
      netlib_get_ipv4addr(WLAN_IFNAME, &addr);
      if (addr.s_addr != 0)
        {
          printf("portalup: %s up at %s\n", WLAN_IFNAME, inet_ntoa(addr));
          break;
        }

      sleep(1);
    }

#ifdef CONFIG_EXAMPLES_MDNSD
  /* Advertise over mDNS.  The interfaces already have IPs (above), so start
   * the responder directly rather than waiting on a netlink event.
   */

  {
    pid_t pid;
    FAR char *av[] = { "mdnsd_start", NULL };

    if (posix_spawn(&pid, "mdnsd_start", NULL, NULL, av, NULL) != 0)
      {
        printf("portalup: failed to spawn mdnsd_start\n");
      }
  }
#endif

  return 0;
}
