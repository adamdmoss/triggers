/* AdamTriggers Module

Copyright (C) 2004-2019 by Adam D. Moss <c@yotes.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
