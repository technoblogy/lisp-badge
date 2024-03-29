/* Lisp Badge - uLisp 4.3
   David Johnson-Davies - www.technoblogy.com - 22nd September 2022

   Licensed under the MIT license: https://opensource.org/licenses/MIT
*/

// Lisp Library
const char LispLibrary[] PROGMEM = "";

// Compile options

#define checkoverflow
// #define resetautorun
#define printfreespace
#define serialmonitor
// #define printgcs
// #define sdcardsupport
// #define lisplibrary

// Includes

// #include "LispLibrary.h"
#include <avr/sleep.h>
#include <setjmp.h>
#include <SPI.h>
#include <limits.h>
#include <EEPROM.h>

#if defined(sdcardsupport)
#include <SD.h>
#define SDSIZE 172
#else
#define SDSIZE 0
#endif

// Workspace - sizes in bytes
#define WORDALIGNED __attribute__((aligned (2)))
#define OBJECTALIGNED __attribute__((aligned (4)))
#define BUFFERSIZE 21                     /* longest builtin name + 1 */

#if defined(__AVR_ATmega1284P__)
  #include "optiboot.h"
  #define WORKSPACESIZE (2848-SDSIZE)     /* Objects (4*bytes) */
//  #define EEPROMSIZE 4096                 /* Bytes */
  #define FLASHWRITESIZE 16384            /* Bytes */
  #define CODESIZE 96                     /* Bytes <= 256 */
  #define STACKDIFF 320
  #define CPU_ATmega1284P

#else
#error "Board not supported!"
#endif

// C Macros

#define nil                NULL
#define car(x)             (((object *) (x))->car)
#define cdr(x)             (((object *) (x))->cdr)

#define first(x)           (((object *) (x))->car)
#define second(x)          (car(cdr(x)))
#define cddr(x)            (cdr(cdr(x)))
#define third(x)           (car(cdr(cdr(x))))

#define push(x, y)         ((y) = cons((x),(y)))
#define pop(y)             ((y) = cdr(y))

#define integerp(x)        ((x) != NULL && (x)->type == NUMBER)
#define symbolp(x)         ((x) != NULL && (x)->type == SYMBOL)
#define stringp(x)         ((x) != NULL && (x)->type == STRING)
#define characterp(x)      ((x) != NULL && (x)->type == CHARACTER)
#define arrayp(x)          ((x) != NULL && (x)->type == ARRAY)
#define streamp(x)         ((x) != NULL && (x)->type == STREAM)

#define mark(x)            (car(x) = (object *)(((uintptr_t)(car(x))) | MARKBIT))
#define unmark(x)          (car(x) = (object *)(((uintptr_t)(car(x))) & ~MARKBIT))
#define marked(x)          ((((uintptr_t)(car(x))) & MARKBIT) != 0)
#define MARKBIT            1

#define setflag(x)         (Flags = Flags | 1<<(x))
#define clrflag(x)         (Flags = Flags & ~(1<<(x)))
#define tstflag(x)         (Flags & 1<<(x))

#define issp(x)            (x == ' ' || x == '\n' || x == '\r' || x == '\t')
#define longsymbolp(x)     (((x)->name & 0x03) == 0)
#define twist(x)           ((uint16_t)((x)<<2) | (((x) & 0xC000)>>14))
#define untwist(x)         (((x)>>2 & 0x3FFF) | ((x) & 0x03)<<14)
#define PACKEDS            17600
#define BUILTINS           64000

// Code marker stores start and end of code block (max 256 bytes)
#define startblock(x)      ((x->integer) & 0xFF)
#define endblock(x)        ((x->integer) >> 8 & 0xFF)

#define SDCARD_SS_PIN 10

#if defined(CPU_ATmega4809)
#define PROGMEM
#define PSTR(s) (s)
#endif

// Constants

const int TRACEMAX = 3; // Number of traced functions
enum type { ZZERO=0, SYMBOL=2, CODE=4, NUMBER=6, STREAM=8, CHARACTER=10, ARRAY=12, STRING=14, PAIR=16 };  // ARRAY STRING and PAIR must be last
enum token { UNUSED, BRA, KET, QUO, DOT };
enum stream { SERIALSTREAM, I2CSTREAM, SPISTREAM, SDSTREAM, STRINGSTREAM };

// Stream names used by printobject
const char serialstream[] PROGMEM = "serial";
const char i2cstream[] PROGMEM = "i2c";
const char spistream[] PROGMEM = "spi";
const char sdstream[] PROGMEM = "sd";
PGM_P const streamname[] PROGMEM = {serialstream, i2cstream, spistream, sdstream};

// Typedefs

typedef unsigned int symbol_t;

typedef struct sobject {
  union {
    struct {
      sobject *car;
      sobject *cdr;
    };
    struct {
      unsigned int type;
      union {
        symbol_t name;
        int integer;
        int chars; // For strings
      };
    };
  };
} object;

typedef object *(*fn_ptr_type)(object *, object *);
typedef void (*mapfun_t)(object *, object **);
typedef int (*intfn_ptr_type)(int w, int x, int y, int z);

typedef const struct {
  const char *string;
  fn_ptr_type fptr;
  uint8_t minmax;
  const char *doc;
} tbl_entry_t;

typedef int (*gfun_t)();
typedef void (*pfun_t)(char);

enum builtin_t { NIL, TEE, NOTHING, OPTIONAL, INITIALELEMENT, ELEMENTTYPE, BIT, AMPREST, LAMBDA, LET,
LETSTAR, CLOSURE, PSTAR, SPECIAL_FORMS, QUOTE, OR, DEFUN, DEFVAR, SETQ, LOOP, RETURN, PUSH, POP, INCF,
DECF, SETF, DOLIST, DOTIMES, TRACE, UNTRACE, FORMILLIS, TIME, WITHSERIAL, WITHI2C, WITHSPI, WITHSDCARD,
DEFCODE, TAIL_FORMS, PROGN, IF, COND, WHEN, UNLESS, CASE, AND, HELP, FUNCTIONS, NOT, NULLFN, CONS, ATOM,
LISTP, CONSP, SYMBOLP, BOUNDP, SETFN, STREAMP, EQ, CAR, FIRST, CDR, REST, CAAR, CADR, SECOND, CDAR, CDDR,
CAAAR, CAADR, CADAR, CADDR, THIRD, CDAAR, CDADR, CDDAR, CDDDR, LENGTH, ARRAYDIMENSIONS, LIST, MAKEARRAY,
REVERSE, NTH, AREF, ASSOC, MEMBER, APPLY, FUNCALL, APPEND, MAPC, MAPCAR, MAPCAN, ADD, SUBTRACT, MULTIPLY,
DIVIDE, TRUNCATE, MOD, ONEPLUS, ONEMINUS, ABS, RANDOM, MAXFN, MINFN, NOTEQ, NUMEQ, LESS, LESSEQ, GREATER,
GREATEREQ, PLUSP, MINUSP, ZEROP, ODDP, EVENP, INTEGERP, NUMBERP, CHAR, CHARCODE, CODECHAR, CHARACTERP,
STRINGP, STRINGEQ, STRINGLESS, STRINGGREATER, SORT, STRINGFN, CONCATENATE, SUBSEQ, READFROMSTRING,
PRINCTOSTRING, PRIN1TOSTRING, LOGAND, LOGIOR, LOGXOR, LOGNOT, ASH, LOGBITP, EVAL, GLOBALS, LOCALS,
MAKUNBOUND, BREAK, READ, PRIN1, PRINT, PRINC, TERPRI, READBYTE, READLINE, WRITEBYTE, WRITESTRING,
WRITELINE, RESTARTI2C, GC, ROOM, SAVEIMAGE, LOADIMAGE, CLS, PINMODE, DIGITALREAD, DIGITALWRITE,
ANALOGREAD, ANALOGREFERENCE, ANALOGREADRESOLUTION, ANALOGWRITE, DACREFERENCE, DELAY, MILLIS, SLEEP, NOTE,
REGISTER, EDIT, PPRINT, PPRINTALL, FORMAT, REQUIRE, LISTLIBRARY, DOCUMENTATION, PLOT, PLOT3D, GLYPHPIXEL,
PLOTPIXEL, FILLSCREEN, KEYWORDS, 
#if defined(CPU_ATmega1284P)
K_HIGH, K_LOW, K_INPUT, K_INPUT_PULLUP, K_OUTPUT, K_DEFAULT, K_INTERNAL1V1, K_INTERNAL2V56, K_EXTERNAL,
#endif
USERFUNCTIONS, ENDFUNCTIONS, SET_SIZE = INT_MAX };

// Global variables

object Workspace[WORKSPACESIZE] OBJECTALIGNED;
#if defined(CODESIZE)
uint8_t MyCode[CODESIZE] WORDALIGNED; // Must be even
#endif

jmp_buf exception;
unsigned int Freespace = 0;
object *Freelist;
unsigned int I2Ccount;
unsigned int TraceFn[TRACEMAX];
unsigned int TraceDepth[TRACEMAX];

object *GlobalEnv;
object *GCStack = NULL;
object *GlobalString;
object *GlobalStringTail;
int GlobalStringIndex = 0;
uint8_t PrintCount = 0;
uint8_t BreakLevel = 0;
char LastChar = 0;
char LastPrint = 0;
uint16_t RandomSeed;

// Flags
enum flag { PRINTREADABLY, RETURNFLAG, ESCAPE, EXITEDITOR, LIBRARYLOADED, NOESC, NOECHO };
volatile uint8_t Flags = 0b00001; // PRINTREADABLY set by default

// Forward references
object *tee;
void pfstring (PGM_P s, pfun_t pfun);

// Error handling

void errorsub (symbol_t fname, PGM_P string) {
  pfl(pserial); pfstring(PSTR("Error: "), pserial);
  if (fname != sym(NIL)) {
    pserial('\'');
    psymbol(fname, pserial);
    pserial('\''); pserial(' ');
  }
  pfstring(string, pserial);
}

void errorsym (symbol_t fname, PGM_P string, object *symbol) {
  errorsub(fname, string);
  pserial(':'); pserial(' ');
  printobject(symbol, pserial);
  errorend();
}

void errorsym2 (symbol_t fname, PGM_P string) {
  errorsub(fname, string);
  errorend();
}

void error (builtin_t fname, PGM_P string, object *symbol) {
  errorsym(sym(fname), string, symbol);
}

void error2 (builtin_t fname, PGM_P string) {
  errorsym2(sym(fname), string);
}

void errorend () { pln(pserial); GCStack = NULL; longjmp(exception, 1); }

void formaterr (object *formatstr, PGM_P string, uint8_t p) {
  pln(pserial); indent(4, ' ', pserial); printstring(formatstr, pserial); pln(pserial);
  indent(p+5, ' ', pserial); pserial('^');
  error2(FORMAT, string);
  pln(pserial);
  GCStack = NULL;
  longjmp(exception, 1);
}

// Save space as these are used multiple times
const char notanumber[] PROGMEM = "argument is not a number";
const char notaninteger[] PROGMEM = "argument is not an integer";
const char notastring[] PROGMEM = "argument is not a string";
const char notalist[] PROGMEM = "argument is not a list";
const char notasymbol[] PROGMEM = "argument is not a symbol";
const char notproper[] PROGMEM = "argument is not a proper list";
const char toomanyargs[] PROGMEM = "too many arguments";
const char toofewargs[] PROGMEM = "too few arguments";
const char noargument[] PROGMEM = "missing argument";
const char nostream[] PROGMEM = "missing stream argument";
const char overflow[] PROGMEM = "arithmetic overflow";
const char divisionbyzero[] PROGMEM = "division by zero";
const char indexnegative[] PROGMEM = "index can't be negative";
const char invalidarg[] PROGMEM = "invalid argument";
const char invalidkey[] PROGMEM = "invalid keyword";
const char illegalclause[] PROGMEM = "illegal clause";
const char invalidpin[] PROGMEM = "invalid pin";
const char oddargs[] PROGMEM = "odd number of arguments";
const char indexrange[] PROGMEM = "index out of range";
const char canttakecar[] PROGMEM = "can't take car";
const char canttakecdr[] PROGMEM = "can't take cdr";
const char unknownstreamtype[] PROGMEM = "unknown stream type";

// Set up workspace

void initworkspace () {
  Freelist = NULL;
  for (int i=WORKSPACESIZE-1; i>=0; i--) {
    object *obj = &Workspace[i];
    car(obj) = NULL;
    cdr(obj) = Freelist;
    Freelist = obj;
    Freespace++;
  }
}

object *myalloc () {
  if (Freespace == 0) error2(NIL, PSTR("no room"));
  object *temp = Freelist;
  Freelist = cdr(Freelist);
  Freespace--;
  return temp;
}

inline void myfree (object *obj) {
  car(obj) = NULL;
  cdr(obj) = Freelist;
  Freelist = obj;
  Freespace++;
}

// Make each type of object

object *number (int n) {
  object *ptr = myalloc();
  ptr->type = NUMBER;
  ptr->integer = n;
  return ptr;
}

object *character (uint8_t c) {
  object *ptr = myalloc();
  ptr->type = CHARACTER;
  ptr->chars = c;
  return ptr;
}

object *cons (object *arg1, object *arg2) {
  object *ptr = myalloc();
  ptr->car = arg1;
  ptr->cdr = arg2;
  return ptr;
}

object *symbol (symbol_t name) {
  object *ptr = myalloc();
  ptr->type = SYMBOL;
  ptr->name = name;
  return ptr;
}

inline object *bsymbol (builtin_t name) {
  return intern(twist(name+BUILTINS));
}

object *codehead (int entry) {
  object *ptr = myalloc();
  ptr->type = CODE;
  ptr->integer = entry;
  return ptr;
}

object *intern (symbol_t name) {
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    if (obj->type == SYMBOL && obj->name == name) return obj;
  }
  return symbol(name);
}

bool eqsymbols (object *obj, char *buffer) {
  object *arg = cdr(obj);
  int i = 0;
  while (!(arg == NULL && buffer[i] == 0)) {
    if (arg == NULL || buffer[i] == 0 || arg->chars != (buffer[i]<<8 | buffer[i+1])) return false;
    arg = car(arg);
    i = i + 2;
  }
  return true;
}

object *internlong (char *buffer) {
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    if (obj->type == SYMBOL && longsymbolp(obj) && eqsymbols(obj, buffer)) return obj;
  }
  object *obj = lispstring(buffer);
  obj->type = SYMBOL;
  return obj;
}

object *stream (uint8_t streamtype, uint8_t address) {
  object *ptr = myalloc();
  ptr->type = STREAM;
  ptr->integer = streamtype<<8 | address;
  return ptr;
}

object *newstring () {
  object *ptr = myalloc();
  ptr->type = STRING;
  ptr->chars = 0;
  return ptr;
}

// Garbage collection

void markobject (object *obj) {
  MARK:
  if (obj == NULL) return;
  if (marked(obj)) return;

  object* arg = car(obj);
  unsigned int type = obj->type;
  mark(obj);

  if (type >= PAIR || type == ZZERO) { // cons
    markobject(arg);
    obj = cdr(obj);
    goto MARK;
  }

  if (type == ARRAY) {
    obj = cdr(obj);
    goto MARK;
  }

  if ((type == STRING) || (type == SYMBOL && longsymbolp(obj))) {
    obj = cdr(obj);
    while (obj != NULL) {
      arg = car(obj);
      mark(obj);
      obj = arg;
    }
  }
}

void sweep () {
  Freelist = NULL;
  Freespace = 0;
  for (int i=WORKSPACESIZE-1; i>=0; i--) {
    object *obj = &Workspace[i];
    if (!marked(obj)) myfree(obj); else unmark(obj);
  }
}

void gc (object *form, object *env) {
  #if defined(printgcs)
  int start = Freespace;
  #endif
  markobject(tee);
  markobject(GlobalEnv);
  markobject(GCStack);
  markobject(form);
  markobject(env);
  sweep();
  #if defined(printgcs)
  pfl(pserial); pserial('{'); pint(Freespace - start, pserial); pserial('}');
  #endif
}

// Compact image

void movepointer (object *from, object *to) {
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    unsigned int type = (obj->type) & ~MARKBIT;
    if (marked(obj) && (type >= ARRAY || type==ZZERO || (type == SYMBOL && longsymbolp(obj)))) {
      if (car(obj) == (object *)((uintptr_t)from | MARKBIT))
        car(obj) = (object *)((uintptr_t)to | MARKBIT);
      if (cdr(obj) == from) cdr(obj) = to;
    }
  }
  // Fix strings and long symbols
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    if (marked(obj)) {
      unsigned int type = (obj->type) & ~MARKBIT;
      if (type == STRING || (type == SYMBOL && longsymbolp(obj))) {
        obj = cdr(obj);
        while (obj != NULL) {
          if (cdr(obj) == to) cdr(obj) = from;
          obj = (object *)((uintptr_t)(car(obj)) & ~MARKBIT);
        }
      }
    }
  }
}

uintptr_t compactimage (object **arg) {
  markobject(tee);
  markobject(GlobalEnv);
  markobject(GCStack);
  object *firstfree = Workspace;
  while (marked(firstfree)) firstfree++;
  object *obj = &Workspace[WORKSPACESIZE-1];
  while (firstfree < obj) {
    if (marked(obj)) {
      car(firstfree) = car(obj);
      cdr(firstfree) = cdr(obj);
      unmark(obj);
      movepointer(obj, firstfree);
      if (GlobalEnv == obj) GlobalEnv = firstfree;
      if (GCStack == obj) GCStack = firstfree;
      if (*arg == obj) *arg = firstfree;
      while (marked(firstfree)) firstfree++;
    }
    obj--;
  }
  sweep();
  return firstfree - Workspace;
}

// Make SD card filename

char *MakeFilename (object *arg, char *buffer) {
  int max = BUFFERSIZE-1;
  int i = 0;
  do {
    char c = nthchar(arg, i);
    if (c == '\0') break;
    buffer[i++] = c;
  } while (i<max);
  buffer[i] = '\0';
  return buffer;
}

// Save-image and load-image

#if defined(sdcardsupport)
void SDWriteInt (File file, int data) {
  file.write(data & 0xFF); file.write(data>>8 & 0xFF);
}

int SDReadInt (File file) {
  uint8_t b0 = file.read(); uint8_t b1 = file.read();
  return b0 | b1<<8;
}
#elif defined(FLASHWRITESIZE)
#if defined (CPU_ATmega1284P)
// save-image area is the 16K bytes (64 256-byte pages) from 0x1bc00 to 0x1fc00
const uint32_t BaseAddress = 0x1bc00;
uint8_t FlashCheck() {
  return 0;
}

void FlashWriteInt (uint32_t *addr, int data) {
  if (((*addr) & 0xFF) == 0) optiboot_page_erase(BaseAddress + ((*addr) & 0xFF00));
  optiboot_page_fill(BaseAddress + *addr, data);
  if (((*addr) & 0xFF) == 0xFE) optiboot_page_write(BaseAddress + ((*addr) & 0xFF00));
  (*addr)++; (*addr)++;
}

void FlashEndWrite (uint32_t *addr) {
  if (((*addr) & 0xFF) != 0) optiboot_page_write((BaseAddress + ((*addr) & 0xFF00)));
}

uint8_t FlashReadByte (uint32_t *addr) {
  return pgm_read_byte_far(BaseAddress + (*addr)++);
}

int FlashReadInt (uint32_t *addr) {
  int data = pgm_read_word_far(BaseAddress + *addr);
  (*addr)++; (*addr)++;
  return data;
}
#elif defined (CPU_AVR128DX48)
// save-image area is the 16K bytes (32 512-byte pages) from 0x1c000 to 0x20000
const uint32_t BaseAddress = 0x1c000;
uint8_t FlashCheck() {
  return Flash.checkWritable();
}

void FlashWriteInt (uint32_t *addr, int data) {
  if (((*addr) & 0x1FF) == 0) Flash.erasePage(BaseAddress + ((*addr) & 0xFE00));
  Flash.writeWord(BaseAddress + *addr, data);
  (*addr)++; (*addr)++;
}

void FlashEndWrite (uint32_t *addr) {
  (void) addr;
}

uint8_t FlashReadByte (uint32_t *addr) {
  return Flash.readByte(BaseAddress + (*addr)++);
}

int FlashReadInt (uint32_t *addr) {
  int data = Flash.readWord(BaseAddress + *addr);
  (*addr)++; (*addr)++;
  return data;
}
#endif
#else
void EEPROMWriteInt (unsigned int *addr, int data) {
  EEPROM.write((*addr)++, data & 0xFF); EEPROM.write((*addr)++, data>>8 & 0xFF);
}

int EEPROMReadInt (unsigned int *addr) {
  uint8_t b0 = EEPROM.read((*addr)++); uint8_t b1 = EEPROM.read((*addr)++);
  return b0 | b1<<8;
}
#endif

unsigned int saveimage (object *arg) {
  unsigned int imagesize = compactimage(&arg);
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file;
  if (stringp(arg)) {
    char buffer[BUFFERSIZE];
    file = SD.open(MakeFilename(arg, buffer), O_RDWR | O_CREAT | O_TRUNC);
    if (!file) error2(SAVEIMAGE, PSTR("problem saving to SD card or invalid filename"));
    arg = NULL;
  } else if (arg == NULL || listp(arg)) {
    file = SD.open("/ULISP.IMG", O_RDWR | O_CREAT | O_TRUNC);
    if (!file) error2(SAVEIMAGE, PSTR("problem saving to SD card"));
  }
  else error(SAVEIMAGE, invalidarg, arg);
  SDWriteInt(file, (uintptr_t)arg);
  SDWriteInt(file, imagesize);
  SDWriteInt(file, (uintptr_t)GlobalEnv);
  SDWriteInt(file, (uintptr_t)GCStack);
  #if defined(CODESIZE)
  for (int i=0; i<CODESIZE; i++) file.write(MyCode[i]);
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    SDWriteInt(file, (uintptr_t)car(obj));
    SDWriteInt(file, (uintptr_t)cdr(obj));
  }
  file.close();
  return imagesize;
#elif defined(FLASHWRITESIZE)
  if (!(arg == NULL || listp(arg))) error(SAVEIMAGE, invalidarg, arg);
  if (FlashCheck()) error2(SAVEIMAGE, PSTR("flash write not supported"));
  // Save to Flash
  int bytesneeded = 10 + CODESIZE + imagesize*4;
  if (bytesneeded > FLASHWRITESIZE) error(SAVEIMAGE, PSTR("image too large"), number(imagesize));
  uint32_t addr = 0;
  FlashWriteInt(&addr, (uintptr_t)arg);
  FlashWriteInt(&addr, imagesize);
  FlashWriteInt(&addr, (uintptr_t)GlobalEnv);
  FlashWriteInt(&addr, (uintptr_t)GCStack);
  #if defined(CODESIZE)
  for (int i=0; i<CODESIZE/2; i++) FlashWriteInt(&addr, MyCode[i*2] | MyCode[i*2+1]<<8);
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    FlashWriteInt(&addr, (uintptr_t)car(obj));
    FlashWriteInt(&addr, (uintptr_t)cdr(obj));
  }
  FlashEndWrite(&addr);
  return imagesize;
#else
  if (!(arg == NULL || listp(arg))) error(SAVEIMAGE, invalidarg, arg);
  int bytesneeded = imagesize*4 + 10;
  if (bytesneeded > EEPROMSIZE) error(SAVEIMAGE, PSTR("image too large"), number(imagesize));
  unsigned int addr = 0;
  EEPROMWriteInt(&addr, (unsigned int)arg);
  EEPROMWriteInt(&addr, imagesize);
  EEPROMWriteInt(&addr, (unsigned int)GlobalEnv);
  EEPROMWriteInt(&addr, (unsigned int)GCStack);
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    EEPROMWriteInt(&addr, (uintptr_t)car(obj));
    EEPROMWriteInt(&addr, (uintptr_t)cdr(obj));
  }
  return imagesize;
#endif
}

unsigned int loadimage (object *arg) {
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file;
  if (stringp(arg)) {
    char buffer[BUFFERSIZE];
    file = SD.open(MakeFilename(arg, buffer));
    if (!file) error2(LOADIMAGE, PSTR("problem loading from SD card or invalid filename"));
  }
  else if (arg == NULL) {
    file = SD.open("/ULISP.IMG");
    if (!file) error2(LOADIMAGE, PSTR("problem loading from SD card"));
  }
  else error(LOADIMAGE, invalidarg, arg);
  SDReadInt(file);
  unsigned int imagesize = SDReadInt(file);
  GlobalEnv = (object *)SDReadInt(file);
  GCStack = (object *)SDReadInt(file);
  #if defined(CODESIZE)
  for (int i=0; i<CODESIZE; i++) MyCode[i] = file.read();
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    car(obj) = (object *)SDReadInt(file);
    cdr(obj) = (object *)SDReadInt(file);
  }
  file.close();
  gc(NULL, NULL);
  return imagesize;
