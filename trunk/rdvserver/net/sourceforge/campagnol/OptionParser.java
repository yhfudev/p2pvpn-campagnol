/**
 * Command-line argument parser
 * 
 * Copyright (C) 2008 Florent Bondoux
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

import java.util.HashMap;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Command-line argument parser
 */
public class OptionParser {
    
    
    private static final int NO_ARGUMENT = 0;
    private static final int MANDATORY_ARGUMENT = 1;
    private static final int OPTIONAL_ARGUMENT = 2;
    
    /**
     * The exception raised by getopt
     */
    public static class GetoptException extends Exception {

        public GetoptException(String message) {
            super(message);
        }
        
    }
    
    /**
     * Search a key in longOptions.
     * 
     * If there is no exact match, search for a unique abbreviation.
     * 
     * Raise GetoptException if no key is found or if an abbreviation is ambiguous.
     * 
     * @param longOptions Long options HashMap from getopt
     * @param key The search pattern
     * @return The key if it has been found
     * @throws GetoptException
     */
    private static String getKey(HashMap longOptions, String key) throws GetoptException {
        if (longOptions.containsKey(key)) {
            return key;
        }
        else {
            String match = null;
            for (Iterator i = longOptions.keySet().iterator(); i.hasNext(); ) {
                String k = (String) i.next();
                if (k.startsWith(key)) {
                    if (match == null) {
                        match = k;
                    }
                    else {
                        throw new GetoptException("Option --"+key+" is ambiguous");
                    }
                }
            }
            if (match != null) {
                return match;
            }
            else {
                throw new GetoptException("Unknow option --"+key);
            }
        }
    }
    
