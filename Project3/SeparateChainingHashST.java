public class SeparateChainingHashST {

    private static final int INIT_CAPACITY = 1300001;
    private int m;                                      // hash table size
    private LinkedList[] st;                            // array of linked-list symbol tables

    // Initialize empty symbol table
    public SeparateChainingHashST() {
        this(INIT_CAPACITY);
    } 

    // Initialize empty symbol table with m chains
    public SeparateChainingHashST(int m) {
        this.m = m;
        st = (LinkedList[]) new LinkedList[m];
        for (int i = 0; i < m; i++)
            st[i] = new LinkedList();
    } 

    // hash value between 0 and m-1
    private int hash(long pageAddress) {
        String s = Long.toString(pageAddress);      // Convert long to string
        return (s.hashCode() & 0x7fffffff) % m;
    } 

    // Returns the count associated with the specified page address
    public int get(long pageAddress) {
        int i = hash(pageAddress);
        return st[i].get(pageAddress);
    } 
   
    // Inserts the specified page address, access type, and access count in the symbol table
    public void put(long pageAddress, String type, int count) {
        int i = hash(pageAddress);
        st[i].insertAtTail(pageAddress, type, count);
    } 

    // Removes the specified page address with its associated access type and access count from the symbol table
    public void delete(long pageAddress) {
        int i = hash(pageAddress);
        st[i].delete(pageAddress);
    }
}
