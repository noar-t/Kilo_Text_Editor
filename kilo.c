#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;                     //default values of terminal so we can reset upon exit

void die(const char *s) {  //exits with nonzero value setting errno to indicate what the error was
  perror(s);
  exit(1);
}

void disableRawMode() {  //puts terminal into raw mode
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {  //puts terminal into raw mode
  tcgetattr(STDIN_FILENO, &orig_termios);
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


  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  char c;
  while (1) {

    char c = '\0';
    read(STDIN_FILENO, &c, 1);

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
