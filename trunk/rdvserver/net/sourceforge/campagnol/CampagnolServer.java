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

package net.sourceforge.campagnol;

import java.util.*;
import java.net.*;

/**
 * This class defines the key used to store the connections
 * into a hashmap.
 * It combines the two vpnIP.
 */
class ConnectionKey {
    public byte[] vpnIP1, vpnIP2;
    public ConnectionKey(byte[] vpnIP1, byte[] vpnIP2) {
        this.vpnIP1 = vpnIP1;
        this.vpnIP2 = vpnIP2;
    }
    public ConnectionKey(Connection connection) {
        this.vpnIP1 = connection.client1.vpnIP;
        this.vpnIP2 = connection.client2.vpnIP;
    }
    public boolean equals(Object obj) {
        ConnectionKey key = (ConnectionKey) obj;
        return Arrays.equals(this.vpnIP1, key.vpnIP1) && Arrays.equals(this.vpnIP2, key.vpnIP2);
    }
    public int hashCode() {
        return this.vpnIP1.hashCode() + this.vpnIP2.hashCode();
    }
}

/**    class in charge of starting up the server
 * and of all operation on the client objects */
public class CampagnolServer {
    
    public static String VERSION = "0.2";
    
    /** flags */
    public static boolean verbose = false;
    public static boolean debug = false;
    public static boolean dump = false;
    
    /** constants */
    final int BUFFSIZE = MsgServStruct.MSG_LENGTH;
    
    /** local variables */
    private int portNumber = 57888;
    private DatagramSocket socket;
    private CampagnolGUI gui = null;
    
    /** HashMaps for the client's structures (mapped by real address and VPN IP)
     * and connections' structures
     */
    private HashMap clientsHashAddr, clientsHashVPN, connectionsHash;
    
    /** insert a client structure into the maps */
    private void addClient(ClientStruct cl) {
        clientsHashAddr.put(cl.sAddr, cl);
        clientsHashVPN.put(cl.vpnInet, cl);
        if (CampagnolServer.verbose) System.out.println("New client ["+cl.sAddr.toString()+"] "+cl.vpnIPString);
    }
    
    /** remove client from the maps */
    private void removeClient(ClientStruct cl) {
        clientsHashAddr.remove(cl.sAddr);
        clientsHashVPN.remove(cl.vpnInet);
        if (CampagnolServer.verbose) System.out.println("Remove client ["+cl.sAddr.toString()+"] "+cl.vpnIPString);
    }
    
    /** Get a client from it's VPN IP */
    private ClientStruct getClientFromId(byte[] vpnip) {
        try {
            return getClientFromId(InetAddress.getByAddress(vpnip));
        } catch (UnknownHostException e) {
            e.printStackTrace();
            return null;
        }
    }
    private ClientStruct getClientFromId(InetAddress vpnip) {
        return (ClientStruct) clientsHashVPN.get(vpnip);
    }
    
    /** Get a client from it's real address */
    private ClientStruct getClientFromIp(SocketAddress ad) {
        return (ClientStruct) clientsHashAddr.get(ad);
    }
    
    
    /** Insert a connection structure into the hashmap */
    private void addConnection(Connection ct) {
        connectionsHash.put(new ConnectionKey(ct), ct);
        if (CampagnolServer.verbose) System.out.println("New connection from "+ct.client1.vpnIPString+" to "+ct.client2.vpnIPString);
    }
    
    /** Remove ct from the hashmap */
    private void removeConnection(Connection ct) {
        ct = (Connection) connectionsHash.remove(new ConnectionKey(ct));
        if (ct != null && CampagnolServer.verbose) System.out.println("Remove connection from "+ct.client1.vpnIPString+" to "+ct.client2.vpnIPString);
    }
    /** Remove the connection vpnIP1 -> vpnIP2 or the reverse one */
    private void removeConnection(byte[] vpnIP1, byte[] vpnIP2) {
        Connection ct;
        ct = (Connection) connectionsHash.remove(new ConnectionKey(vpnIP1, vpnIP2));
        if (ct != null && CampagnolServer.verbose) System.out.println("Remove connection from "+ct.client1.vpnIPString+" to "+ct.client2.vpnIPString);
        ct = (Connection) connectionsHash.remove(new ConnectionKey(vpnIP2, vpnIP1));
        if (ct != null && CampagnolServer.verbose) System.out.println("Remove connection from "+ct.client2.vpnIPString+" to "+ct.client1.vpnIPString);
    }
    
    /** Get the connection started by the first IP */
    private Connection getConnection(byte[] vpnIP1, byte[] vpnIP2) {
        return (Connection) connectionsHash.get(new ConnectionKey(vpnIP1, vpnIP2));
    }
    