#elif defined(FLASHWRITESIZE)
  (void) arg;
  if (FlashCheck()) error2(SAVEIMAGE, PSTR("flash write not supported"));
  uint32_t addr = 0;
  FlashReadInt(&addr); // Skip eval address
  unsigned int imagesize = FlashReadInt(&addr);
  if (imagesize == 0 || imagesize == 0xFFFF) error2(LOADIMAGE, PSTR("no saved image"));
  GlobalEnv = (object *)FlashReadInt(&addr);
  GCStack = (object *)FlashReadInt(&addr);
  #if defined(CODESIZE)
  for (int i=0; i<CODESIZE; i++) MyCode[i] = FlashReadByte(&addr);
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    car(obj) = (object *)FlashReadInt(&addr);
    cdr(obj) = (object *)FlashReadInt(&addr);
  }
  gc(NULL, NULL);
  return imagesize;
#else
  (void) arg;
  unsigned int addr = 2; // Skip eval address
  unsigned int imagesize = EEPROMReadInt(&addr);
  if (imagesize == 0 || imagesize == 0xFFFF) error2(LOADIMAGE, PSTR("no saved image"));
  GlobalEnv = (object *)EEPROMReadInt(&addr);
  GCStack = (object *)EEPROMReadInt(&addr);
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    car(obj) = (object *)EEPROMReadInt(&addr);
    cdr(obj) = (object *)EEPROMReadInt(&addr);
  }
  gc(NULL, NULL);
  return imagesize;
#endif
}

void autorunimage () {
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file = SD.open("ULISP.IMG");
  if (!file) error2(NIL, PSTR("problem autorunning from SD card"));
  object *autorun = (object *)SDReadInt(file);
  file.close();
  if (autorun != NULL) {
    loadimage(NULL);
    apply(NIL, autorun, NULL, NULL);
  }
#elif defined(FLASHWRITESIZE)
  uint32_t addr = 0;
  object *autorun = (object *)FlashReadInt(&addr);
  if (autorun != NULL && (unsigned int)autorun != 0xFFFF) {
    loadimage(nil);
    apply(NIL, autorun, NULL, NULL);
  }
#else
  unsigned int addr = 0;
  object *autorun = (object *)EEPROMReadInt(&addr);
  if (autorun != NULL && (unsigned int)autorun != 0xFFFF) {
    loadimage(nil);
    apply(NIL, autorun, NULL, NULL);
  }
#endif
}

// Tracing

int tracing (symbol_t name) {
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == name) return i+1;
    i++;
  }
  return 0;
}

void trace (symbol_t name) {
  if (tracing(name)) error(TRACE, PSTR("already being traced"), symbol(name));
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == 0) { TraceFn[i] = name; TraceDepth[i] = 0; return; }
    i++;
  }
  error2(TRACE, PSTR("already tracing 3 functions"));
}

void untrace (symbol_t name) {
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == name) { TraceFn[i] = 0; return; }
    i++;
  }
  error(UNTRACE, PSTR("not tracing"), symbol(name));
}

// Helper functions

bool consp (object *x) {
  if (x == NULL) return false;
  unsigned int type = x->type;
  return type >= PAIR || type == ZZERO;
}

#define atom(x) (!consp(x))

bool listp (object *x) {
  if (x == NULL) return true;
  unsigned int type = x->type;
  return type >= PAIR || type == ZZERO;
}

#define improperp(x) (!listp(x))

object *quote (object *arg) {
  return cons(bsymbol(QUOTE), cons(arg,NULL));
}

// Radix 40 encoding

builtin_t builtin (symbol_t name) {
  return (builtin_t)(untwist(name) - BUILTINS);
}

symbol_t sym (builtin_t x) {
  return twist(x + BUILTINS);
}

int8_t toradix40 (char ch) {
  if (ch == 0) return 0;
  if (ch >= '0' && ch <= '9') return ch-'0'+1;
  if (ch == '-') return 37; if (ch == '*') return 38; if (ch == '$') return 39;
  ch = ch | 0x20;
  if (ch >= 'a' && ch <= 'z') return ch-'a'+11;
  return -1; // Invalid
}

char fromradix40 (char n) {
  if (n >= 1 && n <= 9) return '0'+n-1;
  if (n >= 11 && n <= 36) return 'a'+n-11;
  if (n == 37) return '-'; if (n == 38) return '*'; if (n == 39) return '$';
  return 0;
}

uint16_t pack40 (char *buffer) {
  return (((toradix40(buffer[0]) * 40) + toradix40(buffer[1])) * 40 + toradix40(buffer[2]));
}

bool valid40 (char *buffer) {
 return (toradix40(buffer[0]) >= 11 && toradix40(buffer[1]) >= 0 && toradix40(buffer[2]) >= 0);
}

int8_t digitvalue (char d) {
  if (d>='0' && d<='9') return d-'0';
  d = d | 0x20;
  if (d>='a' && d<='f') return d-'a'+10;
  return 16;
}

int checkinteger (builtin_t name, object *obj) {
  if (!integerp(obj)) error(name, notaninteger, obj);
  return obj->integer;
}

int checkbitvalue (builtin_t name, object *obj) {
  if (!integerp(obj)) error(name, notaninteger, obj);
  int n = obj->integer;
  if (n & ~1) error(name, PSTR("argument is not a bit value"), obj);
  return n;
}

int checkchar (builtin_t name, object *obj) {
  if (!characterp(obj)) error(name, PSTR("argument is not a character"), obj);
  return obj->chars;
}

object *checkstring (builtin_t name, object *obj) {
  if (!stringp(obj)) error(name, notastring, obj);
  return obj;
}

int isstream (object *obj){
  if (!streamp(obj)) error(NIL, PSTR("not a stream"), obj);
  return obj->integer;
}

int isbuiltin (object *obj, builtin_t n) {
  return symbolp(obj) && obj->name == sym(n);
}

bool builtinp (symbol_t name) {
  return (untwist(name) > BUILTINS && untwist(name) < ENDFUNCTIONS+BUILTINS);
}

int keywordp (object *obj) {
  if (!symbolp(obj)) return false;
  builtin_t name = builtin(obj->name);
  return ((name > KEYWORDS) && (name < USERFUNCTIONS));
}

int checkkeyword (builtin_t name, object *obj) {
  if (!keywordp(obj)) error(name, PSTR("argument is not a keyword"), obj);
  builtin_t kname = builtin(obj->name);
  uint8_t context = getminmax(kname);
  if (context != 0 && context != name) error(name, invalidkey, obj);
  return ((int)lookupfn(kname));
}

void checkargs (builtin_t name, object *args) {
  int nargs = listlength(name, args);
  checkminmax(name, nargs);
}

int eq (object *arg1, object *arg2) {
  if (arg1 == arg2) return true;  // Same object
  if ((arg1 == nil) || (arg2 == nil)) return false;  // Not both values
  if (arg1->cdr != arg2->cdr) return false;  // Different values
  if (symbolp(arg1) && symbolp(arg2)) return true;  // Same symbol
  if (integerp(arg1) && integerp(arg2)) return true;  // Same integer
  if (characterp(arg1) && characterp(arg2)) return true;  // Same character
  return false;
}

int listlength (builtin_t name, object *list) {
  int length = 0;
  while (list != NULL) {
    if (improperp(list)) error2(name, notproper);
    list = cdr(list);
    length++;
  }
  return length;
}

// Mathematical helper functions

uint16_t pseudoRandom (int range) {
  if (RandomSeed == 0) RandomSeed++;
  uint16_t l = RandomSeed & 1;
  RandomSeed = RandomSeed >> 1;
  if (l == 1) RandomSeed = RandomSeed ^ 0xD295;
  int dummy; if (RandomSeed == 0) Serial.print((int)&dummy); // Do not remove!
  return RandomSeed % range;
}

