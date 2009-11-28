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

const char *tun_default_up[] = {
        "netsh interface ip set address name=%D static %V %n none", NULL };
const char *tun_default_down[] = { NULL };

static HANDLE dev;
static char *device;
static OVERLAPPED overlapped_read;
static OVERLAPPED overlapped_write;
static int pending_read = 0;

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
    const char *tap_id = config.tap_id != NULL ? config.tap_id : "tap0901";
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

int init_tun() {
    HKEY key;
    char adapterid[256];
    char tapname[256];
    char mtu[16];
    in_addr_t ep[3];
    DWORD len;
    ULONG status;

    log_message_level(1, "TUN interface initialization");

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

    // Open the device's file for asynchronous operations
    snprintf(tapname, sizeof(tapname), USERMODEDEVICEDIR "%s" TAPSUFFIX,
            adapterid);
    dev = CreateFile(tapname, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ
            | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM
            | FILE_FLAG_OVERLAPPED, 0);
    if (dev == INVALID_HANDLE_VALUE) {
        log_error_cygwin(GetLastError(),
                "Unable to open the TUN/TAP device for writing");
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
        CloseHandle(dev);
        return -1;
    }

    // Set the adapter to appear as being "connected"
    status = 1;
    if (!DeviceIoControl(dev, TAP_IOCTL_SET_MEDIA_STATUS, &status,
            sizeof(status), &status, sizeof(status), &len, NULL)) {
        log_error_cygwin(GetLastError(),
                "Error while setting up the TUN device");
        CloseHandle(dev);
        return -1;
    }

    log_message_level(1, "TUN interface configuration (%s MTU %d)", device,
                config.tun_mtu);
    exec_up(device);

    overlapped_read.Offset = 0;
    overlapped_read.OffsetHigh = 0;
    overlapped_read.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    overlapped_write.Offset = 0;
    overlapped_write.OffsetHigh = 0;
    overlapped_write.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    return 1;
}

int close_tun(int fd __attribute__((unused))) {
    exec_down(device);
    CloseHandle(overlapped_read.hEvent);
    CloseHandle(overlapped_write.hEvent);
    CloseHandle(dev);
    free(device);
    return 0;
}

/* Set up an asynchronous read and wait at most 'ms' miliseconds
 * return 1 if the read is complete,
 * 0 if the wait timed out
 * -1 in case of error
 */
int read_tun_wait(void *buf, size_t count, unsigned long int ms) {
    DWORD status;

    if (!pending_read) {
        ReadFile(dev, buf, count, NULL, &overlapped_read);
        pending_read = 1;
    }

    status = WaitForSingleObject(overlapped_read.hEvent, ms);
    if (status == WAIT_OBJECT_0)
        return 1;
    else if (status == WAIT_TIMEOUT)
        return 0;
    else if (status == WAIT_FAILED) {
        log_error_cygwin(GetLastError(),
                "Error while waiting for events on the TUN/TAP device");
        return -1;
    }
    return -1;
}

/*
 * Finalize the asynchronous read and return the number of bytes read
 */
ssize_t read_tun_finalize() {
    DWORD len;
    GetOverlappedResult(overlapped_read.hEvent, &overlapped_read, &len, TRUE);
    pending_read = 0;
    return (ssize_t) len;
}

/*
 * Cancel a pending IO
 */
void read_tun_cancel() {
    if (pending_read) {
        CancelIo(dev);
        pending_read = 0;
    }
}

ssize_t write_tun(void *buf, size_t count) {
    DWORD len;
    WriteFile(dev, buf, count, NULL, &overlapped_write);
    GetOverlappedResult(overlapped_write.hEvent, &overlapped_write, &len, TRUE);
    return (ssize_t) len;
}
