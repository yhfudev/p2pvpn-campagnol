/**
 * Rendez-vous server, main class
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

import java.util.*;
import java.net.*;

/**	class in charge of starting up the server
 * and of all operation on the client objects */
public class CampagnolServer {
    
    /** constants */
    final int PORT_NUMBER = 57888;
    final int BUFFSIZE = MsgServStruct.MSG_LENGTH;
    /**	time to wait before forwarding the connection request to the peer
     * use to connect in a second try if one peer is behind a symmetric NAT */
    final long WAIT_MILLIS_FWD = 50;
    /** if we alternate the hole punching sequences (the peer who request start first and
     * if connection cannot be established then the peer requested start first)
     * consider the connection is impossible after this number of tries */
    final int MAX_CONNECTION_TRIES = 4;
    /** local variables */
    private DatagramSocket socket;
    private DatagramPacket packet;
    private byte[] buff;
    private int num_message = 0;
    /** Vector containing the client's ClientStruct */
    private Vector clients;
    /** Vector containing the Connection instances*/
    private Vector connections;
    private CampagnolGUI gui = null;
    
    /** Get a client from it's stringified VPN IP  */
    private ClientStruct getClientFromId(String vpnip) {
        Iterator it = clients.iterator();
        while (it.hasNext()) {
            ClientStruct cl = (ClientStruct) it.next();
            if (cl.vpnIPString.equals(vpnip)) {
                return cl;
            }
        }
        return null;
    }
    
    /** Get a client from it's real address */
    private ClientStruct getClientFromIp(SocketAddress ad) {
        Iterator it = clients.iterator();
        while (it.hasNext()) {
            ClientStruct cl = (ClientStruct) it.next();
            if (cl.sAddr.equals(ad)) {
                return cl;
            }
        }
        return null;
    }
    
    /** Get the connection started by the first IP */
    private Connection getConnection(String vpnIP1, String vpnIP2) {
        Iterator it = connections.iterator();
        while (it.hasNext()) {
            Connection ct = (Connection) it.next();
            if (ct.client1.vpnIPString.equals(vpnIP1) && ct.client2.vpnIPString.equals(vpnIP2)) {
                return ct;
            }
        }
        return null;
    }
    
    /** Remove all connections involving vpnIP */
    private void removeConnectionsWithClient(String vpnIP) {
        Object[] cxs = connections.toArray();
        for (int i=0; i<cxs.length; i++) {
            Connection ct = (Connection) cxs[i];
            if (ct.client1.vpnIPString.equals(vpnIP) || ct.client2.vpnIPString.equals(vpnIP)) {
                connections.remove(ct);
            }
        }
    }
    
    /** Remove the given connection (or reverse connection) */
    private void removeConnection(String vpnIP1, String vpnIP2) {
        Object[] cxs = connections.toArray();
        for (int i=0; i<cxs.length; i++) {
            Connection ct = (Connection) cxs[i];
            if (ct.client1.vpnIPString.equals(vpnIP1) && ct.client2.vpnIPString.equals(vpnIP2)
                || ct.client1.vpnIPString.equals(vpnIP2) && ct.client2.vpnIPString.equals(vpnIP1)) {
                connections.remove(ct);
            }
        }
    }
    
    private void updateViews() {
        if (this.gui != null) {
            this.gui.update();
        }
    }
    
    private CampagnolServer(boolean withGui) {
        /**	instantiating the datagram socket */
        try {
            this.socket = new DatagramSocket(PORT_NUMBER);
            this.socket.setSoTimeout(5000);
            this.buff = new byte[BUFFSIZE];
            this.packet = new DatagramPacket(buff, BUFFSIZE);
            this.clients = new Vector();
            this.connections = new Vector();
            if (withGui) {
                this.gui = new CampagnolGUI(this.clients, this.connections);
                this.gui.setVisible(true);
            }
        } catch (SocketException sex) {
            System.out.println("Server shut down : "+sex.toString());
            System.exit(0);
        }
        System.out.println("Server listening on port "+PORT_NUMBER);
        
        
        /** listening for incomming messages */
        while (true) {
            try {
                socket.receive(packet);
                analyse(packet);
                this.updateViews();
            } catch (SocketTimeoutException ex) {
                this.updateViews();
            } catch (Exception ex) {
                System.out.println("Receive err");
                ex.printStackTrace();
            }
        }
    }
    