object *compare (builtin_t name, object *args, bool lt, bool gt, bool eq) {
  int arg1 = checkinteger(name, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(name, first(args));
    if (!lt && (arg1 < arg2)) return nil;
    if (!eq && (arg1 == arg2)) return nil;
    if (!gt && (arg1 > arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

int intpower (int base, int exp) {
  int result = 1;
  while (exp) {
    if (exp & 1) result = result * base;
    exp = exp / 2;
    base = base * base;
  }
  return result;
}

// Association lists

object *assoc (object *key, object *list) {
  while (list != NULL) {
    if (improperp(list)) error(ASSOC, notproper, list);
    object *pair = first(list);
    if (!listp(pair)) error(ASSOC, PSTR("element is not a list"), pair);
    if (pair != NULL && eq(key,car(pair))) return pair;
    list = cdr(list);
  }
  return nil;
}

object *delassoc (object *key, object **alist) {
  object *list = *alist;
  object *prev = NULL;
  while (list != NULL) {
    object *pair = first(list);
    if (eq(key,car(pair))) {
      if (prev == NULL) *alist = cdr(list);
      else cdr(prev) = cdr(list);
      return key;
    }
    prev = list;
    list = cdr(list);
  }
  return nil;
}

// Array utilities

int nextpower2 (int n) {
  n--; n |= n >> 1; n |= n >> 2; n |= n >> 4;
  n |= n >> 8; n |= n >> 16; n++;
  return n<2 ? 2 : n;
}

object *buildarray (int n, int s, object *def) {
  int s2 = s>>1;
  if (s2 == 1) {
    if (n == 2) return cons(def, def);
    else if (n == 1) return cons(def, NULL);
    else return NULL;
  } else if (n >= s2) return cons(buildarray(s2, s2, def), buildarray(n - s2, s2, def));
  else return cons(buildarray(n, s2, def), nil);
}

object *makearray (builtin_t name, object *dims, object *def, bool bitp) {
  int size = 1;
  object *dimensions = dims;
  while (dims != NULL) {
    int d = car(dims)->integer;
    if (d < 0) error2(name, PSTR("dimension can't be negative"));
    size = size * d;
    dims = cdr(dims);
  }
  // Bit array identified by making first dimension negative
  if (bitp) {
    size = (size + sizeof(int)*8 - 1)/(sizeof(int)*8);
    car(dimensions) = number(-(car(dimensions)->integer));
  }
  object *ptr = myalloc();
  ptr->type = ARRAY;
  object *tree = nil;
  if (size != 0) tree = buildarray(size, nextpower2(size), def);
  ptr->cdr = cons(tree, dimensions);
  return ptr;
}

object **arrayref (object *array, int index, int size) {
  int mask = nextpower2(size)>>1;
  object **p = &car(cdr(array));
  while (mask) {
    if ((index & mask) == 0) p = &(car(*p)); else p = &(cdr(*p));
    mask = mask>>1;
  }
  return p;
}

object **getarray (builtin_t name, object *array, object *subs, object *env, int *bit) {
  int index = 0, size = 1, s;
  *bit = -1;
  bool bitp = false;
  object *dims = cddr(array);
  while (dims != NULL && subs != NULL) {
    int d = car(dims)->integer;
    if (d < 0) { d = -d; bitp = true; }
    if (env) s = checkinteger(name, eval(car(subs), env)); else s = checkinteger(name, car(subs));
    if (s < 0 || s >= d) error(name, PSTR("subscript out of range"), car(subs));
    size = size * d;
    index = index * d + s;
    dims = cdr(dims); subs = cdr(subs);
  }
  if (dims != NULL) error2(name, PSTR("too few subscripts"));
  if (subs != NULL) error2(name, PSTR("too many subscripts"));
  if (bitp) {
    size = (size + sizeof(int)*8 - 1)/(sizeof(int)*8);
    *bit = index & (sizeof(int)==4 ? 0x1F : 0x0F);
    index = index>>(sizeof(int)==4 ? 5 : 4);
  }
  return arrayref(array, index, size);
}

void rslice (object *array, int size, int slice, object *dims, object *args) {
  int d = first(dims)->integer;
  for (int i = 0; i < d; i++) {
    int index = slice * d + i;
    if (!consp(args)) error2(NIL, PSTR("initial contents don't match array type"));
    if (cdr(dims) == NULL) {
      object **p = arrayref(array, index, size);
      *p = car(args);
    } else rslice(array, size, index, cdr(dims), car(args));
    args = cdr(args);
  }
}

object *readarray (int d, object *args) {
  object *list = args;
  object *dims = NULL; object *head = NULL;
  int size = 1;
  for (int i = 0; i < d; i++) {
    if (!listp(list)) error2(NIL, PSTR("initial contents don't match array type"));
    int l = listlength(NIL, list);
    if (dims == NULL) { dims = cons(number(l), NULL); head = dims; }
    else { cdr(dims) = cons(number(l), NULL); dims = cdr(dims); }
    size = size * l;
    if (list != NULL) list = car(list);
  }
  object *array = makearray(NIL, head, NULL, false);
  rslice(array, size, 0, head, args);
  return array;
}

object *readbitarray (gfun_t gfun) {
  char ch = gfun();
  object *head = NULL;
  object *tail = NULL;
  while (!issp(ch) && ch != ')' && ch != '(') {
    if (ch != '0' && ch != '1') error2(NIL, PSTR("illegal character in bit array"));
    object *cell = cons(number(ch - '0'), NULL);
    if (head == NULL) head = cell;
    else tail->cdr = cell;
    tail = cell;
    ch = gfun();
  }
  LastChar = ch;
  int size = listlength(NIL, head);
  object *array = makearray(NIL, cons(number(size), NULL), 0, true);
  size = (size + sizeof(int)*8 - 1)/(sizeof(int)*8);
  int index = 0;
  while (head != NULL) {
    object **loc = arrayref(array, index>>(sizeof(int)==4 ? 5 : 4), size);
    int bit = index & (sizeof(int)==4 ? 0x1F : 0x0F);
    *loc = number((((*loc)->integer) & ~(1<<bit)) | (car(head)->integer)<<bit);
    index++;
    head = cdr(head);
  }
  return array;
}

void pslice (object *array, int size, int slice, object *dims, pfun_t pfun, bool bitp) {
  bool spaces = true;
  if (slice == -1) { spaces = false; slice = 0; }
  int d = first(dims)->integer;
  if (d < 0) d = -d;
  for (int i = 0; i < d; i++) {
    if (i && spaces) pfun(' ');
    int index = slice * d + i;
    if (cdr(dims) == NULL) {
      if (bitp) pint(((*arrayref(array, index>>(sizeof(int)==4 ? 5 : 4), size))->integer)>>
        (index & (sizeof(int)==4 ? 0x1F : 0x0F)) & 1, pfun);
      else printobject(*arrayref(array, index, size), pfun);
    } else { pfun('('); pslice(array, size, index, cdr(dims), pfun, bitp); pfun(')'); }
  }
}

void printarray (object *array, pfun_t pfun) {
  object *dimensions = cddr(array);
  object *dims = dimensions;
  bool bitp = false;
  int size = 1, n = 0;
  while (dims != NULL) {
    int d = car(dims)->integer;
    if (d < 0) { bitp = true; d = -d; }
    size = size * d;
    dims = cdr(dims); n++;
  }
  if (bitp) size = (size + sizeof(int)*8 - 1)/(sizeof(int)*8);
  pfun('#');
  if (n == 1 && bitp) { pfun('*'); pslice(array, size, -1, dimensions, pfun, bitp); }
  else {
    if (n > 1) { pint(n, pfun); pfun('A'); }
    pfun('('); pslice(array, size, 0, dimensions, pfun, bitp); pfun(')');
  }
}

// String utilities

void indent (uint8_t spaces, char ch, pfun_t pfun) {
  for (uint8_t i=0; i<spaces; i++) pfun(ch);
}

object *startstring (builtin_t name) {
  (void) name;
  object *string = newstring();
  GlobalString = string;
  GlobalStringTail = string;
  return string;
}

void buildstring (char ch, object **tail) {
  object *cell;
  if (cdr(*tail) == NULL) {
    cell = myalloc(); cdr(*tail) = cell;
  } else if (((*tail)->chars & 0xFF) == 0) {
    (*tail)->chars = (*tail)->chars | ch; return;
  } else {
    cell = myalloc(); car(*tail) = cell;
  }
  car(cell) = NULL; cell->chars = ch<<8; *tail = cell;
}

object *copystring (object *arg) {
  object *obj = newstring();
  object *ptr = obj;
  arg = cdr(arg);
  while (arg != NULL) {
    object *cell =  myalloc(); car(cell) = NULL;
    if (cdr(obj) == NULL) cdr(obj) = cell; else car(ptr) = cell;
    ptr = cell;
    ptr->chars = arg->chars;
    arg = car(arg);
  }
  return obj;
}

object *readstring (uint8_t delim, gfun_t gfun) {
  object *obj = newstring();
  object *tail = obj;
  int ch = gfun();
  if (ch == -1) return nil;
  while ((ch != delim) && (ch != -1)) {
    if (ch == '\\') ch = gfun();
    buildstring(ch, &tail);
    ch = gfun();
  }
  return obj;
}

int stringlength (object *form) {
  int length = 0;
  form = cdr(form);
  while (form != NULL) {
    int chars = form->chars;
    for (int i=(sizeof(int)-1)*8; i>=0; i=i-8) {
      if (chars>>i & 0xFF) length++;
    }
    form = car(form);
  }
  return length;
}

uint8_t nthchar (object *string, int n) {
  object *arg = cdr(string);
  int top;
  if (sizeof(int) == 4) { top = n>>2; n = 3 - (n&3); }
  else { top = n>>1; n = 1 - (n&1); }
  for (int i=0; i<top; i++) {
    if (arg == NULL) return 0;
    arg = car(arg);
  }
  if (arg == NULL) return 0;
  return (arg->chars)>>(n*8) & 0xFF;
}

int gstr () {
  if (LastChar) {
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  char c = nthchar(GlobalString, GlobalStringIndex++);
  if (c != 0) return c;
  return '\n'; // -1?
}

void pstr (char c) {
  buildstring(c, &GlobalStringTail);
}

object *lispstring (char *s) {
  object *obj = newstring();
  object *tail = obj;
  while(1) {
    char ch = *s++;
    if (ch == 0) break;
    if (ch == '\\') ch = *s++;
    buildstring(ch, &tail);
  }
  return obj;
}

bool stringcompare (builtin_t name, object *args, bool lt, bool gt, bool eq) {
  object *arg1 = checkstring(name, first(args));
  object *arg2 = checkstring(name, second(args));
  arg1 = cdr(arg1);
  arg2 = cdr(arg2);
  while ((arg1 != NULL) || (arg2 != NULL)) {
    if (arg1 == NULL) return lt;
    if (arg2 == NULL) return gt;
    if (arg1->chars < arg2->chars) return lt;
    if (arg1->chars > arg2->chars) return gt;
    arg1 = car(arg1);
    arg2 = car(arg2);
  }
  return eq;
}

object *documentation (builtin_t name, object *arg, object *env) {
  if (!symbolp(arg)) error(name, notasymbol, arg);
  object *pair = findpair(arg, env);
  if (pair != NULL) {
    object *val = cdr(pair);
    if (listp(val) && first(val)->name == sym(LAMBDA) && cdr(val) != NULL && cddr(val) != NULL) {
      if (stringp(third(val))) return third(val);
    }
  }
  symbol_t docname = arg->name;
  if (!builtinp(docname)) return nil;
  char *docstring = lookupdoc(builtin(docname));
  if (docstring == NULL) return nil;
  #if defined(CPU_ATmega4809)
  return lispstring(docstring);
  #else
  object *obj = startstring(PRINCTOSTRING);
  pfstring(docstring, pstr);
  return obj;
  #endif
}

// Lookup variable in environment

object *value (symbol_t n, object *env) {
  while (env != NULL) {
    object *pair = car(env);
    if (pair != NULL && car(pair)->name == n) return pair;
    env = cdr(env);
  }
  return nil;
}

object *findpair (object *var, object *env) {
  symbol_t name = var->name;
  object *pair = value(name, env);
  if (pair == NULL) pair = value(name, GlobalEnv);
  return pair;
}

bool boundp (object *var, object *env) {
  return (findpair(var, env) != NULL);
}

object *findvalue (builtin_t name, object *var, object *env) {
  object *pair = findpair(var, env);
  if (pair == NULL) error(name, PSTR("unknown variable"), var);
  return pair;
}

// Handling closures

object *closure (int tc, symbol_t name, object *function, object *args, object **env) {
  object *state = car(function);
  function = cdr(function);
  int trace = 0;
  if (name) trace = tracing(name);
  if (trace) {
    indent(TraceDepth[trace-1]<<1, ' ', pserial);
    pint(TraceDepth[trace-1]++, pserial);
    pserial(':'); pserial(' '); pserial('('); printsymbol(symbol(name), pserial);
  }
  object *params = first(function);
  if (!listp(params)) errorsym(name, notalist, params);
  function = cdr(function);
  // Dropframe
  if (tc) {
    if (*env != NULL && car(*env) == NULL) {
      pop(*env);
      while (*env != NULL && car(*env) != NULL) pop(*env);
    } else push(nil, *env);
  }
  // Push state
  while (consp(state)) {
    object *pair = first(state);
    push(pair, *env);
    state = cdr(state);
  }
  // Add arguments to environment
  bool optional = false;
  while (params != NULL) {
    object *value;
    object *var = first(params);
    if (isbuiltin(var, OPTIONAL)) optional = true;
    else {
      if (consp(var)) {
        if (!optional) errorsym(name, PSTR("invalid default value"), var);
        if (args == NULL) value = eval(second(var), *env);
        else { value = first(args); args = cdr(args); }
        var = first(var);
        if (!symbolp(var)) errorsym(name, PSTR("illegal optional parameter"), var);
      } else if (!symbolp(var)) {
        errorsym(name, PSTR("illegal function parameter"), var);
      } else if (isbuiltin(var, AMPREST)) {
        params = cdr(params);
        var = first(params);
        value = args;
        args = NULL;
      } else {
        if (args == NULL) {
          if (optional) value = nil;
          else errorsym2(name, toofewargs);
        } else { value = first(args); args = cdr(args); }
      }
      push(cons(var,value), *env);
      if (trace) { pserial(' '); printobject(value, pserial); }
    }
    params = cdr(params);
  }
  if (args != NULL) errorsym2(name, toomanyargs);
  if (trace) { pserial(')'); pln(pserial); }
  // Do an implicit progn
  if (tc) push(nil, *env);
  return tf_progn(function, *env);
}

object *apply (builtin_t name, object *function, object *args, object *env) {
  if (symbolp(function)) {
    builtin_t fname = builtin(function->name);
    if ((fname > FUNCTIONS) && (fname < KEYWORDS)) {
      checkargs(fname, args);
      return ((fn_ptr_type)lookupfn(fname))(args, env);
    } else function = eval(function, env);
  }
  if (consp(function) && isbuiltin(car(function), LAMBDA)) {
    object *result = closure(0, sym(name), function, args, &env);
    return eval(result, env);
  }
  if (consp(function) && isbuiltin(car(function), CLOSURE)) {
    function = cdr(function);
    object *result = closure(0, sym(name), function, args, &env);
    return eval(result, env);
  }
  error(name, PSTR("illegal function"), function);
  return NULL;
}

// In-place operations

object **place (builtin_t name, object *args, object *env, int *bit) {
  *bit = -1;
  if (atom(args)) return &cdr(findvalue(name, args, env));
  object* function = first(args);
  if (symbolp(function)) {
    symbol_t sname = function->name;
    if (sname == sym(CAR) || sname == sym(FIRST)) {
      object *value = eval(second(args), env);
      if (!listp(value)) error(name, canttakecar, value);
      return &car(value);
    }
    if (sname == sym(CDR) || sname == sym(REST)) {
      object *value = eval(second(args), env);
      if (!listp(value)) error(name, canttakecdr, value);
      return &cdr(value);
    }
    if (sname == sym(NTH)) {
      int index = checkinteger(NTH, eval(second(args), env));
      object *list = eval(third(args), env);
      if (atom(list)) error(name, PSTR("second argument to nth is not a list"), list);
      while (index > 0) {
        list = cdr(list);
        if (list == NULL) error2(name, PSTR("index to nth is out of range"));
        index--;
      }
      return &car(list);
    }
    if (sname == sym(AREF)) {
      object *array = eval(second(args), env);
      if (!arrayp(array)) error(AREF, PSTR("first argument is not an array"), array);
      return getarray(AREF, array, cddr(args), env, bit);
    }
  }
  error2(name, PSTR("illegal place"));
  return nil;
}

object *incfdecf (builtin_t name, object *args, int increment, object *env) {
  int bit;
  checkargs(name, args);
  object **loc = place(name, first(args), env, &bit);
  int result = checkinteger(name, *loc);
  args = cdr(args);
  object *inc = (args != NULL) ? eval(first(args), env) : NULL;

  if (bit != -1) {
    if (inc != NULL) increment = checkbitvalue(name, inc);
    int newvalue = (((*loc)->integer)>>bit & 1) + increment;

    if (newvalue & ~1) error2(name, PSTR("result is not a bit value"));
    *loc = number((((*loc)->integer) & ~(1<<bit)) | newvalue<<bit);
    return number(newvalue);
  }

  if (inc != NULL) increment = increment * checkinteger(name, inc);
  #if defined(checkoverflow)
  if (increment < 1) { if (INT_MIN - increment > result) error2(name, overflow); }
  else { if (INT_MAX - increment < result) error2(name, overflow); }
  #endif
  result = result + increment;
  *loc = number(result);
  return *loc;
}

// Checked car and cdr

object *carx (object *arg) {
  if (!listp(arg)) error(NIL, canttakecar, arg);
  if (arg == nil) return nil;
  return car(arg);
}

object *cdrx (object *arg) {
  if (!listp(arg)) error(NIL, canttakecdr, arg);
  if (arg == nil) return nil;
  return cdr(arg);
}

object *cxxxr (object *args, uint8_t pattern) {
  object *arg = first(args);
  while (pattern != 1) {
    if ((pattern & 1) == 0) arg = carx(arg); else arg = cdrx(arg);
    pattern = pattern>>1;
  }
  return arg;
}

// Mapping helper functions

void mapcarfun (object *result, object **tail) {
  object *obj = cons(result,NULL);
  cdr(*tail) = obj; *tail = obj;
}

void mapcanfun (object *result, object **tail) {
  if (cdr(*tail) != NULL) error(MAPCAN, notproper, *tail);
  while (consp(result)) {
    cdr(*tail) = result; *tail = result;
    result = cdr(result);
  }
}

object *mapcarcan (builtin_t name, object *args, object *env, mapfun_t fun) {
  object *function = first(args);
  args = cdr(args);
  object *params = cons(NULL, NULL);
  push(params,GCStack);
  object *head = cons(NULL, NULL);
  push(head,GCStack);
  object *tail = head;
  // Make parameters
  while (true) {
    object *tailp = params;
    object *lists = args;
    while (lists != NULL) {
      object *list = car(lists);
      if (list == NULL) {
         pop(GCStack);
         pop(GCStack);
         return cdr(head);
      }
      if (improperp(list)) error(name, notproper, list);
      object *obj = cons(first(list),NULL);
      car(lists) = cdr(list);
      cdr(tailp) = obj; tailp = obj;
      lists = cdr(lists);
    }
    object *result = apply(name, function, cdr(params), env);
    fun(result, &tail);
  }
}

// I2C interface for AVR platforms, uses much less RAM than Arduino Wire

uint8_t const TWI_SDA_PIN = 17;
uint8_t const TWI_SCL_PIN = 16;

uint32_t const F_TWI = 400000L;  // Hardware I2C clock in Hz
uint8_t const TWSR_MTX_DATA_ACK = 0x28;
uint8_t const TWSR_MTX_ADR_ACK = 0x18;
uint8_t const TWSR_MRX_ADR_ACK = 0x40;
uint8_t const TWSR_START = 0x08;
uint8_t const TWSR_REP_START = 0x10;
uint8_t const I2C_READ = 1;
uint8_t const I2C_WRITE = 0;

void I2Cinit (bool enablePullup) {
  TWSR = 0;                        // no prescaler
  TWBR = (F_CPU/F_TWI - 16)/2;     // set bit rate factor
  if (enablePullup) {
    digitalWrite(TWI_SDA_PIN, HIGH);
    digitalWrite(TWI_SCL_PIN, HIGH);
  }
}

int I2Cread () {
  if (I2Ccount != 0) I2Ccount--;
  TWCR = 1<<TWINT | 1<<TWEN | ((I2Ccount == 0) ? 0 : (1<<TWEA));
  while (!(TWCR & 1<<TWINT));
  return TWDR;
}

bool I2Cwrite (uint8_t data) {
  TWDR = data;
  TWCR = 1<<TWINT | 1 << TWEN;
  while (!(TWCR & 1<<TWINT));
  return (TWSR & 0xF8) == TWSR_MTX_DATA_ACK;
}

bool I2Cstart (uint8_t address, uint8_t read) {
  uint8_t addressRW = address<<1 | read;
  TWCR = 1<<TWINT | 1<<TWSTA | 1<<TWEN;    // Send START condition
  while (!(TWCR & 1<<TWINT));
  if ((TWSR & 0xF8) != TWSR_START && (TWSR & 0xF8) != TWSR_REP_START) return false;
  TWDR = addressRW;  // send device address and direction
  TWCR = 1<<TWINT | 1<<TWEN;
  while (!(TWCR & 1<<TWINT));
  if (addressRW & I2C_READ) return (TWSR & 0xF8) == TWSR_MRX_ADR_ACK;
  else return (TWSR & 0xF8) == TWSR_MTX_ADR_ACK;
}

bool I2Crestart (uint8_t address, uint8_t read) {
  return I2Cstart(address, read);
}

void I2Cstop (uint8_t read) {
  (void) read;
  TWCR = 1<<TWINT | 1<<TWEN | 1<<TWSTO;
  while (TWCR & 1<<TWSTO); // wait until stop and bus released
}

// Streams

inline int spiread () { return SPI.transfer(0); }
#if defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
inline int serial1read () { while (!Serial1.available()) testescape(); return Serial1.read(); }
#elif defined(CPU_ATmega2560)
inline int serial1read () { while (!Serial1.available()) testescape(); return Serial1.read(); }
inline int serial2read () { while (!Serial2.available()) testescape(); return Serial2.read(); }
inline int serial3read () { while (!Serial3.available()) testescape(); return Serial3.read(); }
#endif
#if defined(sdcardsupport)
File SDpfile, SDgfile;
inline int SDread () {
  if (LastChar) { 
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  return SDgfile.read();
}
#endif

void serialbegin (int address, int baud) {
  #if defined(CPU_ATmega328P)
  (void) address; (void) baud;
  #elif defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
  if (address == 1) Serial1.begin((long)baud*100);
  else error(WITHSERIAL, PSTR("port not supported"), number(address));
  #elif defined(CPU_ATmega2560)
  if (address == 1) Serial1.begin((long)baud*100);
  else if (address == 2) Serial2.begin((long)baud*100);
  else if (address == 3) Serial3.begin((long)baud*100);
  else error(WITHSERIAL, PSTR("port not supported"), number(address));
  #endif
}

void serialend (int address) {
  #if defined(CPU_ATmega328P)
  (void) address;
  #elif defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
  if (address == 1) {Serial1.flush(); Serial1.end(); }
  #elif defined(CPU_ATmega2560)
  if (address == 1) {Serial1.flush(); Serial1.end(); }
  else if (address == 2) {Serial2.flush(); Serial2.end(); }
  else if (address == 3) {Serial3.flush(); Serial3.end(); }
  #endif
}

gfun_t gstreamfun (object *args) {
  int streamtype = SERIALSTREAM;
  int address = 0;
  gfun_t gfun = gserial;
  if (args != NULL) {
    int stream = isstream(first(args));
    streamtype = stream>>8; address = stream & 0xFF;
  }
  if (streamtype == I2CSTREAM) gfun = (gfun_t)I2Cread;
  else if (streamtype == SPISTREAM) gfun = spiread;
  else if (streamtype == SERIALSTREAM) {
    if (address == 0) gfun = gserial;
    #if defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
    else if (address == 1) gfun = serial1read;
    #elif defined(CPU_ATmega2560)
    else if (address == 1) gfun = serial1read;
    else if (address == 2) gfun = serial2read;
    else if (address == 3) gfun = serial3read;
    #endif
  }
  #if defined(sdcardsupport)
  else if (streamtype == SDSTREAM) gfun = (gfun_t)SDread;
  #endif
  else error2(NIL, unknownstreamtype);
  return gfun;
}

inline void spiwrite (char c) { SPI.transfer(c); }
#if defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
inline void serial1write (char c) { Serial1.write(c); }
#elif defined(CPU_ATmega2560)
inline void serial1write (char c) { Serial1.write(c); }
inline void serial2write (char c) { Serial2.write(c); }
inline void serial3write (char c) { Serial3.write(c); }
#endif
#if defined(sdcardsupport)
inline void SDwrite (char c) { SDpfile.write(c); }
#endif

pfun_t pstreamfun (object *args) {
  int streamtype = SERIALSTREAM;
  int address = 0;
  pfun_t pfun = pserial;
  if (args != NULL && first(args) != NULL) {
    int stream = isstream(first(args));
    streamtype = stream>>8; address = stream & 0xFF;
  }
  if (streamtype == I2CSTREAM) pfun = (pfun_t)I2Cwrite;
  else if (streamtype == SPISTREAM) pfun = spiwrite;
  else if (streamtype == SERIALSTREAM) {
    if (address == 0) pfun = pserial;
    #if defined(CPU_ATmega1284P) || defined(CPU_AVR128DX48)
    else if (address == 1) pfun = serial1write;
    #elif defined(CPU_ATmega2560)
    else if (address == 1) pfun = serial1write;
    else if (address == 2) pfun = serial2write;
    else if (address == 3) pfun = serial3write;
    #endif
  }   
  else if (streamtype == STRINGSTREAM) {
    pfun = pstr;
  }
  #if defined(sdcardsupport)
  else if (streamtype == SDSTREAM) pfun = (pfun_t)SDwrite;
  #endif
  else error2(NIL, unknownstreamtype);
  return pfun;
}

// Check pins

void checkanalogread (int pin) {
  if (!(pin>=0 && pin<=7)) error(ANALOGREAD, invalidpin, number(pin));
}

void checkanalogwrite (int pin) {
  if (!(pin==3 || pin==4 || pin==6 || pin==7 || (pin>=12 && pin<=15))) error(ANALOGWRITE, invalidpin, number(pin));
}

// Note

const int scale[] PROGMEM = {4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902};

void playnote (int pin, int note, int octave) {
  if (pin == 4) {
    int prescaler = 8 - octave - note/12;
    if (prescaler<0 || prescaler>8) error(NOTE, PSTR("octave out of range"), number(prescaler));
    tone(pin, pgm_read_word(&scale[note%12])>>prescaler);
  } else error(NOTE, PSTR("invalid pin"), number(pin));
}

void nonote (int pin) {
  noTone(pin);
}

// Sleep

// Interrupt vector for sleep watchdog
ISR(WDT_vect) {
  WDTCSR |= 1<<WDIE;
}

void initsleep () {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void sleep (int secs) {
  // Set up Watchdog timer for 1 Hz interrupt
  WDTCSR = 1<<WDCE | 1<<WDE;
  WDTCSR = 1<<WDIE | 6<<WDP0;     // 1 sec interrupt
  delay(100);  // Give serial time to settle
  // Disable ADC and timer 0
  ADCSRA = ADCSRA & ~(1<<ADEN);
  PRR0 = PRR0 | 1<<PRTIM0;
  while (secs > 0) {
    sleep_enable();
    sleep_cpu();
    secs--;
  }
  WDTCSR = 1<<WDCE | 1<<WDE;     // Disable watchdog
  WDTCSR = 0;
  // Enable ADC and timer 0
  ADCSRA = ADCSRA | 1<<ADEN;
  PRR0 = PRR0 & ~(1<<PRTIM0);
}

// Prettyprint

const int PPINDENT = 2;
const int PPWIDTH = 42;

void pcount (char c) {
  if (c == '\n') PrintCount++;
  PrintCount++;
}

uint8_t atomwidth (object *obj) {
  PrintCount = 0;
  printobject(obj, pcount);
  return PrintCount;
}

uint8_t basewidth (object *obj, uint8_t base) {
  PrintCount = 0;
  pintbase(obj->integer, base, pcount);
  return PrintCount;
}

bool quoted (object *obj) {
  return (consp(obj) && car(obj) != NULL && car(obj)->name == sym(QUOTE) && consp(cdr(obj)) && cddr(obj) == NULL);
}

int subwidth (object *obj, int w) {
  if (atom(obj)) return w - atomwidth(obj);
  if (quoted(obj)) obj = car(cdr(obj));
  return subwidthlist(obj, w - 1);
}

int subwidthlist (object *form, int w) {
  while (form != NULL && w >= 0) {
    if (atom(form)) return w - (2 + atomwidth(form));
    w = subwidth(car(form), w - 1);
    form = cdr(form);
  }
  return w;
}

void superprint (object *form, int lm, pfun_t pfun) {
  if (atom(form)) {
    if (symbolp(form) && form->name == sym(NOTHING)) printsymbol(form, pfun);
    else printobject(form, pfun);
  }
  else if (quoted(form)) { pfun('\''); superprint(car(cdr(form)), lm + 1, pfun); }
  else if (subwidth(form, PPWIDTH - lm) >= 0) supersub(form, lm + PPINDENT, 0, pfun);
  else supersub(form, lm + PPINDENT, 1, pfun);
}

const int ppspecials = 16;
const char ppspecial[ppspecials] PROGMEM = 
  { DOTIMES, DOLIST, IF, SETQ, TEE, LET, LETSTAR, LAMBDA, WHEN, UNLESS, WITHI2C, WITHSERIAL, WITHSPI, WITHSDCARD, FORMILLIS, DEFVAR };

void supersub (object *form, int lm, int super, pfun_t pfun) {
  int special = 0, separate = 1;
  object *arg = car(form);
  if (symbolp(arg)) {
    symbol_t sname = arg->name;
    #if defined(CODESIZE)
    if (sname == sym(DEFUN) || sname == sym(DEFCODE)) special = 2;
    #else
    if (sname == sym(DEFUN)) special = 2;
    #endif
    else for (int i=0; i<ppspecials; i++) {
      #if defined(CPU_ATmega4809)
      if (sname == sym(ppspecial[i])) { special = 1; break; }    
      #else
      if (sname == sym((builtin_t)pgm_read_byte(&ppspecial[i]))) { special = 1; break; }
      #endif   
    } 
  }
  while (form != NULL) {
    if (atom(form)) { pfstring(PSTR(" . "), pfun); printobject(form, pfun); pfun(')'); return; }
    else if (separate) { pfun('('); separate = 0; }
    else if (special) { pfun(' '); special--; }
    else if (!super) pfun(' ');
    else { pln(pfun); indent(lm, ' ', pfun); }
    superprint(car(form), lm, pfun);
    form = cdr(form);
  }
  pfun(')'); return;
}

object *edit (object *fun) {
  while (1) {
    if (tstflag(EXITEDITOR)) return fun;
    char c = gserial();
    if (c == 'q') setflag(EXITEDITOR);
    else if (c == 'b') return fun;
    else if (c == 'r') fun = read(gserial);
    else if (c == '\n') { pfl(pserial); superprint(fun, 0, pserial); pln(pserial); }
    else if (c == 'c') fun = cons(read(gserial), fun);
    else if (atom(fun)) pserial('!');
    else if (c == 'd') fun = cons(car(fun), edit(cdr(fun)));
    else if (c == 'a') fun = cons(edit(car(fun)), cdr(fun));
    else if (c == 'x') fun = cdr(fun);
    else pserial('?');
  }
}

// Assembler

#if defined(CPU_ATmega1284P)
#define CODE_ADDRESS 0x1bb00
#elif defined(CPU_AVR128DX48)
#define CODE_ADDRESS 0x1be00
#endif

#if defined(CODESIZE)
object *call (int entry, int nargs, object *args, object *env) {
  (void) env;
  int param[4];
  for (int i=0; i<nargs; i++) {
    object *arg = first(args);
    if (integerp(arg)) param[i] = arg->integer;
    else param[i] = (uintptr_t)arg;
    args = cdr(args);
  }
  uint32_t address = (CODE_ADDRESS + entry)>>1; // Code addresses are word addresses on AVR
  int w = ((intfn_ptr_type)address)(param[0], param[1], param[2], param[3]);
  return number(w);
}

void putcode (object *arg, int origin, int pc) {
  int code = checkinteger(DEFCODE, arg);
  uint8_t hi = (code>>8) & 0xff;
  uint8_t lo = code & 0xff; 
  MyCode[origin+pc] = lo;            // Little-endian
  MyCode[origin+pc+1] = hi;
  #if defined(assemblerlist)
  printhex2(pc>>8, pserial); printhex2(pc, pserial); pserial(' ');
  printhex2(lo, pserial); pserial(' '); printhex2(hi, pserial); pserial(' ');
  #endif
}

int assemble (int pass, int origin, object *entries, object *env, object *pcpair) {
  int pc = 0; cdr(pcpair) = number(pc);
  while (entries != NULL) {
    object *arg = first(entries);
    if (symbolp(arg)) {
      if (pass == 2) {
        #if defined(assemblerlist)
        printhex2(pc>>8, pserial); printhex2(pc, pserial);
        indent(7, ' ', pserial);
        printobject(arg, pserial); pln(pserial);
        #endif
      } else {
        object *pair = findvalue(DEFCODE, arg, env);
        cdr(pair) = number(pc);
      }
    } else {
      object *argval = eval(arg, env);
      if (listp(argval)) {
        object *arglist = argval;
        while (arglist != NULL) {
          if (pass == 2) {
            putcode(first(arglist), origin, pc);
            #if defined(assemblerlist)
            if (arglist == argval) superprint(arg, 0, pserial);
            pln(pserial);
            #endif
          }
          pc = pc + 2;
          cdr(pcpair) = number(pc);
          arglist = cdr(arglist);
        }
      } else if (integerp(argval)) {
        if (pass == 2) {
          putcode(argval, origin, pc);
          #if defined(assemblerlist)
          superprint(arg, 0, pserial); pln(pserial);
          #endif
        }
        pc = pc + 2;
        cdr(pcpair) = number(pc);
      } else error(DEFCODE, PSTR("illegal entry"), arg);
    }
    entries = cdr(entries);
  }
  // Round up to multiple of 2 to give code size
  if (pc%2 != 0) pc = pc + 2 - pc%2;
  return pc;
}
#endif

// Special forms

object *sp_quote (object *args, object *env) {
  (void) env;
  checkargs(QUOTE, args);
  return first(args);
}

object *sp_or (object *args, object *env) {
  while (args != NULL) {
    object *val = eval(car(args), env);
    if (val != NULL) return val;
    args = cdr(args);
  }
  return nil;
}

object *sp_defun (object *args, object *env) {
  (void) env;
  checkargs(DEFUN, args);
  object *var = first(args);
  if (!symbolp(var)) error(DEFUN, notasymbol, var);
  object *val = cons(bsymbol(LAMBDA), cdr(args));
  object *pair = value(var->name, GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);
  return var;
}

object *sp_defvar (object *args, object *env) {
  checkargs(DEFVAR, args);
  object *var = first(args);
  if (!symbolp(var)) error(DEFVAR, notasymbol, var);
  object *val = NULL;
  args = cdr(args);
  if (args != NULL) { setflag(NOESC); val = eval(first(args), env); clrflag(NOESC); }
  object *pair = value(var->name, GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);
  return var;
}

object *sp_setq (object *args, object *env) {
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETQ, oddargs);
    object *pair = findvalue(SETQ, first(args), env);
    arg = eval(second(args), env);
    cdr(pair) = arg;
    args = cddr(args);
  }
  return arg;
}

object *sp_loop (object *args, object *env) {
  object *start = args;
  for (;;) {
    args = start;
    while (args != NULL) {
      object *result = eval(car(args),env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        return result;
      }
      args = cdr(args);
    }
  }
}

object *sp_return (object *args, object *env) {
  object *result = eval(tf_progn(args,env), env);
  setflag(RETURNFLAG);
  return result;
}

object *sp_push (object *args, object *env) {
  int bit;
  checkargs(PUSH, args);
  object *item = eval(first(args), env);
  object **loc = place(PUSH, second(args), env, &bit);
  push(item, *loc);
  return *loc;
}

object *sp_pop (object *args, object *env) {
  int bit;
  checkargs(POP, args);
  object **loc = place(POP, first(args), env, &bit);
  object *result = car(*loc);
  pop(*loc);
  return result;
}

// Accessors

object *sp_incf (object *args, object *env) {
  return incfdecf(INCF, args, 1, env);
}

object *sp_decf (object *args, object *env) {
  return incfdecf(DECF, args, -1, env);
}

object *sp_setf (object *args, object *env) {
  int bit;
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETF, oddargs);
    object **loc = place(SETF, first(args), env, &bit);
    arg = eval(second(args), env);
    if (bit == -1) *loc = arg;
    else *loc = number((checkinteger(SETF,*loc) & ~(1<<bit)) | checkbitvalue(SETF,arg)<<bit);
    args = cddr(args);
  }
  return arg;
}

// Other special forms

object *sp_dolist (object *args, object *env) {
  if (args == NULL || listlength(DOLIST, first(args)) < 2) error2(DOLIST, noargument);
  object *params = first(args);
  object *var = first(params);
  object *list = eval(second(params), env);
  push(list, GCStack); // Don't GC the list
  object *pair = cons(var,nil);
  push(pair,env);
  params = cdr(cdr(params));
  args = cdr(args);
  while (list != NULL) {
    if (improperp(list)) error(DOLIST, notproper, list);
    cdr(pair) = first(list);
    object *forms = args;
    while (forms != NULL) {
      object *result = eval(car(forms), env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        pop(GCStack);
        return result;
      }
      forms = cdr(forms);
    }
    list = cdr(list);
  }
  cdr(pair) = nil;
  pop(GCStack);
  if (params == NULL) return nil;
  return eval(car(params), env);
}

object *sp_dotimes (object *args, object *env) {
  if (args == NULL || listlength(DOTIMES, first(args)) < 2) error2(DOTIMES, noargument);
  object *params = first(args);
  object *var = first(params);
  int count = checkinteger(DOTIMES, eval(second(params), env));
  int index = 0;
  params = cdr(cdr(params));
  object *pair = cons(var,number(0));
  push(pair,env);
  args = cdr(args);
  while (index < count) {
    cdr(pair) = number(index);
    object *forms = args;
    while (forms != NULL) {
      object *result = eval(car(forms), env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        return result;
      }
      forms = cdr(forms);
    }
    index++;
  }
  cdr(pair) = number(index);
  if (params == NULL) return nil;
  return eval(car(params), env);
}

object *sp_trace (object *args, object *env) {
  (void) env;
  while (args != NULL) {
    object *var = first(args);
    if (!symbolp(var)) error(TRACE, notasymbol, var);
    trace(var->name);
    args = cdr(args);
  }
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] != 0) args = cons(symbol(TraceFn[i]), args);
    i++;
  }
  return args;
}

object *sp_untrace (object *args, object *env) {
  (void) env;
  if (args == NULL) {
    int i = 0;
    while (i < TRACEMAX) {
      if (TraceFn[i] != 0) args = cons(symbol(TraceFn[i]), args);
      TraceFn[i] = 0;
      i++;
    }
  } else {
    while (args != NULL) {
      object *var = first(args);
      if (!symbolp(var)) error(UNTRACE, notasymbol, var);
      untrace(var->name);
      args = cdr(args);
    }
  }
  return args;
}

object *sp_formillis (object *args, object *env) {
  if (args == NULL) error2(FORMILLIS, noargument);
  object *param = first(args);
  unsigned long start = millis();
  unsigned long now, total = 0;
  if (param != NULL) total = checkinteger(FORMILLIS, eval(first(param), env));
  eval(tf_progn(cdr(args),env), env);
  do {
    now = millis() - start;
    testescape();
  } while (now < total);
  if (now <= INT_MAX) return number(now);
  return nil;
}

object *sp_time (object *args, object *env) {
  unsigned long start = millis();
  object *result = eval(first(args), env);
  unsigned long elapsed = millis() - start;
  printobject(result, pserial);
  pfstring(PSTR("\nTime: "), pserial);
  if (elapsed < 1000) {
    pint(elapsed, pserial);
    pfstring(PSTR(" ms\n"), pserial);
  } else {
    elapsed = elapsed+50;
    pint(elapsed/1000, pserial);
    pserial('.'); pint((elapsed/100)%10, pserial);
    pfstring(PSTR(" s\n"), pserial);
  }
  return bsymbol(NOTHING);
}

