public class Frame {

  private long pageAddress;
  private String type;
  private int count;
  private Frame prev, next;


  public Frame(long pageAddress, String type, int count)
  {
    this(pageAddress, type, count, null, null);
  }
  
  public Frame(long pageAddress, String type, int count, Frame prev, Frame next)
  {
    setpageAddress( pageAddress );
    setType( type );
    setCount(count);
    setPrev(prev);
    setNext(next);
  }

  public void setpageAddress(long pageAddress)
  {
    this.pageAddress = pageAddress;
  }

  public void setType(String type)
  {
    this.type = type;
  }

  public void setCount(int count) 
  {
      this.count = count;
  }

  public void setPrev(Frame prev)
  {
     this.prev = prev;
  }

  public void setNext(Frame next)
  {
     this.next = next;
  }

  public long getpageAddress()
  {
    return pageAddress;
  }

  public int getCount()
  {
      return count;
  }

  public String getType() 
  {
    return type;
  }

  public Frame getPrev() 
  {
    return prev;
  }

  public Frame getNext() 
  {
    return next;
  }
}