    /** Remove all connections involving vpnIP */
    private void removeConnectionsWithClient(ClientStruct client) {
        Object[] cxs = connectionsHash.values().toArray();
        for (int i=0; i<cxs.length; i++) {
            Connection ct = (Connection) cxs[i];
            if (Arrays.equals(ct.client1.vpnIP, client.vpnIP) || Arrays.equals(ct.client2.vpnIP,client. vpnIP)) {
                removeConnection(ct);
            }
        }
    }
    
    
    private void updateViews() {
        if (this.gui != null) {
            this.gui.update();
        }
    }
    
    private CampagnolServer(int portNumber, boolean withGui) {
        /**    instantiating the datagram socket */
        try {
            this.socket = new DatagramSocket(portNumber);
            this.socket.setSoTimeout(5000);
        } catch (SocketException ex) {
            System.err.println("Error while opening the socket:");
            ex.printStackTrace();
            System.exit(0);
        }
        this.portNumber = portNumber;
        this.clientsHashAddr = new HashMap();
        this.clientsHashVPN = new HashMap();
        this.connectionsHash = new HashMap();
        if (withGui) {
            this.gui = new CampagnolGUI(this.clientsHashAddr, this.connectionsHash);
            this.gui.setVisible(true);
        }
    }
    
    public void run() {
        DatagramPacket packet;
        packet = new DatagramPacket(new byte[BUFFSIZE], BUFFSIZE);
        
        System.out.println("Server listening on port "+portNumber);

        /** listening for incomming messages */
        while (true) {
            try {
                socket.receive(packet);
                analyse(packet);
                this.updateViews();
            } catch (SocketTimeoutException ex) {
                this.updateViews();
            } catch (Exception ex) {
                System.err.println("Receive err:");
                ex.printStackTrace();
            }
        }
    }
    
