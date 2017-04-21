/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 'a',
  ARROW_RIGHT = 'd',
  ARROW_UP = 'w',
  ARROW_DOWN = 's'
};

/*** data ***/
struct editorConfig {
  int cx, cy;
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

  if (c == '\x1b') {//arrow keys behave like wasd
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) 
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) 
      return '\x1b';

    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }

    return '\x1b';
  }
  else {
    return c;
  }
}


int getCursorPosition(int *rows, int *cols) { //get positon of cursor in terminal:
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) 
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i] , 1) != 1)
      break;
    if (buf[i] == 'R') 
      break;
    i++;  
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') 
    return -1;
  if (sscanf(&buf[2], "%d:%d", rows,cols) != 2)
    return -1;

  return 0;
}


int getWindowSize(int *rows, int *cols) { //gets size of terminal
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)  //puts curor bottom left
      return -1;
    return getCursorPosition(rows, cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/***append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) { //putting chars into a buffer
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return ;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { //frees space/destructure
  free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) { // draws tilde on left like vim
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screencols)
        welcomelen = E.screencols;

      int padding  = (E.screencols - welcomelen) / 2; //centering version
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--)
        abAppend(ab, " ", 1);

      abAppend(ab, welcome, welcomelen);
    }
    else {
      abAppend(ab, "~", 1);
    }

    abAppend(ab, "\x1b[K", 3); //clear line
    if (y < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}


void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  
  abAppend(&ab, "\x1b[?25l", 6); //hide cursor sp dpesnt flicker
  abAppend(&ab, "\x1b[H", 3);  //cursor top lefta

  editorDrawRows(&ab);

  char buf[32]; //put cursor at E.cx and E.cy
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); //show cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** input ***/

void editorMoveCursor(char key) {//increments/decrements cursor position
  switch (key) {
    case ARROW_LEFT:
      E.cx--;
      break;
    case ARROW_RIGHT:
      E.cx++;
      break;
    case ARROW_UP:
      E.cy--;
      break;
    case ARROW_DOWN:
      E.cy++;
      break;
  }
}


void editorProcessKeypress() { //process char from editorReadKey()
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4); //escape sequence (x1b), then clear whole screen
      write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top left
      exit(0);
      break;

    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
    case ARROW_DOWN:
      editorMoveCursor(c);
      break;
  }
}
      

/*** init ***/

void initEditor() {
  E.cx = 0;
  E.cy = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) 
    die("getWindowSize");
}


int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
