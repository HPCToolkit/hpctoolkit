/* CS 1520 Spring 2005 Java Example 15
   Demo of using threads with Runnable objects, of the shared dataspace
   accessed by threads and how threads can be switched in and out by the
   system.  Note when running this that the data and output statements are
   not consistent -- this is because a Thread in its "critical area" can
   be switched out before it is done processing.
*/

class MyThread extends Thread  // extending the Thread class
{
    int id;      // id number for thread

    public MyThread(int id, Runnable item)
    {
        super(item);  // Java Threads can have their own run() methods, or they
                      // can be linked to the run() method of any Runnable
             // object.  In this case, when the Thread is start()ed, it begins
             // execution of the run() method in the object it is linked to,
             // using all of the variables as defined in that object.  If more
             // than one Thread is linked to the same Runnable object, then
             // the data will be shared and we must be concerned about data
             // consistency.

        this.id = id;
    }

    public String toString() // allow Thread id to be printed out
    {
        return (new Integer(id).toString());
    }
}

class RunDemo implements Runnable   // the Runnable interface in Java allows
                                    // a class to become part of a thread (or to be
                            // threaded).  It simply means the class must implement
                            // the method run().
{
    int value;
    boolean active;

    public RunDemo()
    {
        value = 0;
        active = true;
    }

    public void deactivate()
    {
        active = false;
    }

    // It is important to realize that if many Threads share the same Runnable()
    // object, then each can change the data within the object.  Also, since
    // threads can be blocked due to I/O or simply switched out by the scheduler
    // to allow other threads to have a chance to execute (when time-
    // slicing is used), we should not count on any specific number of
    // instructions being executed while a thread is active.  We will see shortly
    // how to ensure consistency using synchronization.

    public void run()   // this method will be implicitly called when the
    {                   // thread linked to it is started.  If more than one
           // thread is linked to the same Runnable object, then each thread will
           // be executing the run() method "in parallel".  When the method
           // completes execution for a given thread, the thread terminates.

        while (active)
        {
              MyThread currThread = (MyThread) Thread.currentThread();
                        // static method Thread.currentThread() is useful since
                        // it returns a reference to the current executing
                        // Thread.  If many threads share the same Runnable
                        // object, it may be important to determine which thread
                        // is actually running at any given time.
              System.out.println("Thread = " + currThread + " Value = " + value
                                             + " about to increment");
              value++;

              int i = 1;
              while (i < 30000000)      // Lots of spinning to simulate "work"
              {                         // that is done within the run() method
                     i++;               // Note in the output that a thread may
              }                         // be switched out at any time

              currThread = (MyThread) Thread.currentThread();
              System.out.println("Thread = " + currThread + " Value = " + value
                                             + " done incrementing");
        }
    }
}

public class threads10
{
    static int getNumThreads() // get an int with error checking
    {                          // using JOptionPane
         int val = 10;
         return val;
    }

    public static void main(String [] argv)
    {
         int num_threads;
         num_threads = getNumThreads();
         System.out.println("There will be " + num_threads + " threads");

         MyThread [] T = new MyThread[num_threads];  // array of MyThread
                       // created, but no actual MyThreads exist yet
                       // (since it is an array of references)

         RunDemo MyDemo = new RunDemo(); // create Runnable object
         for (int i = 0; i < T.length; i++)         // note that each new
         {                                          // Thread receives a ref. to
                T[i] = new MyThread(i, MyDemo);     // the same RunDemo object.
                T[i].start();                       // When the Thread is
                                                    // started, the run() method in
                     // the argument object is called.  However,
                     // since my PC has only one CPU, only one thread can
                     // actually "run" at a time...a scheduler handles this
                     // using "time-slicing" on my PC.
         }
         try
         {
               Thread.sleep(5000);    // Have the main sleep a bit to let the
                                     // other threads do their work
         }
         catch (InterruptedException e)
         {
               System.out.println("Hey, who woke me? ");
         }

         System.out.println("Deactivating Threads now");

         MyDemo.deactivate();         // Signal to MyDemo to allow run() to
                                      // terminate.  This will cause all of
                   // the threads linked to MyDemo to terminate, but only after
                   // they spin to the beginning of the run() method loop.

	 for (int i = 0; i < T.length; i++)         
         {                                          
	     try
		 {
		     T[i].join();                       
		 }
	     catch (InterruptedException e)
		 {
		     System.out.println("Hey, who woke me? ");
		 }
	 }

         System.exit(0);        // Needed since JOptionPane windows are used
         						// This should only really be executed when it is
		                        // certain that all Threads have stopped.  However,
		                        // for this example I'm just waiting a second and
                          		// assuming the Threads have terminated
    }
}