object *sp_withserial (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHSERIAL, nostream);
  object *var = first(params);
  int address = checkinteger(WITHSERIAL, eval(second(params), env));
  params = cddr(params);
  int baud = 96;
  if (params != NULL) baud = checkinteger(WITHSERIAL, eval(first(params), env));
  object *pair = cons(var, stream(SERIALSTREAM, address));
  push(pair,env);
  serialbegin(address, baud);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  serialend(address);
  return result;
}

object *sp_withi2c (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHI2C, nostream);
  object *var = first(params);
  int address = checkinteger(WITHI2C, eval(second(params), env));
  params = cddr(params);
  if (address == 0 && params != NULL) params = cdr(params); // Ignore port
  int read = 0; // Write
  I2Ccount = 0;
  if (params != NULL) {
    object *rw = eval(first(params), env);
    if (integerp(rw)) I2Ccount = rw->integer;
    read = (rw != NULL);
  }
  I2Cinit(1); // Pullups
  object *pair = cons(var, (I2Cstart(address, read)) ? stream(I2CSTREAM, address) : nil);
  push(pair,env);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  I2Cstop(read);
  return result;
}

object *sp_withspi (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHSPI, nostream);
  object *var = first(params);
  params = cdr(params);
  if (params == NULL) error2(WITHSPI, nostream);
  int pin = checkinteger(WITHSPI, eval(car(params), env));
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  params = cdr(params);
  int clock = 4000, mode = SPI_MODE0; // Defaults
  int bitorder = MSBFIRST;
  if (params != NULL) {
    clock = checkinteger(WITHSPI, eval(car(params), env));
    params = cdr(params);
    if (params != NULL) {
      bitorder = (checkinteger(WITHSPI, eval(car(params), env)) == 0) ? LSBFIRST : MSBFIRST;
      params = cdr(params);
      if (params != NULL) {
        int modeval = checkinteger(WITHSPI, eval(car(params), env));
        mode = (modeval == 3) ? SPI_MODE3 : (modeval == 2) ? SPI_MODE2 : (modeval == 1) ? SPI_MODE1 : SPI_MODE0;
      }
    }
  }
  object *pair = cons(var, stream(SPISTREAM, pin));
  push(pair,env);
  SPI.begin();
  SPI.beginTransaction(SPISettings(((unsigned long)clock * 1000), bitorder, mode));
  digitalWrite(pin, LOW);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  digitalWrite(pin, HIGH);
  SPI.endTransaction();
  return result;
}

object *sp_withsdcard (object *args, object *env) {
  #if defined(sdcardsupport)
  object *params = first(args);
  if (params == NULL) error2(WITHSDCARD, nostream);
  object *var = first(params);
  object *filename = eval(second(params), env);
  params = cddr(params);
  SD.begin(SDCARD_SS_PIN);
  int mode = 0;
  if (params != NULL && first(params) != NULL) mode = checkinteger(WITHSDCARD, first(params));
  int oflag = O_READ;
  if (mode == 1) oflag = O_RDWR | O_CREAT | O_APPEND; else if (mode == 2) oflag = O_RDWR | O_CREAT | O_TRUNC;
  if (mode >= 1) {
    char buffer[BUFFERSIZE];
    SDpfile = SD.open(MakeFilename(filename, buffer), oflag);
    if (!SDpfile) error2(WITHSDCARD, PSTR("problem writing to SD card or invalid filename"));
  } else {
    char buffer[BUFFERSIZE];
    SDgfile = SD.open(MakeFilename(filename, buffer), oflag);
    if (!SDgfile) error2(WITHSDCARD, PSTR("problem reading from SD card or invalid filename"));
  }
  object *pair = cons(var, stream(SDSTREAM, 1));
  push(pair,env);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  if (mode >= 1) SDpfile.close(); else SDgfile.close();
  return result;
  #else
  (void) args, (void) env;
  error2(WITHSDCARD, PSTR("not supported"));
  return nil;
  #endif
}

// Assembler

object *sp_defcode (object *args, object *env) {
#if defined(CODESIZE)
  setflag(NOESC);
  checkargs(DEFCODE, args);
  object *var = first(args);
  if (!symbolp(var)) error(DEFCODE, PSTR("not a symbol"), var);

  // Make *p* a local variable for program counter
  object *pcpair = cons(bsymbol(PSTAR), number(0));
  push(pcpair,env);
  args = cdr(args);

  // Make labels into local variables
  object *entries = cdr(args);
  while (entries != NULL) {
    object *arg = first(entries);
    if (symbolp(arg)) {
      object *pair = cons(arg,number(0));
      push(pair,env);
    }
    entries = cdr(entries);
  }

  // First pass
  int origin = 0;
  int codesize = assemble(1, origin, cdr(args), env, pcpair);

  // See if it will fit
  object *globals = GlobalEnv;
  while (globals != NULL) {
    object *pair = car(globals);
    if (pair != NULL && car(pair) != var && consp(cdr(pair))) { // Exclude me if I already exist
      object *codeid = second(pair);
      if (codeid->type == CODE) {
        codesize = codesize + endblock(codeid) - startblock(codeid);
      }
    }
    globals = cdr(globals);
  }
  if (codesize > CODESIZE) error(DEFCODE, PSTR("not enough room for code"), var);

  // Compact the code block, removing gaps
  origin = 0;
  object *block;
  int smallest;

  do {
    smallest = CODESIZE;
    globals = GlobalEnv;
    while (globals != NULL) {
      object *pair = car(globals);
      if (pair != NULL && car(pair) != var && consp(cdr(pair))) { // Exclude me if I already exist
        object *codeid = second(pair);
        if (codeid->type == CODE) {
          if (startblock(codeid) < smallest && startblock(codeid) >= origin) {
            smallest = startblock(codeid);
            block = codeid;
          }
        }
      }
      globals = cdr(globals);
    }

    // Compact fragmentation if necessary
    if (smallest == origin) origin = endblock(block); // No gap
    else if (smallest < CODESIZE) { // Slide block down
      int target = origin;
      for (int i=startblock(block); i<endblock(block); i++) {
        MyCode[target] = MyCode[i];
        target++;
      }
      block->integer = target<<8 | origin;
      origin = target;
    }

  } while (smallest < CODESIZE);

  // Second pass - origin is first free location
  codesize = assemble(2, origin, cdr(args), env, pcpair);

  object *val = cons(codehead((origin+codesize)<<8 | origin), args);
  object *pair = value(var->name, GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);

  #if defined(CPU_ATmega1284P)
  // Use Optiboot Flasher in MightyCore with 256 byte page from CODE_ADDRESS 0x1bb00 to 0x1bbff
  optiboot_page_erase(CODE_ADDRESS);
  for (unsigned int i=0; i<CODESIZE/2; i++) optiboot_page_fill(CODE_ADDRESS + i*2, MyCode[i*2] | MyCode[i*2+1]<<8);
  optiboot_page_write(CODE_ADDRESS);
  #elif defined (CPU_AVR128DX48)
  // Use Flash Writer in DxCore with 512 byte page from CODE_ADDRESS 0x1be00 to 0x1c000
  if (Flash.checkWritable()) error2(DEFCODE, PSTR("flash write not supported"));
  if (Flash.erasePage(CODE_ADDRESS, 1)) error2(DEFCODE, PSTR("problem erasing flash"));
  Flash.writeBytes(CODE_ADDRESS, MyCode, CODESIZE);
  #endif
  
  clrflag(NOESC);
  return var;
#else
  (void) args, (void) env;
  return nil;
#endif
}

// Tail-recursive forms

object *tf_progn (object *args, object *env) {
  if (args == NULL) return nil;
  object *more = cdr(args);
  while (more != NULL) {
    object *result = eval(car(args),env);
    if (tstflag(RETURNFLAG)) return result;
    args = more;
    more = cdr(args);
  }
  return car(args);
}

object *tf_if (object *args, object *env) {
  if (args == NULL || cdr(args) == NULL) error2(IF, toofewargs);
  if (eval(first(args), env) != nil) return second(args);
  args = cddr(args);
  return (args != NULL) ? first(args) : nil;
}

object *tf_cond (object *args, object *env) {
  while (args != NULL) {
    object *clause = first(args);
    if (!consp(clause)) error(COND, illegalclause, clause);
    object *test = eval(first(clause), env);
    object *forms = cdr(clause);
    if (test != nil) {
      if (forms == NULL) return quote(test); else return tf_progn(forms, env);
    }
    args = cdr(args);
  }
  return nil;
}

object *tf_when (object *args, object *env) {
  if (args == NULL) error2(WHEN, noargument);
  if (eval(first(args), env) != nil) return tf_progn(cdr(args),env);
  else return nil;
}

object *tf_unless (object *args, object *env) {
  if (args == NULL) error2(UNLESS, noargument);
  if (eval(first(args), env) != nil) return nil;
  else return tf_progn(cdr(args),env);
}

object *tf_case (object *args, object *env) {
  object *test = eval(first(args), env);
  args = cdr(args);
  while (args != NULL) {
    object *clause = first(args);
    if (!consp(clause)) error(CASE, illegalclause, clause);
    object *key = car(clause);
    object *forms = cdr(clause);
    if (consp(key)) {
      while (key != NULL) {
        if (eq(test,car(key))) return tf_progn(forms, env);
        key = cdr(key);
      }
    } else if (eq(test,key) || eq(key,tee)) return tf_progn(forms, env);
    args = cdr(args);
  }
  return nil;
}

object *tf_and (object *args, object *env) {
  if (args == NULL) return tee;
  object *more = cdr(args);
  while (more != NULL) {
    if (eval(car(args), env) == NULL) return nil;
    args = more;
    more = cdr(args);
  }
  return car(args);
}

object *tf_help (object *args, object *env) {
  if (args == NULL) error2(HELP, noargument);
  object *docstring = documentation(HELP, first(args), env);
  if (docstring) {
    char temp = Flags;
    clrflag(PRINTREADABLY);
    printstring(docstring, pserial);
    Flags = temp;
  }
  return bsymbol(NOTHING);
}

// Core functions

object *fn_not (object *args, object *env) {
  (void) env;
  return (first(args) == nil) ? tee : nil;
}

object *fn_cons (object *args, object *env) {
  (void) env;
  return cons(first(args), second(args));
}

object *fn_atom (object *args, object *env) {
  (void) env;
  return atom(first(args)) ? tee : nil;
}

object *fn_listp (object *args, object *env) {
  (void) env;
  return listp(first(args)) ? tee : nil;
}

object *fn_consp (object *args, object *env) {
  (void) env;
  return consp(first(args)) ? tee : nil;
}

object *fn_symbolp (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  return (arg == NULL || symbolp(arg)) ? tee : nil;
}

object *fn_boundp (object *args, object *env) {
  (void) env;
  object *var = first(args);
  if (!symbolp(var)) error(BOUNDP, notasymbol, var);
  return boundp(var, env) ? tee : nil;
}

object *fn_setfn (object *args, object *env) {
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETFN, oddargs);
    object *pair = findvalue(SETFN, first(args), env);
    arg = second(args);
    cdr(pair) = arg;
    args = cddr(args);
  }
  return arg;
}

object *fn_streamp (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  return streamp(arg) ? tee : nil;
}

object *fn_eq (object *args, object *env) {
  (void) env;
  return eq(first(args), second(args)) ? tee : nil;
}

// List functions

object *fn_car (object *args, object *env) {
  (void) env;
  return carx(first(args));
}

object *fn_cdr (object *args, object *env) {
  (void) env;
  return cdrx(first(args));
}

object *fn_caar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b100);
}

object *fn_cadr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b101);
}

object *fn_cdar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b110);
}

object *fn_cddr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b111);
}

object *fn_caaar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1000);
}

object *fn_caadr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1001);;
}

object *fn_cadar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1010);
}

object *fn_caddr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1011);
}

object *fn_cdaar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1100);
}

object *fn_cdadr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1101);
}

object *fn_cddar (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1110);
}

object *fn_cdddr (object *args, object *env) {
  (void) env;
  return cxxxr(args, 0b1111);
}

object *fn_length (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (listp(arg)) return number(listlength(LENGTH, arg));
  if (stringp(arg)) return number(stringlength(arg));
  if (!(arrayp(arg) && cdr(cddr(arg)) == NULL)) error(LENGTH, PSTR("argument is not a list, 1d array, or string"), arg);
  return number(abs(first(cddr(arg))->integer));
}

object *fn_arraydimensions (object *args, object *env) {
  (void) env;
  object *array = first(args);
  if (!arrayp(array)) error(ARRAYDIMENSIONS, PSTR("argument is not an array"), array);
  object *dimensions = cddr(array);
  return (first(dimensions)->integer < 0) ? cons(number(-(first(dimensions)->integer)), cdr(dimensions)) : dimensions;
}

object *fn_list (object *args, object *env) {
  (void) env;
  return args;
}

object *fn_makearray (object *args, object *env) {
  (void) env;
  object *def = nil;
  bool bitp = false;
  object *dims = first(args);
  if (dims == NULL) error2(MAKEARRAY, PSTR("dimensions can't be nil"));
  else if (atom(dims)) dims = cons(dims, NULL);
  args = cdr(args);
  while (args != NULL && cdr(args) != NULL) {
    object *var = first(args);
    if (isbuiltin(first(args), INITIALELEMENT)) def = second(args);
    else if (isbuiltin(first(args), ELEMENTTYPE) && isbuiltin(second(args), BIT)) bitp = true;
    else error(MAKEARRAY, PSTR("argument not recognised"), var);
    args = cddr(args);
  }
  if (bitp) {
    if (def == nil) def = number(0);
    else def = number(-checkbitvalue(MAKEARRAY, def)); // 1 becomes all ones
  }
  return makearray(MAKEARRAY, dims, def, bitp);
}

object *fn_reverse (object *args, object *env) {
  (void) env;
  object *list = first(args);
  object *result = NULL;
  while (list != NULL) {
    if (improperp(list)) error(REVERSE, notproper, list);
    push(first(list),result);
    list = cdr(list);
  }
  return result;
}

object *fn_nth (object *args, object *env) {
  (void) env;
  int n = checkinteger(NTH, first(args));
  if (n < 0) error(NTH, indexnegative, first(args));
  object *list = second(args);
  while (list != NULL) {
    if (improperp(list)) error(NTH, notproper, list);
    if (n == 0) return car(list);
    list = cdr(list);
    n--;
  }
  return nil;
}

object *fn_aref (object *args, object *env) {
  (void) env;
  int bit;
  object *array = first(args);
  if (!arrayp(array)) error(AREF, PSTR("first argument is not an array"), array);
  object *loc = *getarray(AREF, array, cdr(args), 0, &bit);
  if (bit == -1) return loc;
  else return number((loc->integer)>>bit & 1);
}

object *fn_assoc (object *args, object *env) {
  (void) env;
  object *key = first(args);
  object *list = second(args);
  return assoc(key,list);
}

object *fn_member (object *args, object *env) {
  (void) env;
  object *item = first(args);
  object *list = second(args);
  while (list != NULL) {
    if (improperp(list)) error(MEMBER, notproper, list);
    if (eq(item,car(list))) return list;
    list = cdr(list);
  }
  return nil;
}

object *fn_apply (object *args, object *env) {
  object *previous = NULL;
  object *last = args;
  while (cdr(last) != NULL) {
    previous = last;
    last = cdr(last);
  }
  object *arg = car(last);
  if (!listp(arg)) error(APPLY, notalist, arg);
  cdr(previous) = arg;
  return apply(APPLY, first(args), cdr(args), env);
}

object *fn_funcall (object *args, object *env) {
  return apply(FUNCALL, first(args), cdr(args), env);
}

object *fn_append (object *args, object *env) {
  (void) env;
  object *head = NULL;
  object *tail;
  while (args != NULL) {
    object *list = first(args);
    if (!listp(list)) error(APPEND, notalist, list);
    while (consp(list)) {
      object *obj = cons(car(list), cdr(list));
      if (head == NULL) head = obj;
      else cdr(tail) = obj;
      tail = obj;
      list = cdr(list);
      if (cdr(args) != NULL && improperp(list)) error(APPEND, notproper, first(args));
    }
    args = cdr(args);
  }
  return head;
}

object *fn_mapc (object *args, object *env) {
  object *function = first(args);
  args = cdr(args);
  object *result = first(args);
  object *params = cons(NULL, NULL);
  push(params,GCStack);
  // Make parameters
  while (true) {
    object *tailp = params;
    object *lists = args;
    while (lists != NULL) {
      object *list = car(lists);
      if (list == NULL) {
         pop(GCStack);
         return result;
      }
      if (improperp(list)) error(MAPC, notproper, list);
      object *obj = cons(first(list),NULL);
      car(lists) = cdr(list);
      cdr(tailp) = obj; tailp = obj;
      lists = cdr(lists);
    }
    apply(MAPC, function, cdr(params), env);
  }
}

object *fn_mapcar (object *args, object *env) {
  return mapcarcan(MAPCAR, args, env, mapcarfun);
}

object *fn_mapcan (object *args, object *env) {
  return mapcarcan(MAPCAN, args, env, mapcanfun);
}

// Arithmetic functions

object *fn_add (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    int temp = checkinteger(ADD, car(args));
    #if defined(checkoverflow)
    if (temp < 1) { if (INT_MIN - temp > result) error2(ADD, overflow); }
    else { if (INT_MAX - temp < result) error2(ADD, overflow); }
    #endif
    result = result + temp;
    args = cdr(args);
  }
  return number(result);
}

object *fn_subtract (object *args, object *env) {
  (void) env;
  int result = checkinteger(SUBTRACT, car(args));
  args = cdr(args);
  if (args == NULL) {
    #if defined(checkoverflow)
    if (result == INT_MIN) error2(SUBTRACT, overflow);
    #endif
    return number(-result);
  }
  while (args != NULL) {
    int temp = checkinteger(SUBTRACT, car(args));
    #if defined(checkoverflow)
    if (temp < 1) { if (INT_MAX + temp < result) error2(SUBTRACT, overflow); }
    else { if (INT_MIN + temp > result) error2(SUBTRACT, overflow); }
    #endif
    result = result - temp;
    args = cdr(args);
  }
  return number(result);
}

object *fn_multiply (object *args, object *env) {
  (void) env;
  int result = 1;
  while (args != NULL){
    #if defined(checkoverflow)
    signed long temp = (signed long) result * checkinteger(MULTIPLY, car(args));
    if ((temp > INT_MAX) || (temp < INT_MIN)) error2(MULTIPLY, overflow);
    result = temp;
    #else
    result = result * checkinteger(MULTIPLY, car(args));
    #endif
    args = cdr(args);
  }
  return number(result);
}

object *fn_divide (object *args, object *env) {
  (void) env;
  int result = checkinteger(DIVIDE, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg = checkinteger(DIVIDE, car(args));
    if (arg == 0) error2(DIVIDE, divisionbyzero);
    #if defined(checkoverflow)
    if ((result == INT_MIN) && (arg == -1)) error2(DIVIDE, overflow);
    #endif
    result = result / arg;
    args = cdr(args);
  }
  return number(result);
}

object *fn_mod (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(MOD, first(args));
  int arg2 = checkinteger(MOD, second(args));
  if (arg2 == 0) error2(MOD, divisionbyzero);
  int r = arg1 % arg2;
  if ((arg1<0) != (arg2<0)) r = r + arg2;
  return number(r);
}

object *fn_oneplus (object *args, object *env) {
  (void) env;
  int result = checkinteger(ONEPLUS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MAX) error2(ONEPLUS, overflow);
  #endif
  return number(result + 1);
}

object *fn_oneminus (object *args, object *env) {
  (void) env;
  int result = checkinteger(ONEMINUS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MIN) error2(ONEMINUS, overflow);
  #endif
  return number(result - 1);
}

object *fn_abs (object *args, object *env) {
  (void) env;
  int result = checkinteger(ABS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MIN) error2(ABS, overflow);
  #endif
  return number(abs(result));
}

object *fn_random (object *args, object *env) {
  (void) env;
  int arg = checkinteger(RANDOM, first(args));
  return number(pseudoRandom(arg));
}

object *fn_maxfn (object *args, object *env) {
  (void) env;
  int result = checkinteger(MAXFN, first(args));
  args = cdr(args);
  while (args != NULL) {
    int next = checkinteger(MAXFN, car(args));
    if (next > result) result = next;
    args = cdr(args);
  }
  return number(result);
}

object *fn_minfn (object *args, object *env) {
  (void) env;
  int result = checkinteger(MINFN, first(args));
  args = cdr(args);
  while (args != NULL) {
    int next = checkinteger(MINFN, car(args));
    if (next < result) result = next;
    args = cdr(args);
  }
  return number(result);
}

// Arithmetic comparisons

object *fn_noteq (object *args, object *env) {
  (void) env;
  while (args != NULL) {   
    object *nargs = args;
    int arg1 = checkinteger(NOTEQ, first(nargs));
    nargs = cdr(nargs);
    while (nargs != NULL) {
       int arg2 = checkinteger(NOTEQ, first(nargs));
       if (arg1 == arg2) return nil;
       nargs = cdr(nargs);
    }
    args = cdr(args);
  }
  return tee;
}

object *fn_numeq (object *args, object *env) {
  (void) env;
  return compare(NUMEQ, args, false, false, true);
}

object *fn_less (object *args, object *env) {
  (void) env;
  return compare(LESS, args, true, false, false);
}

object *fn_lesseq (object *args, object *env) {
  (void) env;
  return compare(LESSEQ, args, true, false, true);
}

object *fn_greater (object *args, object *env) {
  (void) env;
  return compare(GREATER, args, false, true, false);
}

object *fn_greatereq (object *args, object *env) {
  (void) env;
  return compare(GREATEREQ, args, false, true, true);
}

object *fn_plusp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(PLUSP, first(args));
  if (arg > 0) return tee;
  else return nil;
}

object *fn_minusp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(MINUSP, first(args));
  if (arg < 0) return tee;
  else return nil;
}

object *fn_zerop (object *args, object *env) {
  (void) env;
  int arg = checkinteger(ZEROP, first(args));
  return (arg == 0) ? tee : nil;
}

object *fn_oddp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(ODDP, first(args));
  return ((arg & 1) == 1) ? tee : nil;
}

object *fn_evenp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(EVENP, first(args));
  return ((arg & 1) == 0) ? tee : nil;
}

// Number functions

object *fn_integerp (object *args, object *env) {
  (void) env;
  return integerp(first(args)) ? tee : nil;
}

// Characters

object *fn_char (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (!stringp(arg)) error(CHAR, notastring, arg);
  object *n = second(args);
  char c = nthchar(arg, checkinteger(CHAR, n));
  if (c == 0) error(CHAR, indexrange, n);
  return character(c);
}

object *fn_charcode (object *args, object *env) {
  (void) env;
  return number(checkchar(CHARCODE, first(args)));
}

object *fn_codechar (object *args, object *env) {
  (void) env;
  return character(checkinteger(CODECHAR, first(args)));
}

object *fn_characterp (object *args, object *env) {
  (void) env;
  return characterp(first(args)) ? tee : nil;
}

// Strings

object *fn_stringp (object *args, object *env) {
  (void) env;
  return stringp(first(args)) ? tee : nil;
}

object *fn_stringeq (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGEQ, args, false, false, true) ? tee : nil;
}

object *fn_stringless (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGLESS, args, true, false, false) ? tee : nil;
}

object *fn_stringgreater (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGGREATER, args, false, true, false) ? tee : nil;
}

object *fn_sort (object *args, object *env) {
  if (first(args) == NULL) return nil;
  object *list = cons(nil,first(args));
  push(list,GCStack);
  object *predicate = second(args);
  object *compare = cons(NULL, cons(NULL, NULL));
  push(compare,GCStack);
  object *ptr = cdr(list);
  while (cdr(ptr) != NULL) {
    object *go = list;
    while (go != ptr) {
      car(compare) = car(cdr(ptr));
      car(cdr(compare)) = car(cdr(go));
      if (apply(SORT, predicate, compare, env)) break;
      go = cdr(go);
    }
    if (go != ptr) {
      object *obj = cdr(ptr);
      cdr(ptr) = cdr(obj);
      cdr(obj) = cdr(go);
      cdr(go) = obj;
    } else ptr = cdr(ptr);
  }
  pop(GCStack); pop(GCStack);
  return cdr(list);
}

