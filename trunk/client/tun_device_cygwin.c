/*
 * This file is part of Campagnol VPN.
 * Copyright (C) 2009  Florent Bondoux
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "campagnol.h"

#include <w32api/windows.h>
#include <w32api/winioctl.h>
#include "tap-win32_common.h"
#include <sys/wait.h>

#include "configuration.h"
#include "communication.h"
#include "tun_device.h"
#include "../common/log.h"
#include "../common/pthread_wrap.h"

const char *tun_default_up[] = {
        "netsh interface ip set address name=%D static %V %n none", NULL };
const char *tun_default_down[] = { NULL };

static HANDLE dev;
static int socks[2];
static char *device;
static pthread_t reader_thread;

/*
 * Look for a TAP device into the registry
 * The ID of the adapter is copied into net_cfg_instance_id.
 * 'key' is set to the corresponding registry key
 * 'devname' is the name of the adapter to look up (may be NULL)
 */
static int find_tap(char *net_cfg_instance_id, DWORD net_cfg_instance_id_len,
        HKEY *key, const char *devname) {
    HKEY key1, key2, key3;

    char iface_num[256];
    char iface_key[1024];
    char adapter_key[1024];
    char adapter_name[16384];
    char component_id[256];
    char *tap_id = config.tap_id != NULL ? config.tap_id : "tap0901";
    DWORD data_type;

    int i;
    int found = 0;
    DWORD status;
    DWORD len;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &key1)) {
        log_error_cygwin(GetLastError(), "Unable to read registry");
        return -1;
    }

    // List the adapters
    for (i = 0;; i++) {
        len = sizeof(iface_num);
        if (RegEnumKeyEx(key1, i, iface_num, &len, NULL, NULL, NULL, NULL))
            break;

        snprintf(iface_key, sizeof(iface_key), "%s\\%s", ADAPTER_KEY, iface_num);

        // Open this adapter's key
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, iface_key, 0, KEY_READ
                | KEY_SET_VALUE, &key2)) {
            log_error_cygwin(GetLastError(), "Unable to read registry");
            continue;
        }

        // Search for a 'ComponentId' value
        len = sizeof(component_id);
        status = RegQueryValueEx(key2, "ComponentId", NULL, &data_type,
                (unsigned char *) component_id, &len);
        if (status != ERROR_SUCCESS || data_type != REG_SZ) {
            RegCloseKey(key2);
            continue;
        }

        // Search for a 'NetCfgInstanceId' value
        len = net_cfg_instance_id_len;
        status = RegQueryValueEx(key2, "NetCfgInstanceId", NULL, &data_type,
                (unsigned char *) net_cfg_instance_id, &len);
        if (status != ERROR_SUCCESS || data_type != REG_SZ) {
            RegCloseKey(key2);
            continue;
        }

        // If this is a TAP driver, get and check the adapter's name
        if (strcmp(component_id, tap_id) == 0) {
            snprintf(adapter_key, sizeof(adapter_key), "%s\\%s\\Connection",
                    NETWORK_CONNECTIONS_KEY, net_cfg_instance_id);

            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, adapter_key, 0, KEY_READ,
                    &key3)) {
                log_error_cygwin(GetLastError(), "Unable to read registry");
            }
            else {
                len = sizeof(adapter_name);
                status = RegQueryValueEx(key3, "Name", NULL, &data_type,
                        (unsigned char *) adapter_name, &len);
                if (status == ERROR_SUCCESS && data_type == REG_SZ) {
                    if (devname == NULL || strcmp(devname, adapter_name) == 0) {
                        device = CHECK_ALLOC_FATAL(strdup(adapter_name));
                        found = 1;
                    }
                }
                RegCloseKey(key3);
            }
        }

        if (found) {
            *key = key2;
            RegCloseKey(key1);
            return 0;
        }
        else {
            RegCloseKey(key2);
        }
    }

    RegCloseKey(key1);

    return -1;
}

/*
 * Read the device's file and write the packets into socks[1]
 */
