// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#include "console.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  cprintf("cpu with apicid %d: panic: ", cpu->apicid);
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

static void move_cursor( int indexMovement )
{
  int pos;
  // Cursor position: col + 80*row.
  //Copied from functions above...
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);
  pos = pos + indexMovement;//Move our position based on given distance. indexMovement can be pos or neg.
  //Copied from cgaputc, move our cursor on the screen
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos >> 8 );
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      move_cursor(movement);//Move our cursor, reset the distance value
      movement = 0;
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e - movement != input.w){

        int j, index;
        for( j = 0; j < movement; j++ )
        {
          index = input.e + j - movement;
          input.buf[(index-1) % INPUT_BUF] = input.buf[index % INPUT_BUF];
        }

        move_cursor(movement);
        consputc(BACKSPACE);
        move_cursor(-movement);
        input.e--;
        for( j = 0; j < movement; j++ )
        {
          index = input.e - movement + j;
          c = input.buf[index % INPUT_BUF];
          consputc(c);
        }
        move_cursor(-movement);
      }
      break;
    case upArrow:
      move_cursor(movement);
      movement = 0;
      while( input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n')
      {
        input.e--;
        consputc(BACKSPACE);
      }
      current_index--;
      if( current_index < 0 )
        current_index += MAX_HISTORY;

      current_index = current_index % MAX_HISTORY;
      int j = 0;
      while(history_buffer[current_index][j] != '\0')
      {
        input.buf[input.e % INPUT_BUF] = history_buffer[current_index][j];
        input.e++;
        consputc(history_buffer[current_index][j]);
        j++;
      }
      break;
    case downArrow:
      if( current_index != next_index )
      {
        move_cursor( movement );
        movement = 0;
        while( input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n')
        {
          input.e--;
          consputc(BACKSPACE);
        }
        current_index++;
        current_index = current_index % MAX_HISTORY;
        int j = 0;
        while( history_buffer[current_index][j] != '\0')
        {
          input.buf[input.e % INPUT_BUF] = history_buffer[current_index][j];
          input.e++;
          consputc(history_buffer[current_index][j]);
          j++;
        }
      }
      break;
    case rightArrow:
      if( movement > 0 )
      {
        move_cursor(1);
        movement--;
      }
      break;
    case leftArrow:
      if( movement < input.e - input.w )
      {
        move_cursor(-1);
        movement++;

        uartputc('\b');
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        if( c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          int j;
          if( input.e != input.w )
          {
            
            for(j = 0; j < ( input.e + INPUT_BUF - input.w) % INPUT_BUF; ++j )
            {
              history_buffer[next_index][j] = input.buf[ (input.w + j) % INPUT_BUF];
            }
            history_buffer[next_index][j+1] = '\0';
            next_index++; 
            if( command_count < MAX_HISTORY )
              command_count++;
          }
          move_cursor(movement);
          movement = 0;
          current_index = next_index;
        }
        int index;
        for( j = 0; j < movement; j++ )
        {
          index = input.e - j;
          input.buf[index % INPUT_BUF] = input.buf[(index - 1) % INPUT_BUF];
        }
        input.buf[(input.e-movement) % INPUT_BUF] = c;
        input.e++;
        consputc(c);

        for( j = 0; j < movement; j++ )
        {
          index = input.e - movement + j;
          consputc(input.buf[index % INPUT_BUF]);
        }
        move_cursor(-movement);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(proc->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int history( char * buffer, int historyId)
{
  if( historyId < 0 || historyId > 15 )
    return -2;
  if( MAX_HISTORY-historyId > command_count)
    return -1;
  memset(buffer, '\0', INPUT_BUF);
  int temp = (current_index - 1 + historyId) % MAX_HISTORY;
  memmove( buffer, history_buffer[temp], strlen(history_buffer[temp]));
  return 0;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  picenable(IRQ_KBD);
  ioapicenable(IRQ_KBD, 0);
}

