/*
 * $Id$
 */

#ifndef EVENT_H
#define EVENT_H

#include <list>
#include <string>
#include "types.h"

using std::string;

#define U4_UP           '['
#define U4_DOWN         '/'
#define U4_LEFT         ';'
#define U4_RIGHT        '\''
#define U4_BACKSPACE    8
#define U4_TAB          9
#define U4_SPACE        ' '
#define U4_ESC          27
#define U4_ENTER        13
#define U4_ALT          128
#define U4_META         323
#define U4_FKEY         282
#define U4_RIGHT_SHIFT  303
#define U4_LEFT_SHIFT   304
#define U4_RIGHT_CTRL   305
#define U4_LEFT_CTRL    306
#define U4_RIGHT_ALT    307
#define U4_LEFT_ALT     308
#define U4_RIGHT_META   309
#define U4_LEFT_META    310

extern int eventTimerGranularity;

struct _MouseArea;

/**
 * A class for handling keystrokes. 
 */
class KeyHandler {
public:
    /* Typedefs */
    typedef bool (*Callback)(int, void*);

    /** Additional information to be passed as data param for read buffer key handler */
    typedef struct ReadBuffer {        
        int (*handleBuffer)(string*);
        string *buffer;
        int bufferLen;
        int screenX, screenY;
    } ReadBuffer;

    /** Additional information to be passed as data param for get choice key handler */
    typedef struct GetChoice {
        string choices;
        int (*handleChoice)(int);
    } GetChoice;

    /* Constructors */
    KeyHandler(Callback func, void *data = NULL, bool asyncronous = true);
    
    /* Static functions */    
    static int setKeyRepeat(int delay, int interval);
    static bool globalHandler(int key);    

    /* Static default key handler functions */
    static bool defaultHandler(int key, void *data);
    static bool ignoreKeys(int key, void *data);

    /* Operators */
    bool operator==(Callback cb) const;
    
    /* Member functions */    
    bool handle(int key); 
    virtual bool isKeyIgnored(int key);

protected:
    Callback handler;
    bool async;
    void *data;
};

/**
 * A class for handling timed events.
 */ 
class TimedEvent {
public:
    /* Typedefs */
    typedef std::list<TimedEvent*> List;
    typedef void (*Callback)(void *);

    /* Constructors */
    TimedEvent(Callback callback, int interval, void *data = NULL);

    /* Member functions */
    Callback getCallback() const;
    void *getData();
    void tick();
    
    /* Properties */
protected:    
    Callback callback;
    void *data;
    int interval;
    int current;
};

/**
 * A class for managing timed events
 */ 
class TimedEventMgr {
public:
    /* Typedefs */
    typedef TimedEvent::List List;    

    /* Constructors */
    TimedEventMgr(int baseInterval);
    ~TimedEventMgr();

    /* Static functions */
    static unsigned int callback(unsigned int interval, void *param);

    /* Member functions */
    bool isLocked() const;      /**< Returns true if the event list is locked (in use) */    

    void add(TimedEvent::Callback callback, int interval, void *data = NULL);
    List::iterator remove(List::iterator i);                          
    void remove(TimedEvent* event);
    void remove(TimedEvent::Callback callback, void *data = NULL);
    void tick();
    
    void reset(unsigned int interval);     /**< Re-initializes the event manager to a new base interval */

private:
    void lock();                /**< Locks the event list */
    void unlock();              /**< Unlocks the event list */

    /* Properties */
protected:
    /* Static properties */
    static unsigned int instances;

    void *id;
    int baseInterval;
    bool locked;
    List events;
    List deferredRemovals;
};


/**
 * A class for handling game events. 
 */
class EventHandler {
public:    
    /* Typedefs */
    typedef std::list<KeyHandler*> KeyHandlerList;
    typedef std::list<_MouseArea*> MouseAreaList;    

    /* Constructors */
    EventHandler();    

    /* Static functions */    
    static void sleep(unsigned int usec);    
    static void setExitFlag(bool exit = true);
    static bool getExitFlag();
    static bool timerQueueEmpty();

    /* Member functions */
    TimedEventMgr* getTimer();

    /* Event functions */
    void main(void (*updateScreen)(void));    

    /* Key handler functions */
    void pushKeyHandler(KeyHandler kh);
    KeyHandler *popKeyHandler();
    KeyHandler *getKeyHandler() const;
    void setKeyHandler(KeyHandler kh);

    /* Mouse area functions */
    void pushMouseAreaSet(_MouseArea *mouseAreas);
    void popMouseAreaSet();
    _MouseArea* getMouseAreaSet() const;
    _MouseArea* mouseAreaForPoint(int x, int y);

protected:    
    static bool exit;
    TimedEventMgr timer;
    KeyHandlerList keyHandlers;
    MouseAreaList mouseAreaSets;
};

bool keyHandlerGetChoice(int key, void *data);
bool keyHandlerReadBuffer(int key, void *data);

extern EventHandler eventHandler;

#endif