static void * thread_read(void *arg __attribute__((unused))) {
    char *buf = CHECK_ALLOC_FATAL(malloc(config.tun_mtu));
    DWORD r, status = 0;
    OVERLAPPED overlapped;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // We're ready
    buf[0] = 1;
    write(socks[1], buf, 1);

    while (!end_campagnol) {
        ReadFile(dev, buf, config.tun_mtu, &r, &overlapped);
        while ((!end_campagnol) && (status = WaitForSingleObject(
                overlapped.hEvent, 500)) == WAIT_TIMEOUT) {
        }
        if (status == WAIT_OBJECT_0) {
            GetOverlappedResult(overlapped.hEvent, &overlapped, &r, TRUE);
            write(socks[1], buf, r);
        }
        else if (status == WAIT_FAILED) {
            log_error_cygwin(GetLastError(),
                    "Error while waiting for events on the TUN/TAP device");
        }
    }
    CancelIo(dev);
    free(buf);

    return NULL;
}

int init_tun() {
    HKEY key;
    char adapterid[256];
    char tapname[256];
    char mtu[16];
    in_addr_t ep[3];
    DWORD len;
    ULONG status;

    if (config.verbose)
        printf("TUN interface initialization\n");

    if (find_tap(adapterid, sizeof(adapterid), &key, config.tun_device) != 0) {
        log_error(-1, "Unable to find a TUN/TAP device");
        return -1;
    }

    // Set the MTU
    snprintf(mtu, sizeof(mtu), "%d", config.tun_mtu);
    if (RegSetValueEx(key, "MTU", 0, REG_SZ, (unsigned char*) mtu, sizeof(mtu))) {
        log_error_cygwin(GetLastError(),
                "Unable to set the MTU of the TUN/TAP device");
        RegCloseKey(key);
        return -1;
    }
    RegCloseKey(key);

    /*
     * This idea is from Tinc
     * We can't use select on the Win32 HANDLE and I don't want to "pollute" the
     * code with Win32 calls. So, at least for now, the packets are copied into
     * a unix sockets pair and socks[0] is used as a file descriptor for reading
     * the TUN device.
     * This may change latter.
     */
    if (socketpair(AF_UNIX, SOCK_DGRAM, PF_UNIX, socks)) {
        log_error(errno, "Failed to call socketpair");
        return -1;
    }

    // Open the device's file for asynchronous operations
    snprintf(tapname, sizeof(tapname), USERMODEDEVICEDIR "%s" TAPSUFFIX,
            adapterid);
    dev = CreateFile(tapname, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ
            | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM
            | FILE_FLAG_OVERLAPPED, 0);
    if (dev == INVALID_HANDLE_VALUE) {
        log_error_cygwin(GetLastError(),
                "Unable to open the TUN/TAP device for writing");
        close(socks[0]);
        close(socks[1]);
        return -1;
    }

    // Set up the TUN mode
    ep[0] = config.vpnIP.s_addr;
    ep[1] = config.vpnIP.s_addr & config.vpnNetmask.s_addr;
    ep[2] = config.vpnNetmask.s_addr;
    if (!DeviceIoControl(dev, TAP_IOCTL_CONFIG_TUN, ep, sizeof(ep), ep,
            sizeof(ep), &len, NULL)) {
        log_error_cygwin(GetLastError(),
                "Error while setting up the TUN device");
        close(socks[0]);
        close(socks[1]);
        CloseHandle(dev);
        return -1;
    }

    // Set the adapter to appear as being "connected"
    status = 1;
    if (!DeviceIoControl(dev, TAP_IOCTL_SET_MEDIA_STATUS, &status,
            sizeof(status), &status, sizeof(status), &len, NULL)) {
        log_error_cygwin(GetLastError(),
                "Error while setting up the TUN device");
        close(socks[0]);
        close(socks[1]);
        CloseHandle(dev);
        return -1;
    }

    if (config.verbose)
        printf("TUN interface configuration (%s MTU %d)\n", device,
                config.tun_mtu);
    exec_up(device);

    reader_thread = createThread(thread_read, NULL);
    char test;
    read(socks[0], &test, 1);

    return socks[0];
}

int close_tun(int fd __attribute__((unused))) {
    joinThread(reader_thread, NULL);
    exec_down(device);
    close(socks[0]);
    close(socks[1]);
    CloseHandle(dev);
    free(device);
    return 0;
}

ssize_t read_tun(int fd __attribute__((unused)), void *buf, size_t count) {
    return read(socks[0], buf, count);
}

ssize_t write_tun(int fd __attribute__((unused)), void *buf, size_t count) {
    OVERLAPPED overlapped;
    DWORD len;

    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    WriteFile(dev, buf, count, &len, &overlapped);
    GetOverlappedResult(overlapped.hEvent, &overlapped, &len, TRUE);
    return (ssize_t) len;
}