    public void analyse(DatagramPacket packet) {
        ClientStruct client = getClientFromIp(packet.getSocketAddress());
        ClientStruct requestedClient;
        MsgServStruct message = MsgServStruct.get(packet.getData());
        if (message != null) {
            /** message struct full */
            num_message++;
            System.out.println("MESSAGE n "+num_message);
            if (client == null && message.type != MsgServStruct.HELLO) {
                sendRECONNECT(packet.getSocketAddress());
                return;
            }
            switch (message.type) {
                case MsgServStruct.HELLO :
                    /**	connection request from peer */
                    System.out.println("HELLO received from "+packet.getSocketAddress().toString());
                    if (client == null) {
                        /**	client isn't yet registered */
                        client = new ClientStruct(packet.getSocketAddress(), message.ip1);
                        ClientStruct clientByID = getClientFromId(client.vpnIPString);
                        if (clientByID == null) {
                            /** OK for registering with this vpn IP on this vpn id*/
                            clients.add(client);
                            sendOK(true, packet.getSocketAddress());
                        } else if (clientByID.isTimeout()) {
                            removeConnectionsWithClient(clientByID.vpnIPString);
                            clients.remove(clientByID);
                            clients.add(client);
                            sendOK(true, packet.getSocketAddress());
                        } else {
                            /** NOT OK for registering */
                            client = null;
                            sendOK(false, packet.getSocketAddress());
                        }
                    } else {
                        /** client already registered but...
                         * check timeout connection */
                        if (client.isTimeout()) {
                            /** a PC is already connected but seemed to be timeout */
                            ClientStruct newClient = new ClientStruct(packet.getSocketAddress(), message.ip1);
                            if (newClient.sameAs(client)) {
                                client.updateTime();	// initialize timeout
                                sendOK(true, packet.getSocketAddress());
                            } else {// another VPNIP
                                removeConnectionsWithClient(client.vpnIPString);
                                clients.remove(client);
                                clients.add(newClient);
                                client = null;
                                sendOK(true, packet.getSocketAddress());
                            }
                        } else {
                            /** a PC is already connected on this socket (IPv4 address + port) */
                            System.out.println("A client is already connected with this IP/port");
                            sendOK(false, packet.getSocketAddress());
                        }
                    }
                    break;
                case MsgServStruct.BYE:
                    System.out.println("BYE received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    if (client != null) {
                        removeConnectionsWithClient(client.vpnIPString);
                        clients.remove(client);
                    }
                case MsgServStruct.PING :
                    /**	ping from a peer */
                    System.out.println("PING received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    if (client != null) {
                        sendPONG(packet.getSocketAddress());
                        client.updateTime();
                    }
                    break;
                case MsgServStruct.PONG :
                    /**	pong from the server */
                    System.out.println("PONG received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    break;
//                case MsgServStruct.OK :
//                    //TODO: ce message sert-il à qqchose ?
//                    /**	message ok */
//                    System.out.println("OK receive from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
//                    requestedClient = (ClientStruct) getClientFromId(MsgServStruct.unMapAddress(message.ip1));
//                    /** FWD_CONNECTION or ANS_CONNECTION answer -> remove reference -> peers are connected */
//                    if (requestedClient != null) {
//                        /** remove references */
////						connections.remove(client.vpnIPString+":"+requestedClient.vpnIPString);
////						connections.remove(requestedClient.vpnIPString+":"+client.vpnIPString);
//                    }
//                    break;
                case MsgServStruct.NOK :
                    /**	message not ok */
                    System.out.println("NOK received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    break;
                case MsgServStruct.ASK_CONNECTION :
                    /**	peer2peer connection request */
                    System.out.println("ASK_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    requestedClient = getClientFromId(MsgServStruct.unMapAddress(message.ip1));
                    if (requestedClient == null) {
                        /** unknown peer */
                        sendREJECT(packet.getSocketAddress(), message.ip1);
                        System.out.println("Peer doesn't exist");
                    } else {
                        /** known peer */
                        Connection connection = getConnection(client.vpnIPString, requestedClient.vpnIPString);
                        if (requestedClient.isTimeout()) {
                            /** peer no more connected */
                            sendREJECT(packet.getSocketAddress(), message.ip1);
                            System.out.println("Peer isn't connected anymore");
                            /** remove references */
                            removeConnectionsWithClient(requestedClient.vpnIPString);
                            clients.remove(requestedClient);
                            requestedClient = null;
                            break;
                        }
                        if (connection == null) {
                            // TODO rejet dans tous les cas
                            /** test if the reverse connection exist */
                            Connection reverseConnection = getConnection(requestedClient.vpnIPString, client.vpnIPString);
                            if (reverseConnection == null) {
                                /** connection demand doesn't already exist */
                                connection = new Connection(client, requestedClient);	// instantiating the connection
                                connections.add(connection);
                                sendANS(client, requestedClient);
                                sendFWD(requestedClient, client);
                            } else/* if (reverseConnection.isTimeout())*/ {
                                connections.remove(reverseConnection);
                                reverseConnection = null;
                                connection = new Connection(client, requestedClient);	// instantiating the connection
                                connections.add(connection);
                                sendANS(client, requestedClient);
                                sendFWD(requestedClient, client);
                            }
                        } else {
                            /** connection demand already exists */
//							if (connection.getTries()>MAX_CONNECTION_TRIES-2) {
//								/** after it tries consider connection connot be established */
//								connections.remove(client.vpnIPString+":"+requestedClient.vpnIPString);
//								sendREJECT(client.sAddr, requestedClient.vpnIP);
//								sendREJECT(requestedClient.sAddr, client.vpnIP);
//								connection = null;
//							}
//							if (connection.getTries()%2 == 0) {
                            sendANS(connection.client1, connection.client2);
//								pause();	// wait before forwarding connection request
                            sendFWD(connection.client2, connection.client1);
//							}
//							else {
//								sendFWD(connection.client2, connection.client1);
//								pause();	// wait before forwarding connection request
//								sendANS(connection.client1, connection.client2);
//							}
                            connection.updateTime();
                        }
                    }
                    break;
                case MsgServStruct.CLOSE_CONNECTION :
                    System.out.println("CLOSE_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    requestedClient = getClientFromId(MsgServStruct.unMapAddress(message.ip1));
                    if (client != null && requestedClient != null) {
                        removeConnection(client.vpnIPString, requestedClient.vpnIPString);
                    }
                    break;
                case MsgServStruct.FWD_CONNECTION :
                    /**	peer2peer connection forward */
                    System.out.println("FWD_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    break;
                case MsgServStruct.REJ_CONNECTION :
                    /**	peer2peer connection forward */
                    System.out.println("REJ_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    break;
                case MsgServStruct.ANS_CONNECTION :
                    /**	peer2peer connection answer */
                    System.out.println("ANS_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    
                    break;
                default : break;
            }
            System.gc();	// Garbage useless ClientStruct
        } else System.out.println("Message err : received message too short");
    }
    
    public void pause() {
        try {
            wait(WAIT_MILLIS_FWD);
        } catch (Exception e) {
            System.out.println("Wait err : "+e.toString());
        }
    }
    
    public void sendOK(boolean ok, SocketAddress address) {
        try {
            if (ok) this.socket.send(new DatagramPacket(MsgServStruct.MSG_OK,MsgServStruct.MSG_LENGTH,address));
            else this.socket.send(new DatagramPacket(MsgServStruct.MSG_NOK,MsgServStruct.MSG_LENGTH,address));
            if (ok) System.out.println("OK sent to client");
            else System.out.println("NOK sent to client");
        } catch (Exception ex) {
            System.out.println("SentOK err : "+ex.toString());
        }
    }
    
    /** nok message specifying the unexisting peer for which an ASK_CONNECTION as been made */
    public void sendREJECT(SocketAddress address, byte[] IP) {
        try {
            MsgServStruct message = new MsgServStruct(MsgServStruct.REJ_CONNECTION);
            message.ip1 = IP;
            this.socket.send(new DatagramPacket(MsgServStruct.set(message), MsgServStruct.MSG_LENGTH, address));
        } catch (Exception ex) {
            System.out.println("SentREJECT err : "+ex.toString());
        }
    }
    
    public void sendPING(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_PING,MsgServStruct.MSG_LENGTH,address));
            System.out.println("PING sent to client");
        } catch (Exception ex) {
            System.out.println("SentPING err : "+ex.toString());
        }
    }
    
    public void sendPONG(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_PONG,MsgServStruct.MSG_LENGTH,address));
            System.out.println("PONG sent to client");
        } catch (Exception ex) {
            System.out.println("SentPONG err : "+ex.toString());
        }
    }
    
    public void sendANS(ClientStruct client, ClientStruct requestedClient) {
        try {
            MsgServStruct message = new MsgServStruct(MsgServStruct.ANS_CONNECTION);
            message.port = requestedClient.port;		// public port to punch
            message.ip1 = requestedClient.realIP;		// public address to punch
            message.ip2 = requestedClient.vpnIP;		// corresponding VPN IP (the address asked)
            this.socket.send(new DatagramPacket(MsgServStruct.set(message), MsgServStruct.MSG_LENGTH, client.sAddr));
        } catch (Exception ex) {
            System.out.println("SentANS err : "+ex.toString());
        }
    }
    
    public void sendFWD(ClientStruct client, ClientStruct requestedClient) {
        try {
            MsgServStruct message = new MsgServStruct(MsgServStruct.FWD_CONNECTION);
            message.port = requestedClient.port;		// public port to punch
            message.ip1 = requestedClient.realIP;		// public address to punch
            message.ip2 = requestedClient.vpnIP;		// corresponding VPN IP
            this.socket.send(new DatagramPacket(MsgServStruct.set(message), MsgServStruct.MSG_LENGTH, client.sAddr));
        } catch (Exception ex) {
            System.out.println("SentFWD err : "+ex.toString());
        }
    }
    
    public void sendRECONNECT(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_RECONNECT,MsgServStruct.MSG_LENGTH,address));
            System.out.println("RECONNECT sent to client");
        } catch (Exception ex) {
            System.out.println("SenRECONNECT err : "+ex.toString());
        }
    }
    
    
    /** No arguments needed
     * Change the port number in the CampagnolConstants.java file */
    public static void main(String args[]) {
        if (args.length == 1) {
            if (args[0].equals("-g"))
                new CampagnolServer(true);
            else {
                System.err.println("usage: java CampagnolServer [-g]");
                System.err.println("\t-g");
                System.err.println("\t    start GUI");
                System.exit(1);
            }
        }
        new CampagnolServer(false);
    }
    
}