object *fn_stringfn (object *args, object *env) {
  return fn_princtostring(args, env);
}

object *fn_concatenate (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (builtin(arg->name) != STRINGFN) error2(CONCATENATE, PSTR("only supports strings"));
  args = cdr(args);
  object *result = newstring();
  object *tail = result;
  while (args != NULL) {
    object *obj = checkstring(CONCATENATE, first(args));
    obj = cdr(obj);
    while (obj != NULL) {
      int quad = obj->chars;
      while (quad != 0) {
         char ch = quad>>((sizeof(int)-1)*8) & 0xFF;
         buildstring(ch, &tail);
         quad = quad<<8;
      }
      obj = car(obj);
    }
    args = cdr(args);
  }
  return result;
}

object *fn_subseq (object *args, object *env) {
  (void) env;
  object *arg = checkstring(SUBSEQ, first(args));
  int start = checkinteger(SUBSEQ, second(args));
  if (start < 0) error(SUBSEQ, indexnegative, second(args));
  int end;
  args = cddr(args);
  if (args != NULL) end = checkinteger(SUBSEQ, car(args)); else end = stringlength(arg);
  object *result = newstring();
  object *tail = result;
  for (int i=start; i<end; i++) {
    char ch = nthchar(arg, i);
    if (ch == 0) error2(SUBSEQ, indexrange);
    buildstring(ch, &tail);
  }
  return result;
}

object *fn_readfromstring (object *args, object *env) {
  (void) env;
  object *arg = checkstring(READFROMSTRING, first(args));
  GlobalString = arg;
  GlobalStringIndex = 0;
  return read(gstr);
}

object *fn_princtostring (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  object *obj = startstring(PRINCTOSTRING);
  prin1object(arg, pstr);
  return obj;
}

object *fn_prin1tostring (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  object *obj = startstring(PRIN1TOSTRING);
  printobject(arg, pstr);
  return obj;
}

// Bitwise operators

