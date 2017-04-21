#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


/*** data ***/
struct termios orig_termios;                     //default values of terminal so we can reset upon exit

void die(const char *s) {  //exits with nonzero value setting errno to indicate what the error was
  perror(s);
  exit(1);
}

/*** terminal setting ***/

void disableRawMode() {  //puts terminal into raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)    //catch terminal attribute errors
    die("tcsetattr");
}

void enableRawMode() {  //puts terminal into raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)    //catch terminal attribute errors
    die("tcsetattr");
  
  atexit(disableRawMode);                        //set disableRawMode to be called upon exit

  // various flags to completely enable raw mode
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);        //sets raw input flag | canoncial flag | disable literal input | siginit/sigtstp

  //indexes for c_cc field which controls behavior of read, sets timeout to 1/10 of a sec
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;


  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)    //catch terminal attribute errors
    die("tcsetattr");
}

/*** init ***/

int main() {
  enableRawMode();

  while (1) {

    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)  //catch read errors
      die("read");

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    }
    else{
      printf("%d ('%c')\r\n", c, c);
    }
    
    if (c == 'q') break;
  }
  return 0;
}
