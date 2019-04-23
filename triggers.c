/* AdamTriggers Module -- contact aspirin@icculus.org

Copyright (C) 2002-2004 Adam D. Moss (the "Author").  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the Author of the
Software shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written authorization
from the Author.
*/

/*
  AdamTriggers v0.85.2 - 2004-07-30

  2004-07-30: v0.85.2
  - listenertriggerEventNameIsPrivate() function added
  - listenerGetData() function added
  - reserved event names prefixed with '_' instead of '/'
  - TRIGGER_WARNINGS for the more important debug/sanity messages

  2002-04-05: v0.85.1
  - listenerAllowAutoDelete() takes a flag indicating whether auto-deletion
    implies that the triggers system will free the listener's memory, or
    whether the listener gets told of its auto-deletion and handles its
    deletion manually.
  - Some cleanup.
  - Corrected my contact email address

  2002-11-05: v0.85.0
  - Initial pre-release
 */

/* Uncomment this to make the trigger module verbose on stderr about
   what it's doing.  Really meant for debugging the module itself, but
   may be useful when learning to use the module to see what's happening. */
/* #define TRIGGER_DEBUG */
/* This enables only a subset of the messages of TRIGGER_DEBUG: these are
   warnings about unexpected events or potentially dangerous usages. */
#define TRIGGER_WARNINGS

/**********************************************************************/
#ifdef TRIGGER_DEBUG
#define TRIGGER_WARNINGS
#endif /* TRIGGER_DEBUG */

#include <stdlib.h>
#include <string.h>

#include "triggers.h"

#ifdef TRIGGER_DEBUG
#include <stdio.h>
#endif


static void
listener_disregard_trigger(Listener *const listener,
			   const Trigger *const trigger);
static void
listener_really_delete_inner(Listener *const listener);

/****************************************************/

/* function to return whether two 4-character strings are equal. */
static int
eventname_equals(const char *const str1,
		 const char *const str2)
{
  /* This can get called a lot, so it should be fast.  There are three
     implementations here -- uncomment the one that's fastest for you. */

  /* normalized timing (smaller is better) on x86 with gcc3.1.1 -
     implementation :     A      B      C
     equal strings  :   100     64     62
     inequal strings:   100     87     73
  */


  /* Option A -- if your compiler does an excellent inline memcmp() then
     this might be a competitive option.  That is not true for gcc <= 3.1.1 */
  /*
  return (0 == memcmp(str1, str2, 4));
  */


  /* Option B -- pretty good on gcc 3.1.1 */
  return ( (str1[0]==str2[0]) && (str1[1]==str2[1]) &&
	   (str1[2]==str2[2]) && (str1[3]==str2[3]) );
    

  /* Option C -- this is rather faster still, but assumes that the target
     architecture has exactly 32-bit ints and doesn't bus-fault on misaligned
     int reads.  Fine on x86, terminally bad for a lot of other machines. */
  /*
    return (*(int*)str1 == *(int*)str2);
  */
}


Trigger*
triggerNew(void)
{
  int i;
  Trigger *rtn = malloc(sizeof(Trigger));

  for (i=0; i<TRIGGER_TABLE_SIZE; ++i) {
    rtn->event[i].name[0] =
      rtn->event[i].name[1] = '\0';
    rtn->event[i].num_listeners = 0;
    rtn->event[i].allocated_listeners = 0;
    rtn->event[i].listeners = NULL;
  }

  return rtn;
}


static void
trigger_free(Trigger *const trigger)
{
  int i;

  for (i=0; i<TRIGGER_TABLE_SIZE; ++i) {
    /* free listener-list */
    if (0 != trigger->event[i].allocated_listeners) {
      free(trigger->event[i].listeners);
    }
  }

#ifdef TRIGGER_DEBUG
  fprintf(stderr, "(FREEING TRIGGER %p) ", trigger);
#endif
  free(trigger);
}


void 
triggerDelete(Trigger *const trigger)
{
  /* tell all listeners that this trigger is being deleted, so they
     can remove their own mutual notification link.
  */
  triggerEvent(trigger, TRIGGER_DELETION_EVENT_NAME, trigger);

  /* physically delete the trigger structure and data */
  trigger_free(trigger);
}


static unsigned char
name32_to_hash_key8(const char *const name)
{
  return
    ((name[0]     )                 ) ^
    ((name[1] >> 2) | (name[1] << 6)) ^
    ((name[2] >> 4) | (name[2] << 4)) ^
    ((name[3] >> 6) | (name[3] << 2));    
}


