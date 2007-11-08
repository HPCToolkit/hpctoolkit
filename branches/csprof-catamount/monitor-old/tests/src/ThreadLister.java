// This example is from _Java Examples in a Nutshell_. (http://www.oreilly.com)
// Copyright (c) 1997 by David Flanagan
// This example is provided WITHOUT ANY WARRANTY either expressed or implied.
// You may study, use, modify, and distribute it for non-commercial purposes.
// For any commercial use, see http://www.davidflanagan.com/javaexamples

import java.io.*;

/**
 * This class contains a useful static method for listing all threads
 * and threadgroups in the VM.  It also has a simple main() method so it
 * can be run as a standalone program.
 **/
public class ThreadLister {
  /** Display information about a thread. */
  private static void printThreadInfo(PrintWriter out, Thread t, 
                                        String indent) {
    if (t == null) return;
    out.println(indent + "Thread: " + t.getName() +
                "  Priority: " + t.getPriority() +
                (t.isDaemon()?" Daemon":"") +
                (t.isAlive()?"":" Not Alive"));
  }
  
  /** Display info about a thread group and its threads and groups */
  private static void printGroupInfo(PrintWriter out, ThreadGroup g, 
                                 String indent) {
    if (g == null) return;
    int num_threads = g.activeCount();
    int num_groups = g.activeGroupCount();
    Thread[] threads = new Thread[num_threads];
    ThreadGroup[] groups = new ThreadGroup[num_groups];
    
    g.enumerate(threads, false);
    g.enumerate(groups, false);
    
    out.println(indent + "Thread Group: " + g.getName() + 
                "  Max Priority: " + g.getMaxPriority() +
                (g.isDaemon()?" Daemon":""));
    
    for(int i = 0; i < num_threads; i++)
      printThreadInfo(out, threads[i], indent + "    ");
    for(int i = 0; i < num_groups; i++)
      printGroupInfo(out, groups[i], indent + "    ");
  }
  
  /** Find the root thread group and list it recursively */
  public static void listAllThreads(PrintWriter out) {
    ThreadGroup current_thread_group;
    ThreadGroup root_thread_group;
    ThreadGroup parent;
    
    // Get the current thread group
    current_thread_group = Thread.currentThread().getThreadGroup();
    
    // Now go find the root thread group
    root_thread_group = current_thread_group;
    parent = root_thread_group.getParent();
    while(parent != null) {
      root_thread_group = parent;
      parent = parent.getParent();
    }
    
    // And list it, recursively
    printGroupInfo(out, root_thread_group, "");
  }
  
  /**
   * The main() method:  just print the list of threads to the console
   **/
  public static void main(String[] args) {
    PrintWriter out = new PrintWriter(new OutputStreamWriter(System.out));
    ThreadLister.listAllThreads(out);
    out.flush();
  }
}
