/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;                     //default values of terminal so we can reset upon exit
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {  //exits with nonzero value setting errno to indicate what the error was
  write(STDOUT_FILENO, "\x1b[2J", 4); //clear whole screen
  write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top left
  
  perror(s);
  exit(1);
}


void disableRawMode() {  //puts terminal into raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)    //catch terminal attribute errors
    die("tcsetattr");
}


void enableRawMode() {  //puts terminal into raw mode
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)    //catch terminal attribute errors
    die("tcsetattr");
  
  atexit(disableRawMode);                        //set disableRawMode to be called upon exit

  // various flags to completely enable raw mode
  struct termios raw = E.orig_termios;
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


char editorReadKey() {  //waits and reads in a valid char and returns it
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void editorDrawRows() { // draws tilde on left like vim
  int y;
  for (y = 0; y < 24; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}


void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); //escape sequence (x1b), then clear whole screen
  write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top lefta

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top lefta
}

/*** input ***/

void editorProcessKeypress() { //process char from editorReadKey()
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4); //escape sequence (x1b), then clear whole screen
      write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top left
      exit(0);
      break;
  }
}
      

/*** init ***/

int main() {
  enableRawMode();

  while (1) {
    editorProcessKeypress();
  }
  return 0;
}