/* returns 0/1 depending on whether this event name was found in the Trigger.
   Sets 'index' to the slot in which the name was found, or the nearest
   vacant slot after that name's expected location if it was not found at
   all, or to -1 if the event name was not found and the table is full.
 */
static int
find_name_slot(const Trigger *const trigger,
	       const char *const eventname,
	       int *const index)
{
  int i;
  int vacant_index = -1; /* if table is full, this -1 will persist */
  const unsigned char key =
    name32_to_hash_key8(eventname) % TRIGGER_TABLE_SIZE;

  for (i=key; i<key+TRIGGER_TABLE_SIZE; ++i) {
    if (eventname_equals(trigger->event[i%TRIGGER_TABLE_SIZE].name,
			 eventname)) {
      *index = i%TRIGGER_TABLE_SIZE;
      return 1; /* found eventname at this index */
    }

    if (-1 == vacant_index &&
	'\0' == trigger->event[i%TRIGGER_TABLE_SIZE].name[0]) {
      /* note first vacant slot after expected slot */
      vacant_index = i%TRIGGER_TABLE_SIZE;
    }
  }

#ifdef TRIGGER_WARNING
  if (vacant_index == -1) {
    fprintf(stderr, "\nTRIGGER'S EVENT TABLE IS FULL!\n");
  }
#endif
  *index = vacant_index;
  return 0; /* eventname not in table */
}

static void
send_event_to_listener(Listener *const listener,
		       const char *const eventname,
		       const void *const eventdata)
{
#ifdef TRIGGER_DEBUG
  fprintf(stderr, "{EV:\"%c%c%c%c\" -> L%p:D%p}\n",
	  eventname[0], eventname[1], eventname[2], eventname[3],
	  listener, eventdata);
#endif

  /* non-negotiable! */
  if (eventname_equals(TRIGGER_DELETION_EVENT_NAME, eventname)) {
    listener_disregard_trigger(listener, eventdata);
    return;
  }

  if (NULL != listener->receptor_func) {
    listener->receptor_func(eventname, eventdata, listener->data);
  }
}


void
triggerEvent(Trigger *const trigger,
	     const char *const eventname,
	     const void *const eventdata)
{
  int i;
  int index;

#ifdef TRIGGER_DEBUG
  /*
  fprintf(stderr, "[?%p/\"%c%c%c%c\"<-%p]\n",
	  trigger,
	  eventname[0], eventname[1], eventname[2], eventname[3],
	  eventdata);
  */
#endif

  if (0 == find_name_slot(trigger, eventname, &index)) {
    /* no-one is listening for this event type */
    return;
  }

  /* iterate through our possibly-sparse listener list for this event
     type, sending the event to each listener. */
  for (i=0; i<trigger->event[index].allocated_listeners; ++i) {
    if (NULL != trigger->event[index].listeners[i]) {
      send_event_to_listener(trigger->event[index].listeners[i],
			     eventname, eventdata);
    }
  }
}


static void
trigger_add_listener(Trigger *const trigger,
		     const char *const eventname,
		     Listener *const listener)
{
  int index;

  if (0 == find_name_slot(trigger, eventname, &index)) {
    /* this event type does not yet exist on the trigger; create it,
       add the listener */

    if (index == -1) {
      /* trigger event table is full, so we can't actually add a listener
	 for this event.  :(   return... */
#ifdef TRIGGER_WARNING
      fprintf(stderr, "Event table full, so could not add listener.\n");
#endif
      return;
    }

    memcpy(trigger->event[index].name, eventname, 4);
    trigger->event[index].num_listeners       = 1;
    trigger->event[index].allocated_listeners = 1;
    trigger->event[index].listeners           = malloc(sizeof(Listener*));
    trigger->event[index].listeners[0]        = listener;
  } else {
    /* event type exists; add listener to the listener list for
       that event type if that listener is not already in there. */
    int i;
    int free_listener_slot = -1;
    i = trigger->event[index].allocated_listeners;
    while (i--) {
      if (trigger->event[index].listeners[i] == listener) {
	/* this listener is already registered for this event, so return */
	return;
      }
      if (NULL == trigger->event[index].listeners[i]) {
	free_listener_slot = i;
      }
    }
    if (free_listener_slot > -1) {
      /* we can just plonk the listener in an allocated-but-spare slot
	 and return */
      trigger->event[index].listeners[free_listener_slot] = listener;
    } else {
      /* have to expand the listener list and put the listener at the end */
      ++trigger->event[index].allocated_listeners;
      trigger->event[index].listeners =
	realloc(trigger->event[index].listeners,
		sizeof(Listener*) * trigger->event[index].allocated_listeners);
      trigger->event[index].listeners
	[trigger->event[index].allocated_listeners - 1] = listener;
    }
    ++trigger->event[index].num_listeners;
  }

}


