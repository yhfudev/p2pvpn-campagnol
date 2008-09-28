/**
 * Clients representation
 * 
 * Copyright (C) 2007 Antoine Vianey
 *               2008 Florent Bondoux
 * 
 * This file is part of Campagnol.
 *
 * Campagnol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Campagnol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Campagnol.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//package Campagnol;

import java.text.DateFormat;
import java.util.*;
import java.net.*;

/**    represents the client : public address and port
 * and the peer : vpn address and vpn id */
public class ClientStruct {
    
    public static final long TIMEOUT = 5000;    // 5s
    public int port;                            // public client port
    public byte[] realIP;
    public long timeoutMillis;
    public long createTime;
    public InetSocketAddress sAddr;             // socket address
    public String vpnIPString;                  // stringified VPN IP for debuging and GUI
    public InetAddress vpnInet;                 // InetAddress containing the VPN IP
    public byte[] vpnIP;                        // client VPN IP
    
    public ClientStruct(SocketAddress sAddr, byte[] IP) {
        this.sAddr = (InetSocketAddress)sAddr;
        this.port = this.sAddr.getPort();
        this.realIP = this.sAddr.getAddress().getAddress();
        this.vpnIP = IP;
        this.vpnIPString = MsgServStruct.unMapAddress(IP);
        try {
            this.vpnInet = InetAddress.getByAddress(IP);
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }
        this.timeoutMillis = System.currentTimeMillis();
        this.createTime = this.timeoutMillis;
    }
    
    public void updateTime() {
        this.timeoutMillis = System.currentTimeMillis();
    }
    
    public boolean isTimeout() {
        return (System.currentTimeMillis() - this.timeoutMillis > ClientStruct.TIMEOUT);
    }
    
    public String getStartTime() {
        Date date = new Date(this.createTime);
        return DateFormat.getDateTimeInstance().format(date);
    }
}
