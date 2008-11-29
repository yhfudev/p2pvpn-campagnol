/**
 * Rendez-vous server, starting point
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

import java.util.ArrayList;

public class CampagnolRDV {
    
    public static String VERSION = "0.3";
    
    public static void usage() {
        System.err.println("Usage: campagnol_rdv [OPTION]...\n");
        System.err.println("Options");
        System.err.println(" -v, --verbose                  verbose mode");
        System.err.println(" -d, --debug                    debug mode. Can be use twice to dump the packets");
        System.err.println(" -p, --port=PORT                listening port");
        System.err.println(" -m, --max-clients=N            set the maximum number of connected clients");
        System.err.println(" -g, --gui                      start the GUI with the server");
        System.err.println(" -h, --help                     this help message");
        System.err.println(" -V, --version                  show version information and exit\n");
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
        int maxclients = 0;
        
        ArrayList options = null;
        ArrayList arguments = null;
        try {
            ArrayList[] opt_args = OptionParser.getopt(args, "gvVdhp:m:", new String[] {"gui", "verbose", "version", "debug", "help", "port:", "max-clients:"});
            options = opt_args[0];
            arguments = opt_args[1];
        } catch (OptionParser.GetoptException e) {
            System.err.println(e.getMessage());
            CampagnolRDV.usage();
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
                CampagnolRDV.usage();
                System.exit(1);
            }
            else if (opt[0].equals("-p") || opt[0].equals("--port")) {
                portNumber = Integer.parseInt(opt[1]);
            }
            else if (opt[0].equals("-m") || opt[0].equals("--max-clients")) {
                maxclients = Integer.parseInt(opt[1]);
            }
            else if (opt[0].equals("-V") || opt[0].equals("--version")) {
                CampagnolRDV.version();
                System.exit(0);
            }
        }

        if (arguments.size() != 0) {
            CampagnolRDV.usage();
            System.exit(1);
        }
        
        server = new CampagnolServer(portNumber, maxclients, withGui);
        server.run();
    }
    
}