static void
listener_add_trigger(Listener *listener,
		     Trigger *trigger)
{
  if (0 == listener->allocated_triggers) {
    /* first trigger that listener is interested in */
    listener->allocated_triggers = 1;
    listener->num_triggers = 1;
    listener->triggers =
      malloc(sizeof(Trigger*) * listener->allocated_triggers);
    listener->triggers[0] = trigger;
  } else {
    /* see if we're already noting this trigger, otherwise find
       an empty slot */
    int i;
    int spare_index = -1;
    for (i=0; i<listener->allocated_triggers; ++i) {
      if (listener->triggers[i] == trigger) {
	/* we're already tracking this trigger. */
	return;
      }
      if (NULL == listener->triggers[i]) {
	spare_index = i;
      }
    }
    if (spare_index > -1) {
      /* already have a slot to drop the trigger into */
      listener->triggers[spare_index] = trigger;
    } else {
      /* have to expand trigger list and insert the trigger */
      ++listener->allocated_triggers;
      listener->triggers =
	realloc(listener->triggers,
		sizeof(Trigger*) * listener->allocated_triggers);
      listener->triggers[listener->allocated_triggers - 1] = trigger;
    }

    ++listener->num_triggers;
  }

}


static void
trigger_remove_listenerlist_index(Trigger *trigger,
				  int slot,
				  int listindex)
{
#ifdef TRIGGER_DEBUG
  if (NULL == trigger->event[slot].listeners[listindex]) {
    fprintf(stderr, "trigger_remove_listenerlist_index: trigger->event[slot].listeners[listindex] was already NULL.\n");
  }
#endif

  trigger->event[slot].listeners[listindex] = NULL;
  --trigger->event[slot].num_listeners;
  /* if we just removed the last listener for this event type then
     delete this event slot. */
  if (0 == trigger->event[slot].num_listeners) {
#ifdef TRIGGER_DEBUG
    fprintf(stderr, " - removed last listener for %c%c%c%c\n",
	    trigger->event[slot].name[0],
	    trigger->event[slot].name[1],
	    trigger->event[slot].name[2],
	    trigger->event[slot].name[3]
	    );
#endif
    trigger->event[slot].name[0] = '\0';
    trigger->event[slot].allocated_listeners = 0;
    free(trigger->event[slot].listeners);
    trigger->event[slot].listeners = NULL;
  }
}


int
triggerUnlisten(Trigger *const trigger,
		const char *const eventname,
		Listener *const listener)
{
  int index;

  if (0 == find_name_slot(trigger, eventname, &index)) {
    /* this event type does not exist on the trigger; return */
    return 0;
  } else {
    int i = trigger->event[index].allocated_listeners;
    while (i--) {
      if (trigger->event[index].listeners[i] == listener) {
	trigger_remove_listenerlist_index(trigger, index, i);
        return 1;
      }
    }
  }
  return 0;
}


void
triggerListen(Trigger *const trigger,
	      const char *const eventname,
	      Listener *const listener)
{
  /* automatically make the trigger inform the listener about trigger
     deletion, for housekeeping. */
  trigger_add_listener(trigger, TRIGGER_DELETION_EVENT_NAME, listener);

  /* conversely, make the listener remember triggers that it is interested
     in so it can tell them if it gets deleted. */
  listener_add_trigger(listener, trigger);

  /* now do the explicitly-requested event listener registration */
  trigger_add_listener(trigger, eventname, listener);
}


/* make trigger forget all references to the given listener */
static void
trigger_disregard_listener(Trigger *trigger,
			   Listener *listener)
{
  int i;
  for (i=0; i<TRIGGER_TABLE_SIZE; ++i) {
    if ('\0' != trigger->event[i].name[0]) {
      int s;
      for (s=0; s<trigger->event[i].allocated_listeners; ++s) {
	/*fprintf(stderr, "%d:%d ", i, s);*/
	if (trigger->event[i].listeners[s] == listener) {
	  trigger_remove_listenerlist_index(trigger, i, s);
	  /* can stop searching list since listeners are unique per event */
	  break;
	}
      }
    }
  }
}


/****************************************************/

