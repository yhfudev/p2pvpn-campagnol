/**
 * UDP messages creation and reading
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

import java.net.UnknownHostException;

public class MsgServStruct {
    
    /**    different types of messages */
    public static final byte HELLO = 0;                                         // registered to server
    public static final byte PING = 1;                                          // keep alive
    public static final byte ASK_CONNECTION = 2;                                // ask for a peer connection
    public static final byte PONG = 3;                                          // keep alive answer
    public static final byte OK = 4;                                            // ok message
    public static final byte NOK = 5;                                           // not ok message
    public static final byte FWD_CONNECTION = 6;                                // forward a connection request
    public static final byte ANS_CONNECTION = 7;                                // answer to a connection request
    public static final byte REJ_CONNECTION = 8;                                // connection request rejected
    public static final byte PUNCH = 9;                                         // punch message
    public static final byte PUNCH_ACK = 10;                                    // punch ack message
    public static final byte BYE = 11;                                          // leave VPN
    public static final byte RECONNECT = 12;                                    // ask for registration
    public static final byte CLOSE_CONNECTION = 13;                             // close session
    /** pre formated messages */
    public static final byte[] MSG_OK = {OK,0,0,0,0,0,0,0,0,0,0};               // ok message
    public static final byte[] MSG_NOK = {NOK,0,0,0,0,0,0,0,0,0,0};             // nok message
    public static final byte[] MSG_PING = {PING,0,0,0,0,0,0,0,0,0,0};           // ping message
    public static final byte[] MSG_PONG = {PONG,0,0,0,0,0,0,0,0,0,0};           // pong msg
    public static final byte[] MSG_PUNCH = {PUNCH,0,0,0,0,0,0,0,0,0,0};         // punch msg
    public static final byte[] MSG_RECONNECT = {RECONNECT,0,0,0,0,0,0,0,0,0,0}; // reconnect message
    /** constants */
    public static final int MSG_LENGTH = 11;
    
    /** content of the message */
    public byte type;                                                           // place for the message type
    public int port;                                                            // port number
    public byte[] ip1 = new byte[4];                                            // IPv4 address
    public byte[] ip2 = new byte[4];                                            // IPv4 address
    
    public MsgServStruct() {}
    public MsgServStruct(byte type) {this.type = type;}
    
    /** return a String representation of a byte array IP */
    public static String unMapAddress(byte[] address) {
        if (address == null) return null;
        try {
            return java.net.Inet4Address.getByAddress(address).getHostAddress();
        } catch (UnknownHostException e) {
            e.printStackTrace();
            return null;
        }
    }
    
    /**    return the msgServStruct from the packet's byte array */
    public static MsgServStruct get(byte[] data) {
        if (data.length<MSG_LENGTH) return null;
        MsgServStruct message = new MsgServStruct();
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(data);
        message.type = bb.get();
        message.port = bb.getShort();
        bb.get(message.ip1);
        bb.get(message.ip2);
        if (CampagnolServer.debug) System.out.println(message);
        return message;
    }
    
    /** return the byte array corresponding to the MsgServStruct */
    public static byte[] set(MsgServStruct message) {
        byte[] array = new byte[MSG_LENGTH];
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(array);
        bb.put(message.type);
        bb.putShort((short) message.port);
        bb.put(message.ip1);
        bb.put(message.ip2);
        if (CampagnolServer.debug) System.out.println(message);
        return array;
    }
    
    public String toString() {
        String s = "*****************************************************\n| TYPE  | PORT  | IP 1            | IP 2            |\n";
        String a="", c, d, e;
        switch (this.type) {
            case HELLO :
                a = "| HELLO ";
                break;
            case PING :
                a = "| PING  ";
                break;
            case ASK_CONNECTION :
                a = "| ASK   ";
                break;
            case PONG :
                a = "| PONG  ";
                break;
            case OK :
                a = "| OK    ";
                break;
            case NOK :
                a = "| NOK   ";
                break;
            case FWD_CONNECTION :
                a = "| FWD   ";
                break;
            case ANS_CONNECTION :
                a = "| ANS   ";
                break;
            case REJ_CONNECTION :
                a = "| REJ   ";
                break;
            case PUNCH :
                a = "| PUNCH ";
                break;
            case PUNCH_ACK :
                a = "| PEER+ ";
                break;
            case BYE :
                a = "| BYE   ";
                break;
            case RECONNECT :
                a = "| RECNCT";
                break;
            case CLOSE_CONNECTION :
                a = "| CLOSE ";
                break;
            default :
                break;
        }
        c = "| "+this.port;
        while (c.length()<8) c+=" ";
        d = "| "+unMapAddress(this.ip1);
        while (d.length()<18) d+=" ";
        e = "| "+unMapAddress(this.ip2);
        while (e.length()<18) e+=" ";
        return (s+a+c+d+e+"|\n*****************************************************");
    }
    
}