    /**
     * Parses the command-line arguments
     * 
     * Accept short (-o) and long (--opt) options with required or optional argument.
     * 
     * Short option without argument:
     *   "-o"
     * Short options with a mandatory argument:
     *   "-o foo" or "-ofoo"
     * Short options with an optional argument:
     *   "-o" or "-ofoo"
     * Long options without argument:
     *   "--opt"
     * Long options with a mandatory argument:
     *   "--opt=param" or "--opt param"
     * Long options with an optional argument:
     *   "--opt" or "--opt=param"
     *   
     * Several short options can be specified after one '-' as long as all
     * except possibly the last one take no argument.
     * 
     * Long options may be abbreviated if the abbreviation is not ambiguous.
     * 
     * 
     * @param args The command line arguments.
     * @param options The short options list with a syntax similar to that of the GNU getopt:
     *                It's a string containing the option characters.
     *                If a character is followed by a colon, the option requires an argument.
     *                If a character is followed by two colons, the option takes an optional argument.
     *                If the first character is '+', the processing stops with the first non option argument encountered.
     *                If the first character is '-', each non option argument is handled as the value of an option "" (empty string).
     * @param long_options The long options list, given without the leading "--" characters.
     *                     If a name is followed by a colon, the option requires an argument.
     *                     If a name is followed by two colons, the option takes an optional argument.
     * @return An array of two ArrayList.
     *         The first ArrayList contains String arrays {option, value}.
     *         'option' is the short or long option with the leading "-" or "--".
     *         'value' is null if the option takes no argument.
     *         The second ArrayList contains the remaining arguments.
     * @throws GetoptException
     */
    public static ArrayList[] getopt(String[] args, String options, String[] long_options) throws GetoptException {
        HashMap shortOptions = new HashMap();
        HashMap longOptions = new HashMap();
        ArrayList optionsList = new ArrayList();
        ArrayList remainingOptionsList = new ArrayList();
        
        boolean stopAtFirstNonOption = false;
        boolean handleNonOption = false;
        
        /** parse the short options list */
        if (options != null) {
            char[] optionsArray = options.toCharArray();
            int i = 0;
            if (optionsArray[0] == '-') {
                handleNonOption = true;
                i++;
            }
            else if (optionsArray[0] == '+') {
                stopAtFirstNonOption = true;
                i++;
            }
            for (; i < optionsArray.length; i++) {
                char c = optionsArray[i];
                if (!shortOptions.containsKey(new Character(c))) {
                    if (i + 2 < optionsArray.length && optionsArray[i+1] == ':' && optionsArray[i+2] == ':') {
                        shortOptions.put(new Character(c), new Integer(OPTIONAL_ARGUMENT));
                        i+= 2;
                    }
                    else if (i + 1 < optionsArray.length && optionsArray[i+1] == ':') {
                        shortOptions.put(new Character(c), new Integer(MANDATORY_ARGUMENT));
                        i++;
                    }
                    else {
                        shortOptions.put(new Character(c), new Integer(NO_ARGUMENT));
                    }
                }
            }
        }
        
        /** parse the long options list */
        if (long_options != null) {
            for (int i = 0; i < long_options.length; i++) {
                String option = long_options[i];
                if (option.endsWith("::")) {
                    String key = option.substring(0, option.length()-2);
                    if (!longOptions.containsKey(key)) {
                        longOptions.put(key, new Integer(OPTIONAL_ARGUMENT));
                    }
                }
                else if (option.endsWith(":")) {
                    String key = option.substring(0, option.length()-1);
                    if (!longOptions.containsKey(key)) {
                        longOptions.put(key, new Integer(MANDATORY_ARGUMENT));
                    }
                }
                else {
                    if (!longOptions.containsKey(option)) {
                        longOptions.put(option, new Integer(NO_ARGUMENT));
                    }
                }
            }
        }
        
        int i;
        for (i = 0; i < args.length; i++) {
            String arg = args[i];
            /** Long option */
            if (arg.startsWith("--")) {
                int index = arg.indexOf("=");
                if (index != -1) {
                    String key = getKey(longOptions, arg.substring(2, index));
                    String value = arg.substring(index+1);
                    Integer opt = (Integer) longOptions.get(key);
                    if (opt.intValue() != NO_ARGUMENT) {
                        optionsList.add(new String[] {"--"+key, value});
                    }
                    else {
                        throw new GetoptException("Option --"+key+" takes no argument");
                    }
                }
                else {
                    String key = arg.substring(2);
                    if (key.length() == 0) {
                        i++;
                        break;
                    }
                    key = getKey(longOptions, key);
                    Integer opt = (Integer) longOptions.get(key);
                    if (opt.intValue() == MANDATORY_ARGUMENT) {
                        if (i+1 < args.length) {
                            String value = args[i+1];
                            optionsList.add(new String[] {"--"+key, value});
                            i++;
                        }
                        else {
                            throw new GetoptException("Option --"+key+" takes an argument");
                        }
                    }
                    else {
                        optionsList.add(new String[] {"--"+key, null});
                    }
                }
            }
            /** Short option */
            else if (arg.startsWith("-")) {
                String token = arg.substring(1);
                while (token.length() > 0) {
                    char c = token.charAt(0);
                    if (!shortOptions.containsKey(new Character(c))) {
                        throw new GetoptException("Unknow option -"+c);
                    }
                    Integer opt = (Integer) shortOptions.get(new Character(c));
                    if (opt.intValue() == MANDATORY_ARGUMENT) {
                        if (token.length() > 1) {
                            optionsList.add(new String[] {"-"+c, token.substring(1)});
                        }
                        else if (i+1 < args.length) {
                            optionsList.add(new String[] {"-"+c, args[i+1]});
                            i++;
                        }
                        else {
                            throw new GetoptException("Option -"+c+" takes an argument");
                        }
                        break;
                    }
                    else if (opt.intValue() == OPTIONAL_ARGUMENT) {
                        if (token.length() > 1) {
                            optionsList.add(new String[] {"-"+c, token.substring(1)});
                        }
                        else {
                            optionsList.add(new String[] {"-"+c, null});
                        }
                        break;
                    }
                    else {
                        optionsList.add(new String[] {"-"+c, null});
                        token = token.substring(1);
                    }
                }
            }
            /** argument */
            else {
                if (stopAtFirstNonOption) break;
                else if (handleNonOption) {
                    optionsList.add(new String[] {"", arg});
                }
                else {
                    remainingOptionsList.add(arg);
                }
            }
        }
        
        for (; i < args.length; i++) {
            remainingOptionsList.add(args[i]);
        }
        
        return new ArrayList[] {optionsList, remainingOptionsList};
    }
}
