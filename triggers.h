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
#ifndef TRIGGERS_H
#define TRIGGERS_H

/* note: Only the first four characters of event names are significant */

/* Max number of unique event types to recognise per trigger (1..256)
   (tunable for space usage versus speed) */
#define TRIGGER_TABLE_SIZE 256

/* some event types which the trigger system uses internally (if you
   change these then re-compile the trigger module as well as your
   own code that cares). */
#define TRIGGER_DELETION_EVENT_NAME      "_TDe"
#define LISTENER_DELETION_EVENT_NAME     "_LDe"
#define LISTENER_AUTODELETION_EVENT_NAME "_LAu"


/* trigger/listener structures */

typedef struct _Trigger Trigger;

#define LFUNC_RTN   void
#define LFUNC_PARAM const char *const eventname, \
		    const void *const eventdata, \
		    void *const listener_data
typedef LFUNC_RTN (LFunction)(LFUNC_PARAM);

typedef struct {
  unsigned short int num_triggers;
  unsigned short int allocated_triggers;
  Trigger** triggers;

  LFunction* receptor_func;
  void *data; /* hook for listener-specific data */

  char auto_delete;
} Listener;

struct _Trigger {
  struct {
    char name[4];
    unsigned short int num_listeners;
    unsigned short int allocated_listeners;
    Listener** listeners;
  } event[TRIGGER_TABLE_SIZE];
};


/* methods */

Listener* listenerNew(void);
void listenerInit(Listener *const listener);
void listenerDelete(Listener *const listener);
void listenerDeleteInner(Listener *const listener);
Listener* listenerSetFunction(Listener *const listener,
			 LFunction *const func);
Listener* listenerSetData(Listener *const listener,
			  void *const listener_data);
void* listenerGetData(Listener *const listener);
Listener* listenerAllowAutoDelete(Listener *const listener,
				  const int free_on_delete);

int listenertriggerEventNameIsPrivate(const char *const eventname);

/* very simple utility macro */
#define listenerNewWithFunc(func) listenerSetFunction(listenerNew(), (func))

Trigger* triggerNew(void);
void triggerDelete(Trigger *const trigger);

void triggerListen(Trigger *const trigger,
		   const char *const eventname,
		   Listener *const listener);
int triggerUnlisten(Trigger *const trigger,
                    const char *const eventname,
                    Listener *const listener); /* return 1/0 on success/fail */

void triggerEvent(Trigger *const trigger,
		  const char *const eventname,
		  const void *const eventdata);

#endif