    public void analyse(DatagramPacket packet) {
        ClientStruct client = getClientFromIp(packet.getSocketAddress());
        ClientStruct requestedClient;
        MsgServStruct message = MsgServStruct.fromBytes(packet.getData());
        if (message != null) {
            if (client == null && message.type != MsgServStruct.HELLO) {
                sendRECONNECT(packet.getSocketAddress());
                return;
            }
            switch (message.type) {
                case MsgServStruct.HELLO :
                    /**    connection request from peer */
                    if (CampagnolServer.debug) System.out.println("<< HELLO received from "+packet.getSocketAddress().toString());
                    if (client == null) {
                        /**    client isn't yet registered */
                        if (message.port != 0) {
                            /** the client gave its local port and local IP */
                            client = new ClientStruct(packet.getSocketAddress(), message.ip1, message.ip2, message.port);
                        }
                        else {
                            client = new ClientStruct(packet.getSocketAddress(), message.ip1);
                        }
                        ClientStruct clientByID = getClientFromId(client.vpnInet);
                        if (clientByID == null) {
                            /** OK for registering with this vpn IP on this vpn id*/
                            addClient(client);
                            sendOK(packet.getSocketAddress());
                        } else if (clientByID.isTimeout()) {
                            removeConnectionsWithClient(clientByID);
                            removeClient(clientByID);
                            addClient(client);
                            sendOK(packet.getSocketAddress());
                        } else {
                            /** NOT OK for registering */
                            client = null;
                            sendNOK(packet.getSocketAddress());
                        }
                    } else {
                        /** client already registered but...
                         * check timeout connection */
                        if (client.isTimeout()) {
                            /** a PC is already connected but seemed to be timeout */
                            if (java.util.Arrays.equals(message.ip1, client.vpnIP)) {
                                client.updateTime();    // initialize timeout
                                sendOK(packet.getSocketAddress());
                            } else {// another VPNIP
                                removeConnectionsWithClient(client);
                                removeClient(client);
                                ClientStruct newClient = null;
                                if (message.port != 0) {
                                    newClient = new ClientStruct(packet.getSocketAddress(), message.ip1, message.ip2, message.port);
                                }
                                else {
                                    newClient = new ClientStruct(packet.getSocketAddress(), message.ip1);
                                }
                                addClient(newClient);
                                client = null;
                                sendOK(packet.getSocketAddress());
                            }
                        } else {
                            /** a PC is already connected on this socket (IPv4 address + port) */
                            if (CampagnolServer.debug) System.out.println("A client is already connected with this IP/port "+packet.getSocketAddress());
                            sendNOK(packet.getSocketAddress());
                        }
                    }
                    break;
                case MsgServStruct.BYE:
                    if (CampagnolServer.debug) System.out.println("<< BYE received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    if (client != null) {
                        removeConnectionsWithClient(client);
                        removeClient(client);
                    }
                    break;
                case MsgServStruct.PING :
                    /**    ping from a peer */
                    if (CampagnolServer.debug) System.out.println("<< PING received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    if (client != null) {
                        sendPONG(packet.getSocketAddress());
                        client.updateTime();
                    }
                    break;
                case MsgServStruct.ASK_CONNECTION :
                    /**    peer2peer connection request */
                    if (CampagnolServer.debug) System.out.println("<< ASK_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    requestedClient = getClientFromId(message.ip1);
                    if (requestedClient == null) {
                        /** unknown peer */
                        sendREJECT(packet.getSocketAddress(), message.ip1);
                        if (CampagnolServer.debug) System.out.println("Peer "+MsgServStruct.convertIPtoString(message.ip1)+" doesn't exist");
                    } else {
                        /** known peer */
                        Connection connection = getConnection(client.vpnIP, requestedClient.vpnIP);
                        if (requestedClient.isTimeout()) {
                            /** peer no more connected */
                            sendREJECT(packet.getSocketAddress(), message.ip1);
                            if (CampagnolServer.debug) System.out.println("Peer isn't connected anymore");
                            /** remove references */
                            removeConnectionsWithClient(requestedClient);
                            removeClient(requestedClient);
                            requestedClient = null;
                            break;
                        }
                        /** should we send the local or remote IP/port ?*/
                        boolean send_local_ip = (client.localPort != 0)
                            && (requestedClient.localPort != 0)
                            && Arrays.equals(client.realIP,
                                    requestedClient.realIP);
                        if (connection == null) {
                            /** test if the reverse connection exist */
                            Connection reverseConnection = getConnection(requestedClient.vpnIP, client.vpnIP);
                            if (reverseConnection == null) {
                                /** connection demand doesn't already exist */
                                connection = new Connection(client, requestedClient);    // instantiating the connection
                                addConnection(connection);
                                sendANS(client, requestedClient, send_local_ip);
                                sendFWD(requestedClient, client, send_local_ip);
                            } else {
                                removeConnection(reverseConnection);
                                reverseConnection = null;
                                connection = new Connection(client, requestedClient);    // instantiating the connection
                                addConnection(connection);
                                sendANS(client, requestedClient, send_local_ip);
                                sendFWD(requestedClient, client, send_local_ip);
                            }
                        } else {
                            /** connection demand already exists */
                            sendANS(connection.client1, connection.client2, send_local_ip);
                            sendFWD(connection.client2, connection.client1, send_local_ip);
                            connection.updateTime();
                        }
                    }
                    break;
                case MsgServStruct.CLOSE_CONNECTION :
                    if (CampagnolServer.debug) System.out.println("<< CLOSE_CONNECTION received from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                    requestedClient = getClientFromId(message.ip1);
                    if (client != null && requestedClient != null) {
                        removeConnection(client.vpnIP, requestedClient.vpnIP);
                    }
                    break;
                default:
                    if (CampagnolServer.debug) {
                        System.out.println("Received unexpected message from "+packet.getSocketAddress().toString()+" ("+client.vpnIPString+")");
                        if (CampagnolServer.debug) System.out.println(message);
                    }
                    break;
            }
        } else {
            if (CampagnolServer.debug) System.out.println("Message err: received message too short");
        }
    }
    
    public void sendOK(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_OK,MsgServStruct.MSG_LENGTH,address));
            if (CampagnolServer.debug) System.out.println(">> OK sent to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendOK:");
            ex.printStackTrace();
        }
    }
    
    public void sendNOK(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_NOK,MsgServStruct.MSG_LENGTH,address));
            if (CampagnolServer.debug) System.out.println(">> NOK sent to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendNOK:");
            ex.printStackTrace();
        }
    }
    
    /** nok message specifying the unexisting peer for which an ASK_CONNECTION as been made */
    public void sendREJECT(SocketAddress address, byte[] IP) {
        try {
            MsgServStruct message = new MsgServStruct(MsgServStruct.REJ_CONNECTION, (short) 0, IP, null);
            this.socket.send(new DatagramPacket(message.toBytes(), MsgServStruct.MSG_LENGTH, address));
            if (CampagnolServer.debug) System.out.println(">> REJECT send to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendREJECT:");
            ex.printStackTrace();
        }
    }
    
    public void sendPING(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_PING,MsgServStruct.MSG_LENGTH,address));
            if (CampagnolServer.debug) System.out.println(">> PING sent to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendPING:");
            ex.printStackTrace();
        }
    }
    
    public void sendPONG(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_PONG,MsgServStruct.MSG_LENGTH,address));
            if (CampagnolServer.debug) System.out.println(">> PONG sent to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendPONG:");
            ex.printStackTrace();
        }
    }
    
    public void sendANS(ClientStruct client, ClientStruct requestedClient, boolean send_local) {
        try {
            MsgServStruct message = null;
            if (send_local) {
                message = new MsgServStruct(MsgServStruct.ANS_CONNECTION, requestedClient.localPort, requestedClient.localIP, requestedClient.vpnIP);
            }
            else {
                message = new MsgServStruct(MsgServStruct.ANS_CONNECTION, requestedClient.port, requestedClient.realIP, requestedClient.vpnIP);
            }
            this.socket.send(new DatagramPacket(message.toBytes(), MsgServStruct.MSG_LENGTH, client.sAddr));
            if (CampagnolServer.debug) System.out.println(">> ANS_CONNECTION sent to client "+client.sAddr.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendANS:");
            ex.printStackTrace();
        }
    }
    
    public void sendFWD(ClientStruct client, ClientStruct requestedClient, boolean send_local) {
        try {
            MsgServStruct message = null;
            if (send_local) {
                message = new MsgServStruct(MsgServStruct.FWD_CONNECTION, requestedClient.localPort, requestedClient.localIP, requestedClient.vpnIP);
            }
            else {
                message = new MsgServStruct(MsgServStruct.FWD_CONNECTION, requestedClient.port, requestedClient.realIP, requestedClient.vpnIP);
            }
            this.socket.send(new DatagramPacket(message.toBytes(), MsgServStruct.MSG_LENGTH, client.sAddr));
            if (CampagnolServer.debug) System.out.println(">> FWD_CONNECTION sent to client "+client.sAddr.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendFWD:");
            ex.printStackTrace();
        }
    }
    
    public void sendRECONNECT(SocketAddress address) {
        try {
            this.socket.send(new DatagramPacket(MsgServStruct.MSG_RECONNECT,MsgServStruct.MSG_LENGTH,address));
            if (CampagnolServer.debug) System.out.println(">> RECONNECT sent to client "+address.toString());
        } catch (Exception ex) {
            System.err.println("Error in sendRECONNECT:");
            ex.printStackTrace();
        }
    }
    
    
    public static void usage() {
        System.err.println("Usage: java CampagnolServer [OPTION]...\n");
        System.err.println("Options");
        System.err.println(" -v, --verbose\t\t\tverbose mode");
        System.err.println(" -d, --debug\t\t\tdebug mode. Can be use twice to dump the packets");
        System.err.println(" -p, --port=PORT\t\tlistening port");
        System.err.println(" -g, --gui\t\t\tstart the GUI with the server");
        System.err.println(" -h, --help\t\t\tthis help message");
        System.err.println(" -V, --version\t\t\tshow version information and exit\n");
    }
    
    public static void version() {
        System.err.println("Campagnol VPN | Server | Version "+VERSION);
        System.err.println("Copyright (c) 2007 Antoine Vianey");
        System.err.println("              2008 Florent Bondoux");
    }
    
    public static void main(String args[]) {
        CampagnolServer server = null;
        boolean withGui = false;
        int portNumber = 57888;
        
        ArrayList options = null;
        ArrayList arguments = null;
        try {
            ArrayList[] opt_args = OptionParser.getopt(args, "gvVdhp:", new String[] {"gui", "verbose", "version", "debug", "help", "port:"});
            options = opt_args[0];
            arguments = opt_args[1];
        } catch (OptionParser.GetoptException e) {
            System.err.println(e.getMessage());
            CampagnolServer.usage();
            System.exit(1);
        }
        
        for (int i = 0; i < options.size(); i++) {
            String[] opt = (String[]) options.get(i);
            if (opt[0].equals("-g") || opt[0].equals("--gui")) {
                withGui = true;
            }
            else if (opt[0].equals("-v") || opt[0].equals("--verbose")) {
                CampagnolServer.verbose = true;
            }
            else if (opt[0].equals("-d") || opt[0].equals("--debug")) {
                if (CampagnolServer.debug) {
                    CampagnolServer.dump = true;
                }
                else {
                    CampagnolServer.verbose = true;
                    CampagnolServer.debug = true;
                }
            }
            else if (opt[0].equals("-h") || opt[0].equals("--help")) {
                CampagnolServer.usage();
                System.exit(1);
            }
            else if (opt[0].equals("-p") || opt[0].equals("--port")) {
                portNumber = Integer.parseInt(opt[1]);
            }
            else if (opt[0].equals("-V") || opt[0].equals("--version")) {
                CampagnolServer.version();
                System.exit(0);
            }
        }

        if (arguments.size() != 0) {
            CampagnolServer.usage();
            System.exit(1);
        }
        
        server = new CampagnolServer(portNumber, withGui);
        server.run();
    }
    
}
