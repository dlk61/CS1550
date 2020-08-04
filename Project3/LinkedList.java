import java.io.*;
import java.util.*;

public class LinkedList {

	private Frame headFrame, tailFrame;  // Pointer to the front (headFrame) element and back (tailFrame) of the list
	private int n = 0;					 // Count of how many frames in the list
	
	// Compiler does this anyway. just for emphasis
    public LinkedList()
	{
		headFrame = null; 				
		tailFrame = null;
	}

	// Return the head of the linked list
	public Frame getHead() {
		return headFrame;
	}

	// Return how many frames are in the list
	public int size() {
		return n;
	}

	// 	If index isn't set (for the LRU algorithm), default to -1
	public void insertAtTail(long pageAddress, String type) {		        
		insertAtTail(pageAddress, type, -1);
	}
	
	// Tack a new frame at the end of the list
    public void insertAtTail( long pageAddress, String type, int index)     
	{
		Frame newFrame = new Frame( pageAddress, type, index);				// Create a new frame

		if (headFrame == null){												// If list is empty...
			headFrame = tailFrame = newFrame;								// The frame is the tail as well as the head
		}
		else {																// If not...
			tailFrame.setNext(newFrame);									// Set the tail's next to be the new frame
			newFrame.setPrev(tailFrame);									// Set the new frame's prev to be the tail
			tailFrame = newFrame;										    // The new frame is now the tail
			tailFrame.setNext(null);									    // Set the tail's next to be null
		}	
		n++;																// Increment the number of frames in the list
    }

	// Search for the page address in the linked list
	public Frame search( long pageAddress )					
	{
		Frame curr = headFrame;											    // Set the head frame to curr
		while (curr != null)								                // Go through the frames in the linked list				
		{
			if (curr.getpageAddress() == pageAddress)		                // If page address's are equal, return the frame
				return curr;

			curr = curr.getNext();							                // If not, check the next frame
		}
		return curr;										                // If linked list is empty, return null
	}

	// Move the frame to the tail if it is not already the tail
	public void moveToTail(Frame moveFrame) 
	{
		if (moveFrame != tailFrame) {

			if (moveFrame == headFrame) {					               // If the frame to be moved is the head...
				headFrame.getNext().setPrev(null);			               // Set the frame after the head's prev to be null
				headFrame = headFrame.getNext();			               // Set the frame after the head to be the new head
			}

			else {														   // If the frame to be moved is in the middle...
				moveFrame.getNext().setPrev(moveFrame.getPrev());          // Set its next's prev to be its prev
				moveFrame.getPrev().setNext(moveFrame.getNext());		   // Set its prev's next to be its next
			}

			tailFrame.setNext(moveFrame);				  				  // Set the tail frame's next to be the frame
			Frame tempFrame = tailFrame;				  				  // Set the tail frame to be a temp
			tailFrame = moveFrame;						  				  // Set moveFrame to be the new tail frame
			tailFrame.setNext(null);					  				  // Set the tail frame's next to be null
			tailFrame.setPrev(tempFrame);				  				  // Set the tail frame's prev to be the temp frame

		}

	}

	// Returns the access count associated with the given page address
    public int get(long pageAddress) {
        for (Frame x = headFrame; x != null; x = x.getNext()) {
            if (pageAddress == x.getpageAddress()) {
				return x.getCount();
			}
        }
        return -1;
	}

	// Remove the head frame
	public Frame removeHead()
	{
		Frame temp = headFrame;										    // Set head frame to be a temp

		if (size() == 1) {											   // If there is only 1 frame...
			headFrame = null;							               // Set the head frame to be null
			tailFrame = null;							               // Set the tail frame to be null
		}
		else {											               // If there is more than 1 frame...
			headFrame.getNext().setPrev(null);			               // Set the head's next's prev to be null
			headFrame = headFrame.getNext();			               // Set the new head frame
		}
		n--;											              // Decrement the number of frames in the linked list
		return temp;									              // Return the frame that was removed
	}

	// Removes the page address and associated access type and access count
	public void delete(long pageAddress) {

		for (Frame curr = headFrame; curr != null; curr = curr.getNext()) {
			if (pageAddress == curr.getpageAddress()) {
				remove(curr);
				break;
			}
		}
    }

	// Delete frame in linked list
	public void remove(Frame rFrame) {
		
		if (size() == 1) {
			headFrame = null;
			tailFrame = null;
		}
		else {
			if (rFrame == headFrame) {
				headFrame.getNext().setPrev(null);
				headFrame = headFrame.getNext();
			}
			else if (rFrame == tailFrame) {
				rFrame.getPrev().setNext(null);
				tailFrame = rFrame.getPrev();
			}
			else {
				rFrame.getNext().setPrev(rFrame.getPrev());
				rFrame.getPrev().setNext(rFrame.getNext());
			}
		}
		n--;
		
	}

}