static void
listener_disregard_trigger(Listener *const listener,
			   const Trigger *const trigger)
{
  /* Look for the trigger in the listener's list.  (it should
     be there!)  When we find it, remove it. */
  if (listener->num_triggers) {
    int i;
    for (i=0; i<listener->allocated_triggers; ++i) {
      if (listener->triggers[i] == trigger) {
	listener->triggers[i] = NULL;
	--listener->num_triggers;
	/* if we just removed the last trigger from the list then
	   free the list altogether. */
	if (0 == listener->num_triggers) {
	  listener->allocated_triggers = 0;
	  free(listener->triggers);
	  listener->triggers = NULL;

	  /* ...and if this is an auto-delete listener then removal of
	     the last trigger means that the listener should now be
	     freed! */
	  /* 2 == free_on_delete, we'll just free the listener and
	     that's that. */
	  if (listener->auto_delete == 2) {
#ifdef TRIGGER_DEBUG
	    fprintf(stderr, "{auto-deleting innerlistener %p} ", listener);
#endif
	    listener_really_delete_inner(listener);
	  } else
	    /* 1 == !free_on_delete, so the listener gets sent the
	       LISTENER_AUTODELETION_EVENT_NAME event with itself as payload
	       and is in charge of its own deletion. */
	    if (listener->auto_delete == 1) {
#ifdef TRIGGER_DEBUG
	      fprintf(stderr, "{sending auto-delete signal to listener %p} ", listener);
#endif
	      send_event_to_listener(listener, LISTENER_AUTODELETION_EVENT_NAME,
				     listener);
#ifdef TRIGGER_DEBUG
	      fprintf(stderr, "{done} ");
#endif
	    }
	}
	break;
      }
    }
  }
}

/****************************************************/

void
listenerInit(Listener *const listener)
{
  listener->num_triggers =
    listener->allocated_triggers = 0;
  listener->triggers = NULL;
  
  listener->receptor_func = NULL;
  listener->data = NULL;
  listener->auto_delete = 0;
}

Listener*
listenerNew(void)
{
  Listener* rtn = malloc(sizeof(Listener));

  listenerInit(rtn);

  return rtn;
}


static void
listener_really_delete_inner(Listener *const listener)
{
  /* First, send the listener its destructor event so it can do what it
     needs to free up its 'data' hook if desired. */
  send_event_to_listener(listener, LISTENER_DELETION_EVENT_NAME,
			 listener->data);
  listener->data = NULL;

  /* tell all triggers which we've registered with to forget about us. */
  if (listener->num_triggers) {
    int i;
    for (i=0; i<listener->allocated_triggers; ++i) {
      if (listener->triggers[i]) {
	trigger_disregard_listener(listener->triggers[i],
				   listener);
      }
    }
  }

  if (listener->allocated_triggers) {
    free(listener->triggers);
  }
}


void
listenerDeleteInner(Listener *const listener)
{
#ifdef TRIGGER_WARNING
  /* If this is supposed to be an auto-delete listener then it's usually
     bad practise to explicitly delete it because you have to be very
     sure that the listener has not outlived its triggers --
     otherwise you might be messing with a listener that has already
     auto-deleted itself and that's really hard to detect.  It's better
     to let an auto-delete listener live on until it's automatically deleted
     when all the triggers it is watching are explicitly deleted.  That's
     what it's there for.

     So, issue a warning if trying to explicitly delete an auto-delete
     listener.
  */
  if (listener->auto_delete) {
    fprintf(stderr,
	    "Triggers: Warning -- trying to explicitly delete an auto-delete\n"
	    "listener (%p).  You'd better be very confident that some of the\n"
	    "triggers it is watching are still alive and hence it has not\n"
	    "already been automatically deleted!\n", listener);
  }
#endif

  listener_really_delete_inner(listener);
}


void
listenerDelete(Listener *const listener)
{
  listenerDeleteInner(listener);
  
#ifdef TRIGGER_DEBUG
  fprintf(stderr, "(FREEING LISTENER %p) \n", listener);
#endif
  free(listener);
}


Listener*
listenerAllowAutoDelete(Listener *const listener,
			const int free_on_delete)
{
  if (free_on_delete)
    listener->auto_delete = 2;
  else
    listener->auto_delete = 1;
  return listener;
}


int
listenertriggerEventNameIsPrivate(const char *const eventname) {
  return eventname_equals(eventname, LISTENER_DELETION_EVENT_NAME)
    || eventname_equals(eventname, LISTENER_AUTODELETION_EVENT_NAME)
    || eventname_equals(eventname, TRIGGER_DELETION_EVENT_NAME);
}


Listener*
listenerSetFunction(Listener *const listener,
		    LFunction *const func)
{
  listener->receptor_func = func;
  return listener;
}

Listener*
listenerSetData(Listener *const listener,
		void *const listener_data)
{
  listener->data = listener_data;
  return listener;
}

void*
listenerGetData(Listener *const listener)
{
  return listener->data;
}
