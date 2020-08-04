import java.util.*;
import java.io.*;

public class cs1550 {
    public static void main( String[] args) throws IOException
    {
        // Only run when user inputs 7 arguments
        if (args.length == 7) {
            
            int numFrames = Integer.parseInt(args[1]);                                  // Set and convert this String argument to the number of frames
            int pageSize = Integer.parseInt(args[3]);                                   // Set and convert this String argument to the pagesize in KB
            double pageOffsetDb = Math.log(pageSize * Math.pow(2,10)) / Math.log(2);    // Calculate page offset 
            int pageOffset = (int) pageOffsetDb;                                        // Convert page offset to an integer
            String algorithm = args[5];                                                 // Set the 5th argument to be the name of the page replacement algorithm
            String file = args[6];                                                      // Set the 6th argument to be the name of the trace file
            int totMemAccesses = 0, totPageFaults = 0, totWritesToDisk = 0;             // Set totals to 0
            String type, address, delims;                                               // Declare Strings
            long pageAddress;                                                           // Decalare the page address
            String tokens[];                                                            // Declare String array
            LinkedList lruList = new LinkedList();                                      // Create Linked List for LRU algorithm symbolizing physical memory
            Frame lruFrame;                                                             // Declare frame in lru's physical memory
            Frame removedLruFrame;                                                      // Declare the removed frame in lru list
            LinkedList optList = new LinkedList();                                      // Create Linked List for opt algorithm's physical memory
            Frame optFrame;                                                             // Declare frame in opt's physical memory
            Frame tempRemovedOptFrame;                                                      // Declare the removed frame in opt list
            Scanner reader;                                                             // Declare the Scanner object
            SeparateChainingHashST st = new SeparateChainingHashST();                   // Declare a separate chaining hash symbol table
            LinkedList tieList = new LinkedList();                                      // Create a linked list for frames that are not accessed in the future

            // Try to read the file
            try {
                reader = new Scanner(new FileInputStream(file));                        // Create the Scanner object from a FileInputStream object associated with the file

                // If user selects the LRU page replacement algorithm
                if (algorithm.equals("lru")) {

                    while (reader.hasNext()) {                                          // Iterate through each line
                        type = reader.next();                                           // Convert first string of the line to type access type (l or s)
                        address = reader.next();                                        // Convert second string of the line to the address
                        pageAddress = Long.decode(address) >> pageOffset;               // Convert the address to a long and then shift it to the right by the offset. This is the page address.
                        totMemAccesses++;                                               // increment number of memory accesses
                    
                        lruFrame = lruList.search(pageAddress);                         // Search for the page address in physical memory
                        
                        // If the page address is found...
                        if (lruFrame != null) {                        
                            lruList.moveToTail(lruFrame);                               // Move the address to the tail                         
                            if (type.equals("s"))                                       // If the type is store, set the type to s. This will keep track when we need to write to the disk
                                lruFrame.setType("s");
                        }
                        
                        // If the page address is not found...
                        else {
                            if (lruList.size() == numFrames) {                         // If memory is full...
                                removedLruFrame = lruList.removeHead();                // Remove the head and return it
                                if (removedLruFrame.getType().equals("s"))             // If the removed frame's type was s, write to the disk. This means the dirty bit was set.
                                    totWritesToDisk++;                                 
                            }
                            lruList.insertAtTail(pageAddress, type);                   // Insert page address with its access type at the tail of physical memory
                            totPageFaults++;                                           // Increase number of page faults
                        }
                    
                    }
                }

                // If user selects the LRU page replacement algorithm
                else if (algorithm.equals("opt")) {                                

                    int accessCount = 0;                                               // Declare and set count to 0. This will assign a number to each memory access
                    // Pre-traverse the file
                    while (reader.hasNext()) {                                         // Iterate through each line of file
                        type = reader.next();                                          // Convert first string of the line to access type (l or s)
                        address = reader.next();                                       // Convert second string of the line to the address
                        pageAddress = Long.decode(address) >> pageOffset;              // Convert the address to a long and then shift it to the right by the offset. This is the page address.
                        accessCount++;                                                 // Increment the count
                        st.put(pageAddress, type, accessCount);                        // Put the page address with its access type and access number in the hash table 
                    }
                    reader.close();                                                    // Close the file.


                    reader = new Scanner(new FileInputStream(file));                   // Create the Scanner object from a FileInputStream object associated with the file
                    while (reader.hasNext()) {                                         // Iterate through each line of file
                        type = reader.next();                                          // Convert first string of the line to access type (l or s)
                        address = reader.next();                                       // Convert second string of the line to the address
                        pageAddress = Long.decode(address) >> pageOffset;              // Convert the address to a long and then shift it to the right by the offset. This is the page address.
                        totMemAccesses++;                                              // increment number of memory accesses
                    
                        optFrame = optList.search(pageAddress);                        // Search for the page address in physical memory
                        
                        // If the page address is found...
                        if (optFrame != null) {                        
                            optFrame.setCount(totMemAccesses);                         // Set the frame's access count which is the total memory accesses at the time                
                            if (type.equals("s"))                                      // If the type is store, set the type to s. This will keep track when we need to write to the disk
                                optFrame.setType("s");
                        }

                        // If the page address is not found...
                        else {

                            if (optList.size() == numFrames) {                               // If memory is full...
            
                                tempRemovedOptFrame = optList.getHead();                     // Set the head frame of the optList to the frame that will be removed (as of now)
                                accessCount = st.get(tempRemovedOptFrame.getpageAddress());  // Get the access count number of the temporary removed opt frame. If the temporary removed opt frame is not in the symbol table (not used at all in the future) -1 is returned
                                
                                tieList.insertAtTail(tempRemovedOptFrame.getpageAddress(), tempRemovedOptFrame.getType(), tempRemovedOptFrame.getCount());          // Insert the temporary frame into the tie list

                                // Traverse the physical memory
                                for (Frame curr = tempRemovedOptFrame.getNext(); curr != null; curr = curr.getNext()) {

                                    // If the current frame's access count is greater than the count and the count is not -1, or the current frame's count is not equal to -1 and count is greater than 0...
                                    if (((st.get(curr.getpageAddress()) > accessCount) && accessCount > 0) || ((st.get(curr.getpageAddress()) == -1) && accessCount > 0)) {
                                        accessCount = st.get(curr.getpageAddress());         // Set the new access count to be the current frame's access count
                                        tempRemovedOptFrame = curr;                          // Set the current frame to be the frame that will be removed (as of now)
                                    }

                                    // Else if the current frame's access count is equal to the current access count (both -1 which means both not used at all in the future)
                                    else if (st.get(curr.getpageAddress()) == accessCount) {                                
                                        tieList.insertAtTail(curr.getpageAddress(), curr.getType(), curr.getCount());       // Add the frame to the tieList 
                                    }
                                }
                                
                                // In case of a tie (more than 1 are not used at all in the future), remove the least recently used. 
                                if ((accessCount == -1) && tieList.size() > 1) {
                                    Frame tempRemoved = tieList.getHead();                                                  // Grab the head of the tie list
                                    int compare = tempRemoved.getCount();                                                   // Grab the head's access count
                                    for (Frame curr = tempRemoved.getNext(); curr != null; curr = curr.getNext()) {         // Traverse tie list
                                        if (curr.getCount() < compare) {                                                    // If the current frame's access count is less than temp count...
                                            compare = curr.getCount();                                                      // Set the new compare
                                            tempRemoved = curr;                                                             // curr is now the removed frame (as of now)
                                        }
                                    }
                                    tempRemovedOptFrame = optList.search(tempRemoved.getpageAddress());                     // Search for the temporary removed frame's page address in the opt List and set it equal to the removed frame
                                }
                                
                                if (tempRemovedOptFrame.getType().equals("s"))                                              // If the type is store, set the type to s. This will keep track when we need to write to the disk
                                    totWritesToDisk++; 
                                
                                optList.remove(tempRemovedOptFrame);                                                        // Remove this frame from memory.

                                // Remove every frame from the tie list
                                for (Frame curr = tieList.getHead(); curr != null; curr = curr.getNext()) {
                                    tieList.remove(curr);
                                }
                            }

                            optList.insertAtTail(pageAddress, type, totMemAccesses);          // Insert page address with its access type and access number at the tail of physical memory
                            totPageFaults++;                                                  // Increase number of page faults
                        }

                        st.delete(pageAddress);                                               // Delete the frame from the symbol table to avoid longer runtime in future
                    }


                }

                else 
                    System.out.println("Please select either lru or opt");

                
                reader.close();  // Close stream and release resources
        
            }

            // If can't open file...
            catch (IOException e){
                System.out.println("Can't open file");
                return;                
            }

            // Print out summary statistics.
            System.out.println("Algorithm: " + algorithm.toUpperCase());
            System.out.println("Number of frames: " + numFrames);
            System.out.println("Page size: " + pageSize + " KB");
            System.out.println("Total memory accesses: " + totMemAccesses);
            System.out.println("Total page faults: " + totPageFaults);
            System.out.println("Total writes to disk: " + totWritesToDisk);

        }

        // If the user did not input 7 arguments, display and end program.
        else 
            System.out.println("There must be 7 arguments.");
    }
}