object *fn_logand (object *args, object *env) {
  (void) env;
  int result = -1;
  while (args != NULL) {
    result = result & checkinteger(LOGAND, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_logior (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    result = result | checkinteger(LOGIOR, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_logxor (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    result = result ^ checkinteger(LOGXOR, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_lognot (object *args, object *env) {
  (void) env;
  int result = checkinteger(LOGNOT, car(args));
  return number(~result);
}

object *fn_ash (object *args, object *env) {
  (void) env;
  int value = checkinteger(ASH, first(args));
  int count = checkinteger(ASH, second(args));
  if (count >= 0) return number(value << count);
  else return number(value >> abs(count));
}

object *fn_logbitp (object *args, object *env) {
  (void) env;
  int index = checkinteger(LOGBITP, first(args));
  int value = checkinteger(LOGBITP, second(args));
  return (bitRead(value, index) == 1) ? tee : nil;
}

// System functions

object *fn_eval (object *args, object *env) {
  return eval(first(args), env);
}

object *fn_globals (object *args, object *env) {
  (void) args;
  if (GlobalEnv == NULL) return nil;
  return fn_mapcar(cons(bsymbol(CAR),cons(GlobalEnv,nil)), env);
}

object *fn_locals (object *args, object *env) {
  (void) args;
  return env;
}

object *fn_makunbound (object *args, object *env) {
  (void) env;
  object *var = first(args);
  if (!symbolp(var)) error(MAKUNBOUND, notasymbol, var);
  delassoc(var, &GlobalEnv);
  return var;
}

object *fn_break (object *args, object *env) {
  (void) args;
  pfstring(PSTR("\nBreak!\n"), pserial);
  BreakLevel++;
  repl(env);
  BreakLevel--;
  return nil;
}

object *fn_read (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  return read(gfun);
}

object *fn_prin1 (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  printobject(obj, pfun);
  return obj;
}

object *fn_print (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  pln(pfun);
  printobject(obj, pfun);
  pfun(' ');
  return obj;
}

object *fn_princ (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  prin1object(obj, pfun);
  return obj;
}

object *fn_terpri (object *args, object *env) {
  (void) env;
  pfun_t pfun = pstreamfun(args);
  pln(pfun);
  return nil;
}

object *fn_readbyte (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  int c = gfun();
  return (c == -1) ? nil : number(c);
}

object *fn_readline (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  return readstring('\n', gfun);
}

object *fn_writebyte (object *args, object *env) {
  (void) env;
  int value = checkinteger(WRITEBYTE, first(args));
  pfun_t pfun = pstreamfun(cdr(args));
  (pfun)(value);
  return nil;
}

object *fn_writestring (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printstring(obj, pfun);
  Flags = temp;
  return nil;
}

object *fn_writeline (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printstring(obj, pfun);
  pln(pfun);
  Flags = temp;
  return nil;
}

object *fn_restarti2c (object *args, object *env) {
  (void) env;
  int stream = first(args)->integer;
  args = cdr(args);
  int read = 0; // Write
  I2Ccount = 0;
  if (args != NULL) {
    object *rw = first(args);
    if (integerp(rw)) I2Ccount = rw->integer;
    read = (rw != NULL);
  }
  int address = stream & 0xFF;
  if (stream>>8 != I2CSTREAM) error2(RESTARTI2C, PSTR("not an i2c stream"));
  return I2Crestart(address, read) ? tee : nil;
}

object *fn_gc (object *obj, object *env) {
  int initial = Freespace;
  unsigned long start = micros();
  gc(obj, env);
  unsigned long elapsed = micros() - start;
  pfstring(PSTR("Space: "), pserial);
  pint(Freespace - initial, pserial);
  pfstring(PSTR(" bytes, Time: "), pserial);
  pint(elapsed, pserial);
  pfstring(PSTR(" us\n"), pserial);
  return nil;
}

object *fn_room (object *args, object *env) {
  (void) args, (void) env;
  return number(Freespace);
}

object *fn_saveimage (object *args, object *env) {
  if (args != NULL) args = eval(first(args), env);
  return number(saveimage(args));
}

object *fn_loadimage (object *args, object *env) {
  (void) env;
  if (args != NULL) args = first(args);
  return number(loadimage(args));
}

object *fn_cls (object *args, object *env) {
  (void) args, (void) env;
  pserial(12);
  return nil;
}

// Arduino procedures

object *fn_pinmode (object *args, object *env) {
  (void) env; int pin;
  object *arg = first(args);
  if (keywordp(arg)) pin = checkkeyword(NIL, arg);
  else pin = checkinteger(PINMODE, first(args));
  int pm = INPUT;
  arg = second(args);
  if (keywordp(arg)) pm = checkkeyword(PINMODE, arg);
  else if (integerp(arg)) {
    int mode = arg->integer;
    if (mode == 1) pm = OUTPUT; else if (mode == 2) pm = INPUT_PULLUP;
    #if defined(INPUT_PULLDOWN)
    else if (mode == 4) pm = INPUT_PULLDOWN;
    #endif
  } else if (arg != nil) pm = OUTPUT;
  pinMode(pin, pm);
  return nil;
}

object *fn_digitalread (object *args, object *env) {
  (void) env;
  int pin = checkinteger(DIGITALREAD, first(args));
  if (digitalRead(pin) != 0) return tee; else return nil;
}

object *fn_digitalwrite (object *args, object *env) {
  (void) env;
  int pin;
  object *arg = first(args);
  if (keywordp(arg)) pin = checkkeyword(NIL, arg);
  else pin = checkinteger(DIGITALWRITE, arg);
  arg = second(args);
  int mode;
  if (keywordp(arg)) mode = checkkeyword(DIGITALWRITE, arg);
  else if (integerp(arg)) mode = arg->integer ? HIGH : LOW;
  else mode = (arg != nil) ? HIGH : LOW;
  digitalWrite(pin, mode);
  return arg;
}

object *fn_analogread (object *args, object *env) {
  (void) env;
  int pin;
  object *arg = first(args);
  if (keywordp(arg)) pin = checkkeyword(ANALOGREAD, arg);
  else {
    pin = checkinteger(ANALOGREAD, arg);
    checkanalogread(pin);
  }
  return number(analogRead(pin));
}

object *fn_analogreference (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  analogReference(checkkeyword(ANALOGREFERENCE, arg));
  return arg;
}

object *fn_analogreadresolution (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  #if defined(CPU_AVR128DX48)
  uint8_t res = checkinteger(ANALOGREADRESOLUTION, arg);
  if (res == 10) analogReadResolution(10);
  else if (res == 12) analogReadResolution(12);
  else error(ANALOGREADRESOLUTION, PSTR("invalid resolution"), arg);
  #else
  error2(ANALOGREADRESOLUTION, PSTR("not supported"));
  #endif
  return arg;
}

object *fn_analogwrite (object *args, object *env) {
  (void) env;
  int pin;
  object *arg = first(args);
  if (keywordp(arg)) pin = checkkeyword(NIL, arg);
  else pin = checkinteger(ANALOGWRITE, arg);
  checkanalogwrite(pin);
  object *value = second(args);
  analogWrite(pin, checkinteger(ANALOGWRITE, value));
  return value;
}

object *fn_dacreference (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  #if defined(CPU_AVR128DX48)
  int ref = checkinteger(DACREFERENCE, arg);
  DACReference(ref);
  #endif
  return arg;
}

object *fn_delay (object *args, object *env) {
  (void) env;
  object *arg1 = first(args);
  delay(checkinteger(DELAY, arg1));
  return arg1;
}

object *fn_millis (object *args, object *env) {
  (void) args, (void) env;
  return number(millis());
}

object *fn_sleep (object *args, object *env) {
  (void) env;
  object *arg1 = first(args);
  sleep(checkinteger(SLEEP, arg1));
  return arg1;
}

object *fn_note (object *args, object *env) {
  (void) env;
  static int pin = 255;
  if (args != NULL) {
    pin = checkinteger(NOTE, first(args));
    int note = 0;
    if (cddr(args) != NULL) note = checkinteger(NOTE, second(args));
    int octave = 0;
    if (cddr(args) != NULL) octave = checkinteger(NOTE, third(args));
    playnote(pin, note, octave);
  } else nonote(pin);
  return nil;
}

object *fn_register (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  int addr;
  if (keywordp(arg)) addr = checkkeyword(REGISTER, arg);
  else addr = checkinteger(REGISTER, first(args));
  if (cdr(args) == NULL) return number(*(volatile uint8_t *)addr);
  (*(volatile uint8_t *)addr) = checkinteger(REGISTER, second(args));
  return second(args);
}

// Tree Editor

object *fn_edit (object *args, object *env) {
  object *fun = first(args);
  object *pair = findvalue(EDIT, fun, env);
  clrflag(EXITEDITOR);
  object *arg = edit(eval(fun, env));
  cdr(pair) = arg;
  return arg;
}

// Pretty printer

object *fn_pprint (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  pln(pfun);
  superprint(obj, 0, pfun);
  return bsymbol(NOTHING);
}

object *fn_pprintall (object *args, object *env) {
  (void) env;
  pfun_t pfun = pstreamfun(args);
  object *globals = GlobalEnv;
  while (globals != NULL) {
    object *pair = first(globals);
    object *var = car(pair);
    object *val = cdr(pair);
    pln(pfun);
    if (consp(val) && symbolp(car(val)) && builtin(car(val)->name) == LAMBDA) {
      superprint(cons(bsymbol(DEFUN), cons(var, cdr(val))), 0, pfun);
    #if defined(CODESIZE)
    } else if (consp(val) && car(val)->type == CODE) {
      superprint(cons(bsymbol(DEFCODE), cons(var, cdr(val))), 0, pfun);
    #endif
    } else {
      superprint(cons(bsymbol(DEFVAR), cons(var, cons(quote(val), NULL))), 0, pfun);
    }
    pln(pfun);
    testescape();
    globals = cdr(globals);
  }
  return bsymbol(NOTHING);
}

// Format

object *fn_format (object *args, object *env) {
  (void) env;
  pfun_t pfun = pserial;
  object *output = first(args);
  object *obj;
  if (output == nil) { obj = startstring(FORMAT); pfun = pstr; }
  else if (output != tee) pfun = pstreamfun(args);
  object *formatstr = checkstring(FORMAT, second(args));
  object *save = NULL;
  args = cddr(args);
  int len = stringlength(formatstr);
  uint8_t n = 0, width = 0, w, bra = 0;
  char pad = ' ';
  bool tilde = false, mute = false, comma = false, quote = false;
  while (n < len) {
    char ch = nthchar(formatstr, n);
    char ch2 = ch & ~0x20; // force to upper case
    if (tilde) {
     if (ch == '}') {
        if (save == NULL) formaterr(formatstr, PSTR("no matching ~{"), n);
        if (args == NULL) { args = cdr(save); save = NULL; } else n = bra;
        mute = false; tilde = false;
      }
      else if (!mute) {
        if (comma && quote) { pad = ch; comma = false, quote = false; }
        else if (ch == '\'') {
          if (comma) quote = true;
          else formaterr(formatstr, PSTR("quote not valid"), n);
        }
        else if (ch == '~') { pfun('~'); tilde = false; }
        else if (ch >= '0' && ch <= '9') width = width*10 + ch - '0';
        else if (ch == ',') comma = true;
        else if (ch == '%') { pln(pfun); tilde = false; }
        else if (ch == '&') { pfl(pfun); tilde = false; }
        else if (ch == '^') {
          if (save != NULL && args == NULL) mute = true;
          tilde = false;
        }
        else if (ch == '{') {
          if (save != NULL) formaterr(formatstr, PSTR("can't nest ~{"), n);
          if (args == NULL) formaterr(formatstr, noargument, n);
          if (!listp(first(args))) formaterr(formatstr, notalist, n);
          save = args; args = first(args); bra = n; tilde = false;
          if (args == NULL) mute = true;
        }
        else if (ch2 == 'A' || ch2 == 'S' || ch2 == 'D' || ch2 == 'G' || ch2 == 'X' || ch2 == 'B') {
          if (args == NULL) formaterr(formatstr, noargument, n);
          object *arg = first(args); args = cdr(args);
          uint8_t aw = atomwidth(arg);
          if (width < aw) w = 0; else w = width-aw;
          tilde = false;
          if (ch2 == 'A') { prin1object(arg, pfun); indent(w, pad, pfun); }
          else if (ch2 == 'S') { printobject(arg, pfun); indent(w, pad, pfun); }
          else if (ch2 == 'D' || ch2 == 'G') { indent(w, pad, pfun); prin1object(arg, pfun); }
          else if (ch2 == 'X' || ch2 == 'B') {
            if (integerp(arg)) {
              uint8_t base = (ch2 == 'B') ? 2 : 16;
              uint8_t hw = basewidth(arg, base); if (width < hw) w = 0; else w = width-hw;
              indent(w, pad, pfun); pintbase(arg->integer, base, pfun);
            } else {
              indent(w, pad, pfun); prin1object(arg, pfun);
            }
          }
          tilde = false;
        } else formaterr(formatstr, PSTR("invalid directive"), n);
      }
    } else {
      if (ch == '~') { tilde = true; pad = ' '; width = 0; comma = false; quote = false; }
      else if (!mute) pfun(ch);
    }
    n++;
  }
  if (output == nil) return obj;
  else return nil;
}

// LispLibrary

object *fn_require (object *args, object *env) {
  object *arg = first(args);
  object *globals = GlobalEnv;
  if (!symbolp(arg)) error(REQUIRE, notasymbol, arg);
  while (globals != NULL) {
    object *pair = first(globals);
    object *var = car(pair);
    if (symbolp(var) && var == arg) return nil;
    globals = cdr(globals);
  }
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    // Is this the definition we want
    symbol_t fname = first(line)->name;
    if ((fname == sym(DEFUN) || fname == sym(DEFVAR)) && symbolp(second(line)) && second(line)->name == arg->name) {
      eval(line, env);
      return tee;
    }
    line = read(glibrary);
  }
  return nil;
}

object *fn_listlibrary (object *args, object *env) {
  (void) args, (void) env;
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    builtin_t bname = builtin(first(line)->name);
    if (bname == DEFUN || bname == DEFVAR) {
      printsymbol(second(line), pserial); pserial(' ');
    }
    line = read(glibrary);
  }
  return bsymbol(NOTHING);
}

// Documentation

object *fn_documentation (object *args, object *env) {
  return documentation(DOCUMENTATION, first(args), env);
}

// Lisp Badge plotting

void plotsub (uint8_t x, uint8_t y, uint8_t n, int ys[5]) {
  if (y<64) {
    uint8_t grey = 0x0F-n*3;
    uint8_t blob = grey;
    if ((x&1) == 0) { blob = grey<<4; ys[n] = y; }
    else {
      for (int i=0; i<5; i++) {
        if (y == ys[i]) blob = (0x0F-i*3)<<4 | grey;
      }
    }
    PlotByte(x>>1, y, blob);
  }
}

object *fn_plot (object *args, object *env) {
  int ys[5] = {-1, -1, -1, -1, -1};
  int xaxis = -1, yaxis = -1;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(PLOT, first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(PLOT, first(args)); args = cdr(args); }
  int nargs = min(listlength(PLOT, args),4);
  for (int x=0; x<256; x++) {
    object *rest = args;
    for (int n=0; n<nargs; n++) {
      object *function = first(rest);
      int y = checkinteger(PLOT, apply(PLOT, function, cons(number(x), NULL), env));
      plotsub(x, y, n+1, ys);
      rest = cdr(rest);
    }
    plotsub(x, yaxis, 0, ys);
    if (x == xaxis) for (int y=0; y<64; y++) plotsub(x, y, 0, ys);
    if ((x&1) != 0) for (int i=0; i<5; i++) ys[i] = -1;
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

object *fn_plot3d (object *args, object *env) {
  int xaxis = -1, yaxis = -1;
  uint8_t blob;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(PLOT3D, first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(PLOT3D, first(args)); args = cdr(args); }
  if (args != NULL) {
    object *function = first(args);
    for (int y=0; y<64; y++) {
      for (int x=0; x<256; x++) {
        int z = checkinteger(PLOT3D, apply(PLOT3D, function, cons(number(x), cons(number(y), NULL)), env));
        if (x == xaxis || y == yaxis) z = 0xF;
        if ((x&1) == 0) blob = z<<4; else blob = blob | (z&0xF);
        PlotByte(x>>1, y, blob);
      }
    }
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

extern const uint8_t CharMap[96][6] PROGMEM;

object *fn_glyphpixel (object *args, object *env) {
  (void) env;
  uint8_t c = 0, x = 6, y = 8;
  c = checkchar(GLYPHPIXEL, first(args));
  x = checkinteger(GLYPHPIXEL, second(args));
  y = checkinteger(GLYPHPIXEL, third(args));
  if (x > 5 || y > 7) return number(0);
  return pgm_read_byte(&CharMap[(c & 0x7f) - 32][x]) & 1 << (7 - y) ? number(15) : number(0);
}

object *fn_plotpixel (object *args, object *env) {
  (void) env;
  int x = checkinteger(PLOTPIXEL, first(args));
  int y = checkinteger(PLOTPIXEL, second(args));
  args = cddr(args);
  uint8_t grey = 0xff;
  if (args != NULL) grey = checkinteger(PLOTPIXEL, first(args));
  PlotByte(x, y, grey);
  return nil;
}

object *fn_fillscreen (object *args, object *env) {
  (void) env;
  uint8_t grey = 0;
  if (args != NULL) grey = checkinteger(FILLSCREEN, first(args));
  ClearDisplay(grey);
  return nil;
}

// Insert your own function definitions here

// Built-in symbol names
const char string0[] PROGMEM = "nil";
const char string1[] PROGMEM = "t";
const char string2[] PROGMEM = "nothing";
const char string3[] PROGMEM = "&optional";
const char string4[] PROGMEM = ":initial-element";
const char string5[] PROGMEM = ":element-type";
const char string6[] PROGMEM = "bit";
const char string7[] PROGMEM = "&rest";
const char string8[] PROGMEM = "lambda";
const char string9[] PROGMEM = "let";
const char string10[] PROGMEM = "let*";
const char string11[] PROGMEM = "closure";
const char string12[] PROGMEM = "*p*";
const char string13[] PROGMEM = "";
const char string14[] PROGMEM = "quote";
const char string15[] PROGMEM = "or";
const char string16[] PROGMEM = "defun";
const char string17[] PROGMEM = "defvar";
const char string18[] PROGMEM = "setq";
const char string19[] PROGMEM = "loop";
const char string20[] PROGMEM = "return";
const char string21[] PROGMEM = "push";
const char string22[] PROGMEM = "pop";
const char string23[] PROGMEM = "incf";
const char string24[] PROGMEM = "decf";
const char string25[] PROGMEM = "setf";
const char string26[] PROGMEM = "dolist";
const char string27[] PROGMEM = "dotimes";
const char string28[] PROGMEM = "trace";
const char string29[] PROGMEM = "untrace";
const char string30[] PROGMEM = "for-millis";
const char string31[] PROGMEM = "time";
const char string32[] PROGMEM = "with-serial";
const char string33[] PROGMEM = "with-i2c";
const char string34[] PROGMEM = "with-spi";
const char string35[] PROGMEM = "with-sd-card";
const char string36[] PROGMEM = "defcode";
const char string37[] PROGMEM = "";
const char string38[] PROGMEM = "progn";
const char string39[] PROGMEM = "if";
const char string40[] PROGMEM = "cond";
const char string41[] PROGMEM = "when";
const char string42[] PROGMEM = "unless";
const char string43[] PROGMEM = "case";
const char string44[] PROGMEM = "and";
const char string45[] PROGMEM = "?";
const char string46[] PROGMEM = "";
const char string47[] PROGMEM = "not";
const char string48[] PROGMEM = "null";
const char string49[] PROGMEM = "cons";
const char string50[] PROGMEM = "atom";
const char string51[] PROGMEM = "listp";
const char string52[] PROGMEM = "consp";
const char string53[] PROGMEM = "symbolp";
const char string54[] PROGMEM = "boundp";
const char string55[] PROGMEM = "set";
const char string56[] PROGMEM = "streamp";
const char string57[] PROGMEM = "eq";
const char string58[] PROGMEM = "car";
const char string59[] PROGMEM = "first";
const char string60[] PROGMEM = "cdr";
const char string61[] PROGMEM = "rest";
const char string62[] PROGMEM = "caar";
const char string63[] PROGMEM = "cadr";
const char string64[] PROGMEM = "second";
const char string65[] PROGMEM = "cdar";
const char string66[] PROGMEM = "cddr";
const char string67[] PROGMEM = "caaar";
const char string68[] PROGMEM = "caadr";
const char string69[] PROGMEM = "cadar";
const char string70[] PROGMEM = "caddr";
const char string71[] PROGMEM = "third";
const char string72[] PROGMEM = "cdaar";
const char string73[] PROGMEM = "cdadr";
const char string74[] PROGMEM = "cddar";
const char string75[] PROGMEM = "cdddr";
const char string76[] PROGMEM = "length";
const char string77[] PROGMEM = "array-dimensions";
const char string78[] PROGMEM = "list";
const char string79[] PROGMEM = "make-array";
const char string80[] PROGMEM = "reverse";
const char string81[] PROGMEM = "nth";
const char string82[] PROGMEM = "aref";
const char string83[] PROGMEM = "assoc";
const char string84[] PROGMEM = "member";
const char string85[] PROGMEM = "apply";
const char string86[] PROGMEM = "funcall";
const char string87[] PROGMEM = "append";
const char string88[] PROGMEM = "mapc";
const char string89[] PROGMEM = "mapcar";
const char string90[] PROGMEM = "mapcan";
const char string91[] PROGMEM = "+";
const char string92[] PROGMEM = "-";
const char string93[] PROGMEM = "*";
const char string94[] PROGMEM = "/";
const char string95[] PROGMEM = "truncate";
const char string96[] PROGMEM = "mod";
const char string97[] PROGMEM = "1+";
const char string98[] PROGMEM = "1-";
const char string99[] PROGMEM = "abs";
const char string100[] PROGMEM = "random";
const char string101[] PROGMEM = "max";
const char string102[] PROGMEM = "min";
const char string103[] PROGMEM = "/=";
const char string104[] PROGMEM = "=";
const char string105[] PROGMEM = "<";
const char string106[] PROGMEM = "<=";
const char string107[] PROGMEM = ">";
const char string108[] PROGMEM = ">=";
const char string109[] PROGMEM = "plusp";
const char string110[] PROGMEM = "minusp";
const char string111[] PROGMEM = "zerop";
const char string112[] PROGMEM = "oddp";
const char string113[] PROGMEM = "evenp";
const char string114[] PROGMEM = "integerp";
const char string115[] PROGMEM = "numberp";
const char string116[] PROGMEM = "char";
const char string117[] PROGMEM = "char-code";
const char string118[] PROGMEM = "code-char";
const char string119[] PROGMEM = "characterp";
const char string120[] PROGMEM = "stringp";
const char string121[] PROGMEM = "string=";
const char string122[] PROGMEM = "string<";
const char string123[] PROGMEM = "string>";
const char string124[] PROGMEM = "sort";
const char string125[] PROGMEM = "string";
const char string126[] PROGMEM = "concatenate";
const char string127[] PROGMEM = "subseq";
const char string128[] PROGMEM = "read-from-string";
const char string129[] PROGMEM = "princ-to-string";
const char string130[] PROGMEM = "prin1-to-string";
const char string131[] PROGMEM = "logand";
const char string132[] PROGMEM = "logior";
const char string133[] PROGMEM = "logxor";
const char string134[] PROGMEM = "lognot";
const char string135[] PROGMEM = "ash";
const char string136[] PROGMEM = "logbitp";
const char string137[] PROGMEM = "eval";
const char string138[] PROGMEM = "globals";
const char string139[] PROGMEM = "locals";
const char string140[] PROGMEM = "makunbound";
const char string141[] PROGMEM = "break";
const char string142[] PROGMEM = "read";
const char string143[] PROGMEM = "prin1";
const char string144[] PROGMEM = "print";
const char string145[] PROGMEM = "princ";
const char string146[] PROGMEM = "terpri";
const char string147[] PROGMEM = "read-byte";
const char string148[] PROGMEM = "read-line";
const char string149[] PROGMEM = "write-byte";
const char string150[] PROGMEM = "write-string";
const char string151[] PROGMEM = "write-line";
const char string152[] PROGMEM = "restart-i2c";
const char string153[] PROGMEM = "gc";
const char string154[] PROGMEM = "room";
const char string155[] PROGMEM = "save-image";
const char string156[] PROGMEM = "load-image";
const char string157[] PROGMEM = "cls";
const char string158[] PROGMEM = "pinmode";
const char string159[] PROGMEM = "digitalread";
const char string160[] PROGMEM = "digitalwrite";
const char string161[] PROGMEM = "analogread";
const char string162[] PROGMEM = "analogreference";
const char string163[] PROGMEM = "analogreadresolution";
const char string164[] PROGMEM = "analogwrite";
const char string165[] PROGMEM = "dacreference";
const char string166[] PROGMEM = "delay";
const char string167[] PROGMEM = "millis";
const char string168[] PROGMEM = "sleep";
const char string169[] PROGMEM = "note";
const char string170[] PROGMEM = "register";
const char string171[] PROGMEM = "edit";
const char string172[] PROGMEM = "pprint";
const char string173[] PROGMEM = "pprintall";
const char string174[] PROGMEM = "format";
const char string175[] PROGMEM = "require";
const char string176[] PROGMEM = "list-library";
const char string177[] PROGMEM = "documentation";
const char string178[] PROGMEM = "plot";
const char string179[] PROGMEM = "plot3d";
const char string180[] PROGMEM = "glyph-pixel";
const char string181[] PROGMEM = "plot-pixel";
const char string182[] PROGMEM = "fill-screen";
const char string183[] PROGMEM = "";
#if defined(CPU_ATmega1284P)
const char string184[] PROGMEM = ":high";
const char string185[] PROGMEM = ":low";
const char string186[] PROGMEM = ":input";
const char string187[] PROGMEM = ":input-pullup";
const char string188[] PROGMEM = ":output";
const char string189[] PROGMEM = ":default";
const char string190[] PROGMEM = ":internal1v1";
const char string191[] PROGMEM = ":internal2v56";
const char string192[] PROGMEM = ":external";
const char string193[] PROGMEM = "";
#endif

// Insert your own function names here

// Documentation strings
const char doc0[] PROGMEM = "nil\n"
"A symbol equivalent to the empty list (). Also represents false.";
const char doc1[] PROGMEM = "t\n"
"A symbol representing true.";
const char doc2[] PROGMEM = "nothing\n"
"A symbol with no value.\n"
"It is useful if you want to suppress printing the result of evaluating a function.";
const char doc3[] PROGMEM = "&optional\n"
"Can be followed by one or more optional parameters in a lambda or defun parameter list.";
const char doc7[] PROGMEM = "&rest\n"
"Can be followed by a parameter in a lambda or defun parameter list,\n"
"and is assigned a list of the corresponding arguments.";
const char doc8[] PROGMEM = "(lambda (parameter*) form*)\n"
"Creates an unnamed function with parameters. The body is evaluated with the parameters as local variables\n"
"whose initial values are defined by the values of the forms after the lambda form.";
const char doc9[] PROGMEM = "(let ((var value) ... ) forms*)\n"
"Declares local variables with values, and evaluates the forms with those local variables.";
const char doc10[] PROGMEM = "(let* ((var value) ... ) forms*)\n"
"Declares local variables with values, and evaluates the forms with those local variables.\n"
"Each declaration can refer to local variables that have been defined earlier in the let*.";
const char doc15[] PROGMEM = "(or item*)\n"
"Evaluates its arguments until one returns non-nil, and returns its value.";
const char doc16[] PROGMEM = "(defun name (parameters) form*)\n"
"Defines a function.";
const char doc17[] PROGMEM = "(defvar variable form)\n"
"Defines a global variable.";
const char doc18[] PROGMEM = "(setq symbol value [symbol value]*)\n"
"For each pair of arguments assigns the value of the second argument\n"
"to the variable specified in the first argument.";
const char doc19[] PROGMEM = "(loop forms*)\n"
"Executes its arguments repeatedly until one of the arguments calls (return),\n"
"which then causes an exit from the loop.";
const char doc20[] PROGMEM = "(return [value])\n"
"Exits from a (dotimes ...), (dolist ...), or (loop ...) loop construct and returns value.";
const char doc21[] PROGMEM = "(push item place)\n"
"Modifies the value of place, which should be a list, to add item onto the front of the list,\n"
"and returns the new list.";
const char doc22[] PROGMEM = "(pop place)\n"
"Modifies the value of place, which should be a list, to remove its first item, and returns that item.";
const char doc23[] PROGMEM = "(incf place [number])\n"
"Increments a place, which should have an numeric value, and returns the result.\n"
"The third argument is an optional increment which defaults to 1.";
const char doc24[] PROGMEM = "(decf place [number])\n"
"Decrements a place, which should have an numeric value, and returns the result.\n"
"The third argument is an optional decrement which defaults to 1.";
const char doc25[] PROGMEM = "(setf place value [place value]*)\n"
"For each pair of arguments modifies a place to the result of evaluating value.";
const char doc26[] PROGMEM = "(dolist (var list [result]) form*)\n"
"Sets the local variable var to each element of list in turn, and executes the forms.\n"
"It then returns result, or nil if result is omitted.";
const char doc27[] PROGMEM = "(dotimes (var number [result]) form*)\n"
"Executes the forms number times, with the local variable var set to each integer from 0 to number-1 in turn.\n"
"It then returns result, or nil if result is omitted.";
const char doc28[] PROGMEM = "(trace [function]*)\n"
"Turns on tracing of up to TRACEMAX user-defined functions,\n"
"and returns a list of the functions currently being traced.";
const char doc29[] PROGMEM = "(untrace [function]*)\n"
"Turns off tracing of up to TRACEMAX user-defined functions, and returns a list of the functions untraced.\n"
"If no functions are specified it untraces all functions.";
const char doc30[] PROGMEM = "(for-millis ([number]) form*)\n"
"Executes the forms and then waits until a total of number milliseconds have elapsed.\n"
"Returns the total number of milliseconds taken.";
const char doc31[] PROGMEM = "(time form)\n"
"Prints the value returned by the form, and the time taken to evaluate the form\n"
"in milliseconds or seconds.";
const char doc32[] PROGMEM = "(with-serial (str port [baud]) form*)\n"
"Evaluates the forms with str bound to a serial-stream using port.\n"
"The optional baud gives the baud rate divided by 100, default 96.";
const char doc33[] PROGMEM = "(with-i2c (str [port] address [read-p]) form*)\n"
"Evaluates the forms with str bound to an i2c-stream defined by address.\n"
"If read-p is nil or omitted the stream is written to, otherwise it specifies the number of bytes\n"
"to be read from the stream. The port if specified is ignored.";
const char doc34[] PROGMEM = "(with-spi (str pin [clock] [bitorder] [mode]) form*)\n"
"Evaluates the forms with str bound to an spi-stream.\n"
"The parameters specify the enable pin, clock in kHz (default 4000),\n"
"bitorder 0 for LSBFIRST and 1 for MSBFIRST (default 1), and SPI mode (default 0).";
const char doc35[] PROGMEM = "(with-sd-card (str filename [mode]) form*)\n"
"Evaluates the forms with str bound to an sd-stream reading from or writing to the file filename.\n"
"If mode is omitted the file is read, otherwise 0 means read, 1 write-append, or 2 write-overwrite.";
const char doc36[] PROGMEM = "(defcode name (parameters) form*)\n"
"Creates a machine-code function called name from a series of 16-bit integers given in the body of the form.\n"
"These are written into RAM, and can be executed by calling the function in the same way as a normal Lisp function.";
const char doc38[] PROGMEM = "(progn form*)\n"
"Evaluates several forms grouped together into a block, and returns the result of evaluating the last form.";
const char doc39[] PROGMEM = "(if test then [else])\n"
"Evaluates test. If it's non-nil the form then is evaluated and returned;\n"
"otherwise the form else is evaluated and returned.";
const char doc40[] PROGMEM = "(cond ((test form*) (test form*) ... ))\n"
"Each argument is a list consisting of a test optionally followed by one or more forms.\n"
"If the test evaluates to non-nil the forms are evaluated, and the last value is returned as the result of the cond.\n"
"If the test evaluates to nil, none of the forms are evaluated, and the next argument is processed in the same way.";
const char doc41[] PROGMEM = "(when test form*)\n"
"Evaluates the test. If it's non-nil the forms are evaluated and the last value is returned.";
const char doc42[] PROGMEM = "(unless test form*)\n"
"Evaluates the test. If it's nil the forms are evaluated and the last value is returned.";
const char doc43[] PROGMEM = "(case keyform ((key form*) (key form*) ... ))\n"
"Evaluates a keyform to produce a test key, and then tests this against a series of arguments,\n"
"each of which is a list containing a key optionally followed by one or more forms.";
const char doc44[] PROGMEM = "(and item*)\n"
"Evaluates its arguments until one returns nil, and returns the last value.";
const char doc45[] PROGMEM = "(? item)\n"
"Prints the documentation string of a built-in or user-defined function.";
const char doc47[] PROGMEM = "(not item)\n"
"Returns t if its argument is nil, or nil otherwise. Equivalent to null.";
const char doc49[] PROGMEM = "(cons item item)\n"
"If the second argument is a list, cons returns a new list with item added to the front of the list.\n"
"If the second argument isn't a list cons returns a dotted pair.";
const char doc50[] PROGMEM = "(atom item)\n"
"Returns t if its argument is a single number, symbol, or nil.";
const char doc51[] PROGMEM = "(listp item)\n"
"Returns t if its argument is a list.";
const char doc52[] PROGMEM = "(consp item)\n"
"Returns t if its argument is a non-null list.";
const char doc53[] PROGMEM = "(symbolp item)\n"
"Returns t if its argument is a symbol.";
const char doc54[] PROGMEM = "(boundp item)\n"
"Returns t if its argument is a symbol with a value.";
const char doc55[] PROGMEM = "(set symbol value [symbol value]*)\n"
"For each pair of arguments, assigns the value of the second argument to the value of the first argument.";
const char doc56[] PROGMEM = "(streamp item)\n"
"Returns t if its argument is a stream.";
const char doc57[] PROGMEM = "(eq item item)\n"
"Tests whether the two arguments are the same symbol, same character, equal numbers,\n"
"or point to the same cons, and returns t or nil as appropriate.";
const char doc58[] PROGMEM = "(car list)\n"
"Returns the first item in a list.";
const char doc60[] PROGMEM = "(cdr list)\n"
"Returns a list with the first item removed.";
const char doc62[] PROGMEM = "(caar list)";
const char doc63[] PROGMEM = "(cadr list)";
const char doc65[] PROGMEM = "(cdar list)\n"
"Equivalent to (cdr (car list)).";
const char doc66[] PROGMEM = "(cddr list)\n"
"Equivalent to (cdr (cdr list)).";
const char doc67[] PROGMEM = "(caaar list)\n"
"Equivalent to (car (car (car list))).";
const char doc68[] PROGMEM = "(caadr list)\n"
"Equivalent to (car (car (cdar list))).";
const char doc69[] PROGMEM = "(cadar list)\n"
"Equivalent to (car (cdr (car list))).";
const char doc70[] PROGMEM = "(caddr list)\n"
"Equivalent to (car (cdr (cdr list))).";
const char doc72[] PROGMEM = "(cdaar list)\n"
"Equivalent to (cdar (car (car list))).";
const char doc73[] PROGMEM = "(cdadr list)\n"
"Equivalent to (cdr (car (cdr list))).";
const char doc74[] PROGMEM = "(cddar list)\n"
"Equivalent to (cdr (cdr (car list))).";
const char doc75[] PROGMEM = "(cdddr list)\n"
"Equivalent to (cdr (cdr (cdr list))).";
const char doc76[] PROGMEM = "(length item)\n"
"Returns the number of items in a list, the length of a string, or the length of a one-dimensional array.";
const char doc77[] PROGMEM = "(array-dimensions item)\n"
"Returns a list of the dimensions of an array.";
const char doc78[] PROGMEM = "(list item*)\n"
"Returns a list of the values of its arguments.";
const char doc79[] PROGMEM = "(make-array size [:initial-element element] [:element-type 'bit])\n"
"If size is an integer it creates a one-dimensional array with elements from 0 to size-1.\n"
"If size is a list of n integers it creates an n-dimensional array with those dimensions.\n"
"If :element-type 'bit is specified the array is a bit array.";
const char doc80[] PROGMEM = "(reverse list)\n"
"Returns a list with the elements of list in reverse order.";
const char doc81[] PROGMEM = "(nth number list)\n"
"Returns the nth item in list, counting from zero.";
const char doc82[] PROGMEM = "(aref array index [index*])\n"
"Returns an element from the specified array.";
const char doc83[] PROGMEM = "(assoc key list)\n"
"Looks up a key in an association list of (key . value) pairs,\n"
"and returns the matching pair, or nil if no pair is found.";
const char doc84[] PROGMEM = "(member item list)\n"
"Searches for an item in a list, using eq, and returns the list starting from the first occurrence of the item,\n"
"or nil if it is not found.";
const char doc85[] PROGMEM = "(apply function list)\n"
"Returns the result of evaluating function, with the list of arguments specified by the second parameter.";
const char doc86[] PROGMEM = "(funcall function argument*)\n"
"Evaluates function with the specified arguments.";
const char doc87[] PROGMEM = "(append list*)\n"
"Joins its arguments, which should be lists, into a single list.";
const char doc88[] PROGMEM = "(mapc function list1 [list]*)\n"
"Applies the function to each element in one or more lists, ignoring the results.\n"
"It returns the first list argument.";
const char doc89[] PROGMEM = "(mapcar function list1 [list]*)\n"
"Applies the function to each element in one or more lists, and returns the resulting list.";
const char doc90[] PROGMEM = "(mapcan function list1 [list]*)\n"
"Applies the function to each element in one or more lists. The results should be lists,\n"
"and these are appended together to give the value returned.";
const char doc91[] PROGMEM = "(+ number*)\n"
"Adds its arguments together.";
const char doc92[] PROGMEM = "(- number*)\n"
"If there is one argument, negates the argument.\n"
"If there are two or more arguments, subtracts the second and subsequent arguments from the first argument.";
const char doc93[] PROGMEM = "(* number*)\n"
"Multiplies its arguments together.";
const char doc94[] PROGMEM = "(/ number*)\n"
"Divides the first argument by the second and subsequent arguments.";
const char doc96[] PROGMEM = "(mod number number)\n"
"Returns its first argument modulo the second argument.";
const char doc97[] PROGMEM = "(1+ number)\n"
"Adds one to its argument and returns it.";
const char doc98[] PROGMEM = "(1- number)\n"
"Subtracts one from its argument and returns it.";
const char doc99[] PROGMEM = "(abs number)\n"
"Returns the absolute, positive value of its argument.";
const char doc100[] PROGMEM = "(random number)\n"
"Returns a random number between 0 and one less than its argument.";
const char doc101[] PROGMEM = "(max number*)\n"
"Returns the maximum of one or more arguments.";
const char doc102[] PROGMEM = "(min number*)\n"
"Returns the minimum of one or more arguments.";
const char doc103[] PROGMEM = "(/= number*)\n"
"Returns t if none of the arguments are equal, or nil if two or more arguments are equal.";
const char doc104[] PROGMEM = "(= number*)\n"
"Returns t if all the arguments, which must be numbers, are numerically equal, and nil otherwise.";
const char doc105[] PROGMEM = "(< number*)\n"
"Returns t if each argument is less than the next argument, and nil otherwise.";
const char doc106[] PROGMEM = "(<= number*)\n"
"Returns t if each argument is less than or equal to the next argument, and nil otherwise.";
const char doc107[] PROGMEM = "(> number*)\n"
"Returns t if each argument is greater than the next argument, and nil otherwise.";
const char doc108[] PROGMEM = "(>= number*)\n"
"Returns t if each argument is greater than or equal to the next argument, and nil otherwise.";
const char doc109[] PROGMEM = "(plusp number)\n"
"Returns t if the argument is greater than zero, or nil otherwise.";
const char doc111[] PROGMEM = "(zerop number)\n"
"Returns t if the argument is zero.";
const char doc112[] PROGMEM = "(oddp number)\n"
"Returns t if the integer argument is odd.";
const char doc113[] PROGMEM = "(evenp number)\n"
"Returns t if the integer argument is even.";
const char doc114[] PROGMEM = "(integerp number)\n"
"Returns t if the argument is an integer.";
const char doc116[] PROGMEM = "(char string n)\n"
"Returns the nth character in a string, counting from zero.";
const char doc117[] PROGMEM = "(char-code character)\n"
"Returns the ASCII code for a character, as an integer.";
const char doc118[] PROGMEM = "(code-char integer)\n"
"Returns the character for the specified ASCII code.";
const char doc119[] PROGMEM = "(characterp item)\n"
"Returns t if the argument is a character and nil otherwise.";
const char doc120[] PROGMEM = "(stringp item)\n"
"Returns t if the argument is a string and nil otherwise.";
const char doc121[] PROGMEM = "(string= string string)\n"
"Tests whether two strings are the same.";
const char doc122[] PROGMEM = "(string< string string)\n"
"Returns t if the first string is alphabetically less than the second string, and nil otherwise. For example:";
const char doc123[] PROGMEM = "(string> string string)\n"
"Returns t if the first string is alphabetically greater than the second string, and nil otherwise.";
const char doc124[] PROGMEM = "(sort list test)\n"
"Destructively sorts list according to the test function, using an insertion sort, and returns the sorted list.";
const char doc125[] PROGMEM = "(string item)\n"
"Converts its argument to a string.";
const char doc126[] PROGMEM = "(concatenate 'string string*)\n"
"Joins together the strings given in the second and subsequent arguments, and returns a single string.";
const char doc127[] PROGMEM = "(subseq string start [end])\n"
"Returns a substring from a string, from character start to character end-1:";
const char doc128[] PROGMEM = "(read-from-string string)\n"
"Reads an atom or list from the specified string and returns it.";
const char doc129[] PROGMEM = "(princ-to-string item)\n"
"Prints its argument to a string, and returns the string.\n"
"Characters and strings are printed without quotation marks or escape characters.";
const char doc130[] PROGMEM = "(prin1-to-string item [stream])\n"
"Prints its argument to a string, and returns the string.\n"
"Characters and strings are printed with quotation marks and escape characters,\n"
"in a format that will be suitable for read-from-string.";
const char doc131[] PROGMEM = "(logand [value*])\n"
"Returns the bitwise & of the values.";
const char doc132[] PROGMEM = "(logior [value*])\n"
"Returns the bitwise | of the values.";
const char doc133[] PROGMEM = "(logxor [value*])\n"
"Returns the bitwise ^ of the values.";
const char doc134[] PROGMEM = "(prin1-to-string item [stream])\n"
"Prints its argument to a string, and returns the string.\n"
"Characters and strings are printed with quotation marks and escape characters,\n"
"in a format that will be suitable for read-from-string.";
const char doc135[] PROGMEM = "(ash value shift)\n"
"Returns the result of bitwise shifting value by shift bits. If shift is positive, value is shifted to the left.";
const char doc136[] PROGMEM = "(logbitp bit value)\n"
"Returns t if bit number bit in value is a '1', and nil if it is a '0'.";
const char doc137[] PROGMEM = "(eval form*)\n"
"Evaluates its argument an extra time.";
const char doc138[] PROGMEM = "(globals)\n"
"Returns an association list of global variables and their values.";
const char doc139[] PROGMEM = "(locals)\n"
"Returns an association list of local variables and their values.";
const char doc140[] PROGMEM = "(makunbound symbol)\n"
"Removes the value of the symbol from GlobalEnv and returns the symbol.";
const char doc141[] PROGMEM = "(break)\n"
"Inserts a breakpoint in the program. When evaluated prints Break! and reenters the REPL.";
const char doc142[] PROGMEM = "(read [stream])\n"
"Reads an atom or list from the serial input and returns it.\n"
"If stream is specified the item is read from the specified stream.";
const char doc143[] PROGMEM = "(prin1 item [stream])\n"
"Prints its argument, and returns its value.\n"
"Strings are printed with quotation marks and escape characters.";
const char doc144[] PROGMEM = "(print item [stream])\n"
"Prints its argument with quotation marks and escape characters, on a new line, and followed by a space.\n"
"If stream is specified the argument is printed to the specified stream.";
const char doc145[] PROGMEM = "(princ item [stream])\n"
"Prints its argument, and returns its value.\n"
"Characters and strings are printed without quotation marks or escape characters.";
const char doc146[] PROGMEM = "(terpri [stream])\n"
"Prints a new line, and returns nil.\n"
"If stream is specified the new line is written to the specified stream.";
const char doc147[] PROGMEM = "(read-byte stream)\n"
"Reads a byte from a stream and returns it.";
const char doc148[] PROGMEM = "(read-line [stream])\n"
"Reads characters from the serial input up to a newline character, and returns them as a string, excluding the newline.\n"
"If stream is specified the line is read from the specified stream.";
const char doc149[] PROGMEM = "(write-byte number [stream])\n"
"Writes a byte to a stream.";
const char doc150[] PROGMEM = "(write-string string [stream])\n"
"Writes a string. If stream is specified the string is written to the stream.";
const char doc151[] PROGMEM = "(write-line string [stream])\n"
"Writes a string terminated by a newline character. If stream is specified the string is written to the stream.";
const char doc152[] PROGMEM = "(restart-i2c stream [read-p])\n"
"Restarts an i2c-stream.\n"
"If read-p is nil or omitted the stream is written to.\n"
"If read-p is an integer it specifies the number of bytes to be read from the stream.";
const char doc153[] PROGMEM = "(gc)\n"
"Forces a garbage collection and prints the number of objects collected, and the time taken.";
const char doc154[] PROGMEM = "(room)\n"
"Returns the number of free Lisp cells remaining.";
const char doc155[] PROGMEM = "(save-image [symbol])\n"
"Saves the current uLisp image to non-volatile memory or SD card so it can be loaded using load-image.";
const char doc156[] PROGMEM = "(load-image [filename])\n"
"Loads a saved uLisp image from non-volatile memory or SD card.";
const char doc157[] PROGMEM = "(cls)\n"
"Prints a clear-screen character.";
const char doc158[] PROGMEM = "(pinmode pin mode)\n"
"Sets the input/output mode of an Arduino pin number, and returns nil.\n"
"The mode parameter can be an integer, a keyword, or t or nil.";
const char doc159[] PROGMEM = "(digitalread pin)\n"
"Reads the state of the specified Arduino pin number and returns t (high) or nil (low).";
const char doc160[] PROGMEM = "(digitalwrite pin state)\n"
"Sets the state of the specified Arduino pin number.";
const char doc161[] PROGMEM = "(analogread pin)\n"
"Reads the specified Arduino analogue pin number and returns the value.";
const char doc162[] PROGMEM = "(analogreference keyword)\n"
"Specifies a keyword to set the analogue reference voltage used for analogue input.";
const char doc163[] PROGMEM = "(analogreadresolution bits)\n"
"Specifies the resolution for the analogue inputs on platforms that support it.\n"
"The default resolution on all platforms is 10 bits.";
const char doc164[] PROGMEM = "(analogwrite pin value)\n"
"Writes the value to the specified Arduino pin number.";
const char doc165[] PROGMEM = "(dacreference value)\n"
"Sets the DAC voltage reference. AVR128DX48 only.";
const char doc166[] PROGMEM = "(delay number)\n"
"Delays for a specified number of milliseconds.";
const char doc167[] PROGMEM = "(millis)\n"
"Returns the time in milliseconds that uLisp has been running.";
const char doc168[] PROGMEM = "(sleep secs)\n"
"Puts the processor into a low-power sleep mode for secs.\n"
"Only supported on some platforms. On other platforms it does delay(1000*secs).";
const char doc169[] PROGMEM = "(note [pin] [note] [octave])\n"
"Generates a square wave on pin.\n"
"The argument note represents the note in the well-tempered scale, from 0 to 11,\n"
"where 0 represents C, 1 represents C#, and so on.\n"
"The argument octave can be from 3 to 6. If omitted it defaults to 0.";
const char doc170[] PROGMEM = "(register address [value])\n"
"Reads or writes the value of a peripheral register.\n"
"If value is not specified the function returns the value of the register at address.\n"
"If value is specified the value is written to the register at address and the function returns value.";
const char doc171[] PROGMEM = "(edit 'function)\n"
"Calls the Lisp tree editor to allow you to edit a function definition.";
const char doc172[] PROGMEM = "(pprint item [str])\n"
"Prints its argument, using the pretty printer, to display it formatted in a structured way.\n"
"If str is specified it prints to the specified stream. It returns no value.";
const char doc173[] PROGMEM = "(pprintall [str])\n"
"Pretty-prints the definition of every function and variable defined in the uLisp workspace.\n"
"If str is specified it prints to the specified stream. It returns no value.";
const char doc174[] PROGMEM = "(format output controlstring arguments*)\n"
"Outputs its arguments formatted according to the format directives in controlstring.";
const char doc175[] PROGMEM = "(require 'symbol)\n"
"Loads the definition of a function defined with defun, or a variable defined with defvar, from the Lisp Library.\n"
"It returns t if it was loaded, or nil if the symbol is already defined or isn't defined in the Lisp Library.";
const char doc176[] PROGMEM = "(list-library)\n"
"Prints a list of the functions defined in the List Library.";
const char doc177[] PROGMEM = "(documentation 'symbol [type])\n"
"Returns the documentation string of a built-in or user-defined function. The type argument is ignored.";

// Insert your own function documentation here

// Built-in symbol lookup table
const tbl_entry_t lookup_table[] PROGMEM = {
  { string0, NULL, 0x00, doc0 },
  { string1, NULL, 0x00, doc1 },
  { string2, NULL, 0x00, doc2 },
  { string3, NULL, 0x00, doc3 },
  { string4, NULL, 0x00, NULL },
  { string5, NULL, 0x00, NULL },
  { string6, NULL, 0x00, NULL },
  { string7, NULL, 0x00, doc7 },
  { string8, NULL, 0x0F, doc8 },
  { string9, NULL, 0x0F, doc9 },
  { string10, NULL, 0x0F, doc10 },
  { string11, NULL, 0x0F, NULL },
  { string12, NULL, 0x0F, NULL },
  { string13, NULL, 0x00, NULL },
  { string14, sp_quote, 0x11, NULL },
  { string15, sp_or, 0x0F, doc15 },
  { string16, sp_defun, 0x2F, doc16 },
  { string17, sp_defvar, 0x13, doc17 },
  { string18, sp_setq, 0x2F, doc18 },
  { string19, sp_loop, 0x0F, doc19 },
  { string20, sp_return, 0x0F, doc20 },
  { string21, sp_push, 0x22, doc21 },
  { string22, sp_pop, 0x11, doc22 },
  { string23, sp_incf, 0x12, doc23 },
  { string24, sp_decf, 0x12, doc24 },
  { string25, sp_setf, 0x2F, doc25 },
  { string26, sp_dolist, 0x1F, doc26 },
  { string27, sp_dotimes, 0x1F, doc27 },
  { string28, sp_trace, 0x01, doc28 },
  { string29, sp_untrace, 0x01, doc29 },
  { string30, sp_formillis, 0x1F, doc30 },
  { string31, sp_time, 0x11, doc31 },
  { string32, sp_withserial, 0x1F, doc32 },
  { string33, sp_withi2c, 0x1F, doc33 },
  { string34, sp_withspi, 0x1F, doc34 },
  { string35, sp_withsdcard, 0x2F, doc35 },
  { string36, sp_defcode, 0x0F, doc36 },
  { string37, NULL, 0x00, NULL },
  { string38, tf_progn, 0x0F, doc38 },
  { string39, tf_if, 0x23, doc39 },
  { string40, tf_cond, 0x0F, doc40 },
  { string41, tf_when, 0x1F, doc41 },
  { string42, tf_unless, 0x1F, doc42 },
  { string43, tf_case, 0x1F, doc43 },
  { string44, tf_and, 0x0F, doc44 },
  { string45, tf_help, 0x11, doc45 },
  { string46, NULL, 0x00, NULL },
  { string47, fn_not, 0x11, doc47 },
  { string48, fn_not, 0x11, NULL },
  { string49, fn_cons, 0x22, doc49 },
  { string50, fn_atom, 0x11, doc50 },
  { string51, fn_listp, 0x11, doc51 },
  { string52, fn_consp, 0x11, doc52 },
  { string53, fn_symbolp, 0x11, doc53 },
  { string54, fn_boundp, 0x11, doc54 },
  { string55, fn_setfn, 0x2F, doc55 },
  { string56, fn_streamp, 0x11, doc56 },
  { string57, fn_eq, 0x22, doc57 },
  { string58, fn_car, 0x11, doc58 },
  { string59, fn_car, 0x11, NULL },
  { string60, fn_cdr, 0x11, doc60 },
  { string61, fn_cdr, 0x11, NULL },
  { string62, fn_caar, 0x11, doc62 },
  { string63, fn_cadr, 0x11, doc63 },
  { string64, fn_cadr, 0x11, NULL },
  { string65, fn_cdar, 0x11, doc65 },
  { string66, fn_cddr, 0x11, doc66 },
  { string67, fn_caaar, 0x11, doc67 },
  { string68, fn_caadr, 0x11, doc68 },
  { string69, fn_cadar, 0x11, doc69 },
  { string70, fn_caddr, 0x11, doc70 },
  { string71, fn_caddr, 0x11, NULL },
  { string72, fn_cdaar, 0x11, doc72 },
  { string73, fn_cdadr, 0x11, doc73 },
  { string74, fn_cddar, 0x11, doc74 },
  { string75, fn_cdddr, 0x11, doc75 },
  { string76, fn_length, 0x11, doc76 },
  { string77, fn_arraydimensions, 0x11, doc77 },
  { string78, fn_list, 0x0F, doc78 },
  { string79, fn_makearray, 0x15, doc79 },
  { string80, fn_reverse, 0x11, doc80 },
  { string81, fn_nth, 0x22, doc81 },
  { string82, fn_aref, 0x2F, doc82 },
  { string83, fn_assoc, 0x22, doc83 },
  { string84, fn_member, 0x22, doc84 },
  { string85, fn_apply, 0x2F, doc85 },
  { string86, fn_funcall, 0x1F, doc86 },
  { string87, fn_append, 0x0F, doc87 },
  { string88, fn_mapc, 0x2F, doc88 },
  { string89, fn_mapcar, 0x2F, doc89 },
  { string90, fn_mapcan, 0x2F, doc90 },
  { string91, fn_add, 0x0F, doc91 },
  { string92, fn_subtract, 0x1F, doc92 },
  { string93, fn_multiply, 0x0F, doc93 },
  { string94, fn_divide, 0x2F, doc94 },
  { string95, fn_divide, 0x12, NULL },
  { string96, fn_mod, 0x22, doc96 },
  { string97, fn_oneplus, 0x11, doc97 },
  { string98, fn_oneminus, 0x11, doc98 },
  { string99, fn_abs, 0x11, doc99 },
  { string100, fn_random, 0x11, doc100 },
  { string101, fn_maxfn, 0x1F, doc101 },
  { string102, fn_minfn, 0x1F, doc102 },
  { string103, fn_noteq, 0x1F, doc103 },
  { string104, fn_numeq, 0x1F, doc104 },
  { string105, fn_less, 0x1F, doc105 },
  { string106, fn_lesseq, 0x1F, doc106 },
  { string107, fn_greater, 0x1F, doc107 },
  { string108, fn_greatereq, 0x1F, doc108 },
  { string109, fn_plusp, 0x11, doc109 },
  { string110, fn_minusp, 0x11, NULL },
  { string111, fn_zerop, 0x11, doc111 },
  { string112, fn_oddp, 0x11, doc112 },
  { string113, fn_evenp, 0x11, doc113 },
  { string114, fn_integerp, 0x11, doc114 },
  { string115, fn_integerp, 0x11, NULL },
  { string116, fn_char, 0x22, doc116 },
  { string117, fn_charcode, 0x11, doc117 },
  { string118, fn_codechar, 0x11, doc118 },
  { string119, fn_characterp, 0x11, doc119 },
  { string120, fn_stringp, 0x11, doc120 },
  { string121, fn_stringeq, 0x22, doc121 },
  { string122, fn_stringless, 0x22, doc122 },
  { string123, fn_stringgreater, 0x22, doc123 },
  { string124, fn_sort, 0x22, doc124 },
  { string125, fn_stringfn, 0x11, doc125 },
  { string126, fn_concatenate, 0x1F, doc126 },
  { string127, fn_subseq, 0x23, doc127 },
  { string128, fn_readfromstring, 0x11, doc128 },
  { string129, fn_princtostring, 0x11, doc129 },
  { string130, fn_prin1tostring, 0x11, doc130 },
  { string131, fn_logand, 0x0F, doc131 },
  { string132, fn_logior, 0x0F, doc132 },
  { string133, fn_logxor, 0x0F, doc133 },
  { string134, fn_lognot, 0x11, doc134 },
  { string135, fn_ash, 0x22, doc135 },
  { string136, fn_logbitp, 0x22, doc136 },
  { string137, fn_eval, 0x11, doc137 },
  { string138, fn_globals, 0x00, doc138 },
  { string139, fn_locals, 0x00, doc139 },
  { string140, fn_makunbound, 0x11, doc140 },
  { string141, fn_break, 0x00, doc141 },
  { string142, fn_read, 0x01, doc142 },
  { string143, fn_prin1, 0x12, doc143 },
  { string144, fn_print, 0x12, doc144 },
  { string145, fn_princ, 0x12, doc145 },
  { string146, fn_terpri, 0x01, doc146 },
  { string147, fn_readbyte, 0x02, doc147 },
  { string148, fn_readline, 0x01, doc148 },
  { string149, fn_writebyte, 0x12, doc149 },
  { string150, fn_writestring, 0x12, doc150 },
  { string151, fn_writeline, 0x12, doc151 },
  { string152, fn_restarti2c, 0x12, doc152 },
  { string153, fn_gc, 0x00, doc153 },
  { string154, fn_room, 0x00, doc154 },
  { string155, fn_saveimage, 0x01, doc155 },
  { string156, fn_loadimage, 0x01, doc156 },
  { string157, fn_cls, 0x00, doc157 },
  { string158, fn_pinmode, 0x22, doc158 },
  { string159, fn_digitalread, 0x11, doc159 },
  { string160, fn_digitalwrite, 0x22, doc160 },
  { string161, fn_analogread, 0x11, doc161 },
  { string162, fn_analogreference, 0x11, doc162 },
  { string163, fn_analogreadresolution, 0x11, doc163 },
  { string164, fn_analogwrite, 0x22, doc164 },
  { string165, fn_dacreference, 0x11, doc165 },
  { string166, fn_delay, 0x11, doc166 },
  { string167, fn_millis, 0x00, doc167 },
  { string168, fn_sleep, 0x11, doc168 },
  { string169, fn_note, 0x03, doc169 },
  { string170, fn_register, 0x12, doc170 },
  { string171, fn_edit, 0x11, doc171 },
  { string172, fn_pprint, 0x12, doc172 },
  { string173, fn_pprintall, 0x01, doc173 },
  { string174, fn_format, 0x2F, doc174 },
  { string175, fn_require, 0x11, doc175 },
  { string176, fn_listlibrary, 0x00, doc176 },
  { string177, fn_documentation, 0x12, doc177 },
  { string178, fn_plot, 0x06, NULL },
  { string179, fn_plot3d, 0x03, NULL },
  { string180, fn_glyphpixel, 0x33, NULL },
  { string181, fn_plotpixel, 0x23, NULL },
  { string182, fn_fillscreen, 0x01, NULL },
  { string183, NULL, 0x00, NULL },
#if defined(CPU_ATmega1284P)
  { string184, (fn_ptr_type)HIGH, DIGITALWRITE, NULL },
  { string185, (fn_ptr_type)LOW, DIGITALWRITE, NULL },
  { string186, (fn_ptr_type)INPUT, PINMODE, NULL },
  { string187, (fn_ptr_type)INPUT_PULLUP, PINMODE, NULL },
  { string188, (fn_ptr_type)OUTPUT, PINMODE, NULL },
  { string189, (fn_ptr_type)DEFAULT, ANALOGREFERENCE, NULL },
  { string190, (fn_ptr_type)INTERNAL1V1, ANALOGREFERENCE, NULL },
  { string191, (fn_ptr_type)INTERNAL2V56, ANALOGREFERENCE, NULL },
  { string192, (fn_ptr_type)EXTERNAL, ANALOGREFERENCE, NULL },
  { string193, NULL, 0x00, NULL },
#endif

// Insert your own table entries here

};

// Table lookup functions

builtin_t lookupbuiltin (char* n) {
  int entry = 0;
  while (entry < ENDFUNCTIONS) {
    #if defined(CPU_ATmega4809)
    if (strcasecmp(n, (char*)lookup_table[entry].string) == 0)
    #else
    if (strcasecmp_P(n, (char*)pgm_read_word(&lookup_table[entry].string)) == 0)
    #endif
      return (builtin_t)entry;
    entry++;
  }
  return ENDFUNCTIONS;
}

intptr_t lookupfn (builtin_t name) {
  #if defined(CPU_ATmega4809)
  return (intptr_t)lookup_table[name].fptr;
  #else
  return pgm_read_word(&lookup_table[name].fptr);
  #endif
}

uint8_t getminmax (builtin_t name) {
  #if defined(CPU_ATmega4809)
  uint8_t minmax = lookup_table[name].minmax;
  #else
  uint8_t minmax = pgm_read_byte(&lookup_table[name].minmax);
  #endif
  return minmax;
}

void checkminmax (builtin_t name, int nargs) {
  uint8_t minmax = getminmax(name);
  if (nargs<(minmax >> 4)) error2(name, toofewargs);
  if ((minmax & 0x0f) != 0x0f && nargs>(minmax & 0x0f)) error2(name, toomanyargs);
}

char *lookupdoc (builtin_t name) {
  #if defined(CPU_ATmega4809)
  return (char*)lookup_table[name].doc;
  #else
  return (char*)pgm_read_ptr(&lookup_table[name].doc);
  #endif
}

void testescape () {
#if defined serialmonitor
  if (Serial.read() == '~') error2(NIL, PSTR("escape!"));
#endif
}

// Main evaluator

extern char __bss_end[];

object *eval (object *form, object *env) {
  uint8_t sp[0];
  int TC=0;
  EVAL:
  // Enough space?
  //Serial.println((uint16_t)sp - (uint16_t)__bss_end); // Find best STACKDIFF value
  if ((uint16_t)sp - (uint16_t)__bss_end < STACKDIFF) error2(NIL, PSTR("stack overflow"));
  if (Freespace <= WORKSPACESIZE>>4) gc(form, env);      // GC when 1/16 of workspace left
  // Escape
  if (tstflag(ESCAPE)) { clrflag(ESCAPE); error2(NIL, PSTR("escape!"));}
  if (!tstflag(NOESC)) testescape();

  if (form == NULL) return nil;

  if (form->type >= NUMBER && form->type <= STRING) return form;

  if (symbolp(form)) {
    symbol_t name = form->name;
    object *pair = value(name, env);
    if (pair != NULL) return cdr(pair);
    pair = value(name, GlobalEnv);
    if (pair != NULL) return cdr(pair);
    else if (builtinp(name)) return form;
    error(NIL, PSTR("undefined"), form);
  }

  #if defined(CODESIZE)
  if (form->type == CODE) error2(NIL, PSTR("can't evaluate CODE header"));
  #endif

  // It's a list
  object *function = car(form);
  object *args = cdr(form);

  if (function == NULL) error(NIL, PSTR("illegal function"), nil);
  if (!listp(args)) error(NIL, PSTR("can't evaluate a dotted pair"), args);

  // List starts with a symbol?
  if (symbolp(function)) {
    builtin_t name = builtin(function->name);

    if ((name == LET) || (name == LETSTAR)) {
      int TCstart = TC;
      if (args == NULL) error2(name, noargument);
      object *assigns = first(args);
      if (!listp(assigns)) error(name, notalist, assigns);
      object *forms = cdr(args);
      object *newenv = env;
      push(newenv, GCStack);
      while (assigns != NULL) {
        object *assign = car(assigns);
        if (!consp(assign)) push(cons(assign,nil), newenv);
        else if (cdr(assign) == NULL) push(cons(first(assign),nil), newenv);
        else push(cons(first(assign),eval(second(assign),env)), newenv);
        car(GCStack) = newenv;
        if (name == LETSTAR) env = newenv;
        assigns = cdr(assigns);
      }
      env = newenv;
      pop(GCStack);
      form = tf_progn(forms,env);
      TC = TCstart;
      goto EVAL;
    }

    if (name == LAMBDA) {
      if (env == NULL) return form;
      object *envcopy = NULL;
      while (env != NULL) {
        object *pair = first(env);
        if (pair != NULL) push(pair, envcopy);
        env = cdr(env);
      }
      return cons(bsymbol(CLOSURE), cons(envcopy,args));
    }

    if ((name > SPECIAL_FORMS) && (name < TAIL_FORMS)) {
      return ((fn_ptr_type)lookupfn(name))(args, env);
    }

    if ((name > TAIL_FORMS) && (name < FUNCTIONS)) {
      form = ((fn_ptr_type)lookupfn(name))(args, env);
      TC = 1;
      goto EVAL;
    }
    if (((name > 0) && (name < SPECIAL_FORMS)) || ((name > KEYWORDS) && (name < USERFUNCTIONS))) error2(name, PSTR("can't be used as a function"));
  }

  // Evaluate the parameters - result in head
  object *fname = car(form);
  int TCstart = TC;
  object *head = cons(eval(fname, env), NULL);
  push(head, GCStack); // Don't GC the result list
  object *tail = head;
  form = cdr(form);
  int nargs = 0;

  while (form != NULL){
    object *obj = cons(eval(car(form),env),NULL);
    cdr(tail) = obj;
    tail = obj;
    form = cdr(form);
    nargs++;
  }

  function = car(head);
  args = cdr(head);

  if (symbolp(function)) {
    builtin_t bname = builtin(function->name);
    if (!builtinp(function->name)) error(NIL, PSTR("not valid here"), fname);
    checkminmax(bname, nargs);
    object *result = ((fn_ptr_type)lookupfn(bname))(args, env);
    pop(GCStack);
    return result;
  }

  if (consp(function)) {
    symbol_t name = sym(NIL);
    if (!listp(fname)) name = fname->name;

    if (isbuiltin(car(function), LAMBDA)) {
      form = closure(TCstart, name, function, args, &env);
      pop(GCStack);
      int trace = tracing(fname->name);
      if (trace) {
        object *result = eval(form, env);
        indent((--(TraceDepth[trace-1]))<<1, ' ', pserial);
        pint(TraceDepth[trace-1], pserial);
        pserial(':'); pserial(' ');
        printobject(fname, pserial); pfstring(PSTR(" returned "), pserial);
        printobject(result, pserial); pln(pserial);
        return result;
      } else {
        TC = 1;
        goto EVAL;
      }
    }

    if (isbuiltin(car(function), CLOSURE)) {
      function = cdr(function);
      form = closure(TCstart, name, function, args, &env);
      pop(GCStack);
      TC = 1;
      goto EVAL;
    }

    #if defined(CODESIZE)
    if (car(function)->type == CODE) {
      int n = listlength(DEFCODE, second(function));
      if (nargs<n) errorsym2(fname->name, toofewargs);
      if (nargs>n) errorsym2(fname->name, toomanyargs);
      uint32_t entry = startblock(car(function));
      pop(GCStack);
      return call(entry, n, args, env);
    }
    #endif

  }
  error(NIL, PSTR("illegal function"), fname); return nil;
}

// Print functions

void pserial (char c) {
  LastPrint = c;
  Display(c);
  #if defined (serialmonitor)
  if (c == '\n') Serial.write('\r');
  Serial.write(c);
  #endif
}

const char ControlCodes[] PROGMEM = "Null\0SOH\0STX\0ETX\0EOT\0ENQ\0ACK\0Bell\0Backspace\0Tab\0Newline\0VT\0"
"Page\0Return\0SO\0SI\0DLE\0DC1\0DC2\0DC3\0DC4\0NAK\0SYN\0ETB\0CAN\0EM\0SUB\0Escape\0FS\0GS\0RS\0US\0Space\0";

void pcharacter (uint8_t c, pfun_t pfun) {
  if (!tstflag(PRINTREADABLY)) pfun(c);
  else {
    pfun('#'); pfun('\\');
    if (c <= 32) {
      PGM_P p = ControlCodes;
      #if defined(CPU_ATmega4809)
      while (c > 0) {p = p + strlen(p) + 1; c--; }
      #else
      while (c > 0) {p = p + strlen_P(p) + 1; c--; }
      #endif
      pfstring(p, pfun);
    } else if (c < 127) pfun(c);
    else pint(c, pfun);
  }
}

void pstring (char *s, pfun_t pfun) {
  while (*s) pfun(*s++);
}

void plispstring (object *form, pfun_t pfun) {
  plispstr(form->name, pfun);
}

void plispstr (symbol_t name, pfun_t pfun) {
  object *form = (object *)name;
  while (form != NULL) {
    int chars = form->chars;
    for (int i=(sizeof(int)-1)*8; i>=0; i=i-8) {
      char ch = chars>>i & 0xFF;
      if (tstflag(PRINTREADABLY) && (ch == '"' || ch == '\\')) pfun('\\');
      if (ch) pfun(ch);
    }
    form = car(form);
  }
}

void printstring (object *form, pfun_t pfun) {
  if (tstflag(PRINTREADABLY)) pfun('"');
  plispstr(form->name, pfun);
  if (tstflag(PRINTREADABLY)) pfun('"');
}

void pbuiltin (builtin_t name, pfun_t pfun) {
  int p = 0;
  #if defined(CPU_ATmega4809)
  PGM_P s = lookup_table[name].string;
  #else
  PGM_P s = (char*)pgm_read_word(&lookup_table[name].string);
  #endif
  while (1) {
    #if defined(CPU_ATmega4809)
    char c = s[p++];
    #else
    char c = pgm_read_byte(&s[p++]);
    #endif
    if (c == 0) return;
    pfun(c);
  }
}

void pradix40 (symbol_t name, pfun_t pfun) {
  uint16_t x = untwist(name);
  for (int d=1600; d>0; d = d/40) {
    uint16_t j = x/d;
    char c = fromradix40(j);
    if (c == 0) return;
    pfun(c); x = x - j*d;
  }
}

void printsymbol (object *form, pfun_t pfun) {
  psymbol(form->name, pfun);
}

void psymbol (symbol_t name, pfun_t pfun) {
  if ((name & 0x03) == 0) plispstr(name, pfun);
  else {
    uint16_t value = untwist(name);
    if (value < PACKEDS) error2(NIL, PSTR("invalid symbol"));
    else if (value >= BUILTINS) pbuiltin((builtin_t)(value-BUILTINS), pfun);
    else pradix40(name, pfun);
  }
}

void pfstring (PGM_P s, pfun_t pfun) {
  int p = 0;
  while (1) {
    #if defined(CPU_ATmega4809)
    char c = s[p++];
    #else
    char c = pgm_read_byte(&s[p++]);
    #endif
    if (c == 0) return;
    pfun(c);
  }
}

void pint (int i, pfun_t pfun) {
  uint16_t j = i;
  if (i<0) { pfun('-'); j=-i; }
  pintbase(j, 10, pfun);
}

void pintbase (uint16_t i, uint8_t base, pfun_t pfun) {
  int lead = 0; uint16_t p = 10000;
  if (base == 2) p = 0x8000; else if (base == 16) p = 0x1000;
  for (uint16_t d=p; d>0; d=d/base) {
    uint16_t j = i/d;
    if (j!=0 || lead || d==1) { pfun((j<10) ? j+'0' : j+'W'); lead=1;}
    i = i - j*d;
  }
}

void printhex2 (int i, pfun_t pfun) {
  for (unsigned int d=0x10; d>0; d=d>>4) {
    unsigned int j = i/d;
    pfun((j<10) ? j+'0' : j+'W'); 
    i = i - j*d;
  }
}

inline void pln (pfun_t pfun) {
  pfun('\n');
}

void pfl (pfun_t pfun) {
  if (LastPrint != '\n') pfun('\n');
}

void plist (object *form, pfun_t pfun) {
  pfun('(');
  printobject(car(form), pfun);
  form = cdr(form);
  while (form != NULL && listp(form)) {
    pfun(' ');
    printobject(car(form), pfun);
    form = cdr(form);
  }
  if (form != NULL) {
    pfstring(PSTR(" . "), pfun);
    printobject(form, pfun);
  }
  pfun(')');
}

void pstream (object *form, pfun_t pfun) {
  pfun('<');
  pfstring(streamname[(form->integer)>>8], pfun);
  pfstring(PSTR("-stream "), pfun);
  pint(form->integer & 0xFF, pfun);
  pfun('>');
}

void printobject (object *form, pfun_t pfun) {
  if (form == NULL) pfstring(PSTR("nil"), pfun);
  else if (listp(form) && isbuiltin(car(form), CLOSURE)) pfstring(PSTR("<closure>"), pfun);
  else if (listp(form)) plist(form, pfun);
  else if (integerp(form)) pint(form->integer, pfun);
  else if (symbolp(form)) { if (form->name != sym(NOTHING)) printsymbol(form, pfun); }
  else if (characterp(form)) pcharacter(form->chars, pfun);
  else if (stringp(form)) printstring(form, pfun);
  else if (arrayp(form)) printarray(form, pfun);
  #if defined(CODESIZE)
  else if (form->type == CODE) pfstring(PSTR("code"), pfun);
  #endif
  else if (streamp(form)) pstream(form, pfun);
  else error2(NIL, PSTR("error in print"));
}

void prin1object (object *form, pfun_t pfun) {
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printobject(form, pfun);
  Flags = temp;
}

// For Lisp Badge
volatile int WritePtr = 0, ReadPtr = 0;
const int KybdBufSize = 333; // 42*8 - 3
char KybdBuf[KybdBufSize];
volatile uint8_t KybdAvailable = 0;

// Read functions

int glibrary () {
  if (LastChar) { 
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  char c = pgm_read_byte(&LispLibrary[GlobalStringIndex++]);
  return (c != 0) ? c : -1; // -1?
}

void loadfromlibrary (object *env) {
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    push(line, GCStack);
    eval(line, env);
    pop(GCStack);
    line = read(glibrary);
  }
}

int gserial () {
  if (LastChar) {
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  #if defined (serialmonitor)
  unsigned long start = millis();
  while (!Serial.available() && !KybdAvailable) if (millis() - start > 1000) clrflag(NOECHO);
  if (Serial.available()) {
    char temp = Serial.read();
    if (temp != '\n' && !tstflag(NOECHO)) Serial.print(temp); // Don't print on Lisp Badge
    return temp;
  } else {
    if (ReadPtr != WritePtr) {
      char temp = KybdBuf[ReadPtr++];
      Serial.write(temp);
      return temp;
    }
    KybdAvailable = 0;
    WritePtr = 0;
    return '\n';
  }
  #else
  while (!KybdAvailable);
  if (ReadPtr != WritePtr) return KybdBuf[ReadPtr++];
  KybdAvailable = 0;
  WritePtr = 0;
  return '\n';
  #endif
}

object *nextitem (gfun_t gfun) {
  int ch = gfun();
  while(issp(ch)) ch = gfun();

  #if defined(CPU_ATmega328P)
  if (ch == ';') {
    while(ch != '(') ch = gfun();
  }
  #else
  if (ch == ';') {
    do { ch = gfun(); if (ch == ';' || ch == '(') setflag(NOECHO); }
    while(ch != '(');
  }
  #endif
  if (ch == '\n') ch = gfun();
  if (ch == -1) return nil;
  if (ch == ')') return (object *)KET;
  if (ch == '(') return (object *)BRA;
  if (ch == '\'') return (object *)QUO;
  if (ch == '.') return (object *)DOT;

  // Parse string
  if (ch == '"') return readstring('"', gfun);

  // Parse symbol, character, or number
  int index = 0, base = 10, sign = 1;
  char buffer[BUFFERSIZE];
  int bufmax = BUFFERSIZE-1; // Max index
  unsigned int result = 0;
  if (ch == '+' || ch == '-') {
    buffer[index++] = ch;
    if (ch == '-') sign = -1;
    ch = gfun();
  }

  // Parse reader macros
  else if (ch == '#') {
    ch = gfun();
    char ch2 = ch & ~0x20; // force to upper case
    if (ch == '\\') { // Character
      base = 0; ch = gfun();
      if (issp(ch) || ch == ')' || ch == '(') return character(ch);
      else LastChar = ch;
    } else if (ch == '|') {
      do { while (gfun() != '|'); }
      while (gfun() != '#');
      return nextitem(gfun);
    } else if (ch2 == 'B') base = 2;
    else if (ch2 == 'O') base = 8;
    else if (ch2 == 'X') base = 16;
    else if (ch == '\'') return nextitem(gfun);
    else if (ch == '.') {
      setflag(NOESC);
      object *result = eval(read(gfun), NULL);
      clrflag(NOESC);
      return result;
    }
    else if (ch == '(') { LastChar = ch; return readarray(1, read(gfun)); }
    else if (ch == '*') return readbitarray(gfun);
    else if (ch >= '1' && ch <= '9' && (gfun() & ~0x20) == 'A') return readarray(ch - '0', read(gfun));
    else error2(NIL, PSTR("illegal character after #"));
    ch = gfun();
  }

  int isnumber = (digitvalue(ch)<base);
  buffer[2] = '\0'; // In case symbol is one letter

  while(!issp(ch) && ch != ')' && ch != '(' && index < bufmax) {
    buffer[index++] = ch;
    int temp = digitvalue(ch);
    result = result * base + temp;
    isnumber = isnumber && (digitvalue(ch)<base);
    ch = gfun();
  }

  buffer[index] = '\0';
  if (ch == ')' || ch == '(') LastChar = ch;

  if (isnumber) {
    if (base == 10 && result > ((unsigned int)INT_MAX+(1-sign)/2)) 
      error2(NIL, PSTR("Number out of range"));
    return number(result*sign);
  } else if (base == 0) {
    if (index == 1) return character(buffer[0]);
    PGM_P p = ControlCodes; char c = 0;
    while (c < 33) {
      #if defined(CPU_ATmega4809)
      if (strcasecmp(buffer, p) == 0) return character(c);
      p = p + strlen(p) + 1; c++;
      #else
      if (strcasecmp_P(buffer, p) == 0) return character(c);
      p = p + strlen_P(p) + 1; c++;
      #endif
    }
    if (index == 3) return character((buffer[0]*10+buffer[1])*10+buffer[2]-5328);
    error2(NIL, PSTR("unknown character"));
  }
  
  builtin_t x = lookupbuiltin(buffer);
  if (x == NIL) return nil;
  if (x != ENDFUNCTIONS) return bsymbol(x);
  else if ((index <= 3) && valid40(buffer)) return intern(twist(pack40(buffer)));
  buffer[index+1] = '\0'; // For internlong
  return internlong(buffer);
}

object *readrest (gfun_t gfun) {
  object *item = nextitem(gfun);
  object *head = NULL;
  object *tail = NULL;

  while (item != (object *)KET) {
    if (item == (object *)BRA) {
      item = readrest(gfun);
    } else if (item == (object *)QUO) {
      item = cons(bsymbol(QUOTE), cons(read(gfun), NULL));
    } else if (item == (object *)DOT) {
      tail->cdr = read(gfun);
      if (readrest(gfun) != NULL) error2(NIL, PSTR("malformed list"));
      return head;
    } else {
      object *cell = cons(item, NULL);
      if (head == NULL) head = cell;
      else tail->cdr = cell;
      tail = cell;
      item = nextitem(gfun);
    }
  }
  return head;
}

object *read (gfun_t gfun) {
  object *item = nextitem(gfun);
  if (item == (object *)KET) error2(NIL, PSTR("incomplete list"));
  if (item == (object *)BRA) return readrest(gfun);
  if (item == (object *)DOT) return read(gfun);
  if (item == (object *)QUO) return cons(bsymbol(QUOTE), cons(read(gfun), NULL));
  return item;
}

// Lisp Badge terminal and keyboard support

// These are the bit positions in PORTA
int const clk = 7;   // PA7
int const data = 6;  // PA6
int const dc = 5;    // PA5
int const cs = 4;    // PA4

// Terminal **********************************************************************************

// Character set - stored in program memory
const uint8_t CharMap[96][6] PROGMEM = {
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
{ 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 },
{ 0x00, 0x07, 0x00, 0x07, 0x00, 0x00 },
{ 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00 },
{ 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00 },
{ 0x23, 0x13, 0x08, 0x64, 0x62, 0x00 },
{ 0x36, 0x49, 0x56, 0x20, 0x50, 0x00 },
{ 0x00, 0x08, 0x07, 0x03, 0x00, 0x00 },
{ 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00 },
{ 0x00, 0x41, 0x22, 0x1C, 0x00, 0x00 },
{ 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00 },
{ 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },
{ 0x00, 0x80, 0x70, 0x30, 0x00, 0x00 },
{ 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },
{ 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },
{ 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 },
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 },
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 },
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 },
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 },
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 },
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 },
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 },
{ 0x00, 0x36, 0x36, 0x00, 0x00, 0x00 },
{ 0x00, 0x56, 0x36, 0x00, 0x00, 0x00 },
{ 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },
{ 0x14, 0x14, 0x14, 0x14, 0x14, 0x00 },
{ 0x00, 0x41, 0x22, 0x14, 0x08, 0x00 },
{ 0x02, 0x01, 0x59, 0x09, 0x06, 0x00 },
{ 0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00 },
{ 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00 },
{ 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00 },
{ 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 },
{ 0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00 },
{ 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 },
{ 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00 },
{ 0x3E, 0x41, 0x41, 0x51, 0x73, 0x00 },
{ 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 },
{ 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00 },
{ 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00 },
{ 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00 },
{ 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00 },
{ 0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00 },
{ 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00 },
{ 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00 },
{ 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 },
{ 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00 },
{ 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00 },
{ 0x26, 0x49, 0x49, 0x49, 0x32, 0x00 },
{ 0x03, 0x01, 0x7F, 0x01, 0x03, 0x00 },
{ 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00 },
{ 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00 },
{ 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00 },
{ 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 },
{ 0x03, 0x04, 0x78, 0x04, 0x03, 0x00 },
{ 0x61, 0x59, 0x49, 0x4D, 0x43, 0x00 },
{ 0x00, 0x7F, 0x41, 0x41, 0x41, 0x00 },
{ 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
{ 0x00, 0x41, 0x41, 0x41, 0x7F, 0x00 },
{ 0x04, 0x02, 0x01, 0x02, 0x04, 0x00 },
{ 0x40, 0x40, 0x40, 0x40, 0x40, 0x00 },
{ 0x00, 0x03, 0x07, 0x08, 0x00, 0x00 },
{ 0x20, 0x54, 0x54, 0x78, 0x40, 0x00 },
{ 0x7F, 0x28, 0x44, 0x44, 0x38, 0x00 },
{ 0x38, 0x44, 0x44, 0x44, 0x28, 0x00 },
{ 0x38, 0x44, 0x44, 0x28, 0x7F, 0x00 },
{ 0x38, 0x54, 0x54, 0x54, 0x18, 0x00 },
{ 0x00, 0x08, 0x7E, 0x09, 0x02, 0x00 },
{ 0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00 },
{ 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00 },
{ 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00 },
{ 0x20, 0x40, 0x40, 0x3D, 0x00, 0x00 },
{ 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00 },
{ 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00 },
{ 0x7C, 0x04, 0x78, 0x04, 0x78, 0x00 },
{ 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00 },
{ 0x38, 0x44, 0x44, 0x44, 0x38, 0x00 },
{ 0xFC, 0x18, 0x24, 0x24, 0x18, 0x00 },
{ 0x18, 0x24, 0x24, 0x18, 0xFC, 0x00 },
{ 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00 },
{ 0x48, 0x54, 0x54, 0x54, 0x24, 0x00 },
{ 0x04, 0x04, 0x3F, 0x44, 0x24, 0x00 },
{ 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00 },
{ 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00 },
{ 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00 },
{ 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
{ 0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00 },
{ 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 },
{ 0x00, 0x08, 0x36, 0x41, 0x00, 0x00 },
{ 0x00, 0x00, 0x77, 0x00, 0x00, 0x00 },
{ 0x00, 0x41, 0x36, 0x08, 0x00, 0x00 },
{ 0x02, 0x01, 0x02, 0x04, 0x02, 0x00 },
{ 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0 }
};

void Send (uint8_t d) {  
  for (uint8_t bit = 0x80; bit; bit >>= 1) {
    PINA = 1<<clk;                        // clk low
    if (d & bit) PORTA = PORTA | (1<<data); else PORTA = PORTA & ~(1<<data);
    PINA = 1<<clk;                        // clk high
  }
}

void InitDisplay () {
  // Define pins
  DDRA = DDRA | 1<<clk | 1<<dc | 1<<cs | 1<<data;   // All outputs
  PORTA = PORTA | 1<<clk | 1<<dc | 1<<cs;           // All high
  //
  ClearDisplay(0);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0xD3);Send(0x00);                  // Clear scroll
  Send(0x81);Send(0xC0);                  // Increase contrast
  Send(0xAF);                             // Display on
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

// Character terminal **********************************************

uint8_t Grey = 0xF;                       // Grey level; 0 to 15

// Optimised for fast scrolling
void ClearLine (uint8_t line, uint8_t grey) {
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x00);                             // Column start low
  Send(0x10);                             // Column start high
  Send(0xB0); Send(line<<3);              // Row start
  PINA = 1<<dc;                           // dc high
  for (int i=0; i<128*8; i++) Send(grey);
  PINA = 1<<cs;                           // cs high
}

void ClearDisplay (uint8_t grey) {
  for (uint8_t p=0; p < 8; p++) ClearLine(p, grey);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x40);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

// Clears the top line, then scrolls the display up by one line
void ScrollDisplay (uint8_t *scroll) {
  ClearLine(*scroll, 0);
  *scroll = (*scroll + 1) & 0x07;
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x40 + (*scroll<<3));
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

void PlotChar (uint8_t ch, uint8_t line, uint8_t column) {
  uint8_t row = line<<3; uint8_t col = column*3;
  uint8_t off = (ch & 0x80) ? 0x7 : 0;    // Parenthesis highlight
  ch = (ch & 0x7f) - 32;
  PINA = 1<<cs;                           // cs low
  for (uint8_t r = 0 ; r<8; r++) {
    PINA = 1<<dc;                         // dc low
    Send(0x00 + (col & 0x0F));            // Column start low
    Send(0x10 + (col >> 4));              // Column start high
    Send(0xB0); Send((row+r) & 0x3F);     // Row start
    PINA = 1<<dc;                         // dc high
    for (uint8_t c = 0 ; c < 3; c++) {
      const uint8_t *adds = &CharMap[ch][c*2];
      uint8_t hi = pgm_read_byte(adds);
      uint8_t lo = pgm_read_byte(adds + 1);
      uint8_t mask = 1<<r;
      hi = hi & mask ? Grey<<4 : off<<4;
      lo = lo & mask ? Grey : off;
      Send(hi | lo);
    }
  }
  PINA = 1<<cs;                           // cs high
}

void PlotByte (int x, int y, int grey) {
  PINA = 1<<cs | 1<<dc;                   // cs and dc low
  Send(0x00 + (x & 0x0F));                // Column start low
  Send(0x10 + (x >> 4));                  // Column start high
  Send(0xB0); Send(63-y);                 // Row start
  PINA = 1<<dc;                           // dc high
  Send(grey);
  PINA = 1<<cs;                           // cs high
}

const int LastColumn = 41;

// Prints a character to display, with cursor, handling control characters
void Display (char c) {
  static uint8_t Line = 0, Column = 0, Scroll = 0;
  // These characters don't affect the cursor
  if (c == 8) {                    // Backspace
    if (Column == 0) {
      Line--; Column = LastColumn;
    } else Column--;
    return;
  }
  if (c == 9) {                    // Cursor forward
    if (Column == LastColumn) {
      Line++; Column = 0;
    } else Column++;
    return;
  }
  if ((c >= 17) && (c <= 20)) {    // Parentheses
    if (c == 17) PlotChar('(', Line+Scroll, Column);
    else if (c == 18) PlotChar('(' | 0x80, Line+Scroll, Column);
    else if (c == 19) PlotChar(')', Line+Scroll, Column);
    else PlotChar(')' | 0x80, Line+Scroll, Column);
    return;
  }
  // Hide cursor
  PlotChar(' ', Line+Scroll, Column);
  if (c == 0x7F) {                 // DEL
    if (Column == 0) {
      Line--; Column = LastColumn;
    } else Column--;
  } else if ((c & 0x7f) >= 32) {   // Normal character
    PlotChar(c, Line+Scroll, Column++);
    if (Column > LastColumn) {
      Column = 0;
      if (Line == 7) ScrollDisplay(&Scroll); else Line++;
    }
  // Control characters
  } else if (c == 12) {            // Clear display
    ClearDisplay(0); Line = 0; Column = 0;
  } else if (c == '\n') {          // Newline
    Column = 0;
    if (Line == 7) ScrollDisplay(&Scroll); else Line++;
  } else if (c == 7) tone(4, 440, 125); // Beep
  // Show cursor
  PlotChar(0x7F, Line+Scroll, Column);
}

// Keyboard **********************************************************************************

const int ColumnsC = 0b01111100;            // Columns 0 to 4 in port C
const int ColumnsD = 0b11111100;            // Columns 5 to 11 in port D
const int RowBits  = 0b00001111;            // Rows 0 to 4 in port B

// Character set - stored in program memory
const char Keymap[] PROGMEM = 
// Without shift
"1234567890\b" "qwertyuiop\n" "asdfghjkl \e" " zxcvbnm()."
// With shift
"\'\"#=-+/*\\;%" "QWERTYUIOP:" "ASDFGHJKL ~" "?ZXCVBNM<>,";

// Parenthesis highlighting
void Highlight (int p, uint8_t invert) {
  if (p) {
    for (int n=0; n < p; n++) Display(8);
    Display(17 + invert);
    for (int n=1; n < p; n++) Display(9);
    Display(19 + invert);
    Display(9);
  }
}

ISR(TIMER1_OVF_vect, ISR_NOBLOCK) {
  static uint8_t column = 0, nokey = 0;
  uint8_t rows, shift, row;
  // Check rows and shift key
  shift = (PINC & 1<<PINC7) ? 0 : 1;
  rows = PINB & RowBits;
  if (rows == RowBits) { if (nokey < 11) nokey++; }
  else if (nokey < 11) nokey = 0;
  else {
    nokey = 0; row = 0;
    while ((rows & (1<<row)) != 0) row++;
    char c = pgm_read_byte(&Keymap[(3-row)*11 + column + 44*shift]);
    ProcessKey(c);
  }
  // Take last column high and next column low
  if (column < 5) PORTC = PORTC | 1<<(6-column); else PORTD = PORTD | 1<<(12-column);
  column = (column + 1) % 11;   // 0 to 10
  if (column < 5) PORTC = PORTC & ~(1<<(6-column)); else PORTD = PORTD & ~(1<<(12-column));
}
  
void ProcessKey (char c) {
  static int parenthesis = 0;
  if (c == 27) { setflag(ESCAPE); return; }    // Escape key
  // Undo previous parenthesis highlight
  Highlight(parenthesis, 0);
  parenthesis = 0;
  // Edit buffer
  if (c == '\n') {
    pserial('\n');
    KybdAvailable = 1;
    ReadPtr = 0;
    return;
  }
  if (c == 8) {     // Backspace key
    if (WritePtr > 0) {
      WritePtr--;
      Display(0x7F);
      if (WritePtr) c = KybdBuf[WritePtr-1];
    }
  } else if (WritePtr < KybdBufSize) {
    KybdBuf[WritePtr++] = c;
    Display(c);
  }
  // Do new parenthesis highlight
  if (c == ')') {
    int search = WritePtr-1, level = 0;
    while (search >= 0 && parenthesis == 0) {
      c = KybdBuf[search--];
      if (c == ')') level++;
      if (c == '(') {
        level--;
        if (level == 0) parenthesis = WritePtr-search-1;
      }
    }
    Highlight(parenthesis, 1);
  }
  return;
}

void InitKybd () {
  // Make rows input pullups
  PORTB = PORTB | RowBits;
  // Make shift key input pullup
  PORTC = PORTC | 1<<PINC7;
  // Make columns outputs
  DDRC = DDRC | ColumnsC;         // Columns 0 to 4
  DDRD = DDRD | ColumnsD;         // Columns 5 to 11
  // Take columns high
  PORTC = PORTC | ColumnsC;       // Columns 0 to 4
  PORTD = PORTD | ColumnsD;       // Columns 5 to 11
  // Start timer for interrupt
  TCCR1A = 1<<WGM10;              // Fast PWM mode, 8-bit
  TCCR1B = 1<<WGM12 | 3<<CS10;    // Divide clock by 64
  TIMSK1 = 1<<TOIE1;              // Overflow interrupt
}

// Setup

void initenv () {
  GlobalEnv = NULL;
  tee = bsymbol(TEE);
}

void setup () {
  InitDisplay();
  InitKybd();
  #if defined (serialmonitor)
  pinMode(8, INPUT_PULLUP); // RX0
  Serial.begin(9600);
  int start = millis();
  while (millis() - start < 5000) { if (Serial) break; }
  #endif
  initworkspace();
  initenv();
  initsleep();
  pfstring(PSTR("uLisp 4.3 "), pserial); pln(pserial);
}

// Read/Evaluate/Print loop

void repl (object *env) {
  for (;;) {
    RandomSeed = micros();
    gc(NULL, env);
    #if defined (printfreespace)
    pint(Freespace, pserial);
    #endif
    if (BreakLevel) {
      pfstring(PSTR(" : "), pserial);
      pint(BreakLevel, pserial);
    }
    pserial('>'); pserial(' ');
    object *line = read(gserial);
    if (BreakLevel && line == nil) { pln(pserial); return; }
    if (line == (object *)KET) error2(NIL, PSTR("unmatched right bracket"));
    push(line, GCStack);
    pfl(pserial);
    line = eval(line, env);
    pfl(pserial);
    printobject(line, pserial);
    pop(GCStack);
    pfl(pserial);
    pln(pserial);
  }
}

void loop () {
  if (!setjmp(exception)) {
    #if defined(resetautorun)
    volatile int autorun = 12; // Fudge to keep code size the same
    #else
    volatile int autorun = 13;
    #endif
    if (autorun == 12) autorunimage();
  }
  // Come here after error
  delay(100); while (Serial.available()) Serial.read();
  clrflag(NOESC); BreakLevel = 0; nonote(4);
  for (int i=0; i<TRACEMAX; i++) TraceDepth[i] = 0;
  #if defined(sdcardsupport)
  SDpfile.close(); SDgfile.close();
  #endif
  #if defined(lisplibrary)
  if (!tstflag(LIBRARYLOADED)) { setflag(LIBRARYLOADED); loadfromlibrary(NULL); }
  #endif
  repl(NULL);
}
