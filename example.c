#include <stdlib.h>
#include <stdio.h>

#include "triggers.h"


static LFUNC_RTN
my_listener_callback(LFUNC_PARAM)
{
  printf("Got event '%c%c%c%c' with event data %p\tand listener data %p\n",
	 eventname[0], eventname[1], eventname[2], eventname[3],
	 eventdata, listener_data);
}


int
main(int in_argc, char **in_argv) {
  Listener *listener  = listenerNew();
  Listener *listener2 = listenerNew();
  Listener *listener3 = listenerNew();
  Trigger  *trigger   = triggerNew();
  Trigger  *trigger2  = triggerNew();

  /* set up the same callback function of all three listeners */
  listenerSetFunction(listener,  my_listener_callback);
  listenerSetFunction(listener2, my_listener_callback);
  listenerSetFunction(listener3, my_listener_callback);

  /* set up each listener's data hook with a dummy pointer that will
     make the listeners easy to distinguish when the callback prints out
     the event data, since all the listeners are sharing the same callback. */
  listenerSetData(listener,  (void*)0x1111);
  listenerSetData(listener2, (void*)0x2222);
  listenerSetData(listener3, (void*)0x3333);

  /* listener3 should auto-free itself when the triggers are freed */
  listenerAllowAutoDelete(listener3, 1);

  /* only one instance each of 'yerf' and 'murr' should actually
     end up getting registered for listener. */
  triggerListen(trigger, "yerf", listener);
  triggerListen(trigger, "murr", listener);
  triggerListen(trigger, "yerf", listener); /* -- 2nd listen changes nothing */
  triggerListen(trigger, "murr", listener2);
  triggerListen(trigger2,"yerf", listener2);
  triggerListen(trigger, "murr", listener3);
  triggerListen(trigger2,"yerf", listener3);

  /* only 'murr' and 'yerf' events should reach the listener */
  triggerEvent(trigger, "erm.", NULL); /* -- no-one cares about 'erm.' */
  triggerEvent(trigger, "murr", (void*)0xAAAA); /* dummy pointer for testing */
  triggerEvent(trigger, "yerf", NULL);
  triggerEvent(trigger, "fuh!", NULL);

  /* 'yerf' events will no longer be listened for */
  triggerUnlisten(trigger, "KWAH", listener); /* -- changes nothing */
  triggerUnlisten(trigger, "yerf", listener);

  /* so only 'murr' events should reach the listener */
  triggerEvent(trigger, "erm.", NULL);
  triggerEvent(trigger, "murr", NULL);
  triggerEvent(trigger, "yerf", NULL);
  triggerEvent(trigger, "fuh!", NULL);
  triggerEvent(trigger2,"yerf", NULL);

  /* should be able to delete listener and trigger in any order
     without leaving dangling pointers, since they are aware of
     each others' deletions and sanitize themselves appropriately. */
  listenerDelete(listener);

  /* no events should attempt to reach the (deleted) listener */
  triggerEvent(trigger, "erm.", NULL);
  triggerEvent(trigger, "murr", NULL);
  triggerEvent(trigger, "yerf", NULL);
  triggerEvent(trigger, "fuh!", NULL);  

  triggerDelete(trigger);
  triggerDelete(trigger2);

  /* listener3 should have auto-deleted now */

  listenerDelete(listener2);

  printf("Trigger tests didn't crash.  :D\n");

  return 0;
}
