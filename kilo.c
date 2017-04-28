/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

typedef struct erow { // a row of a file
  int size;
  char *chars;
} erow;


struct editorConfig {
  int cx, cy;
  int rowoff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  struct termios orig_termios; // default values of terminal
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {  // error handling
  write(STDOUT_FILENO, "\x1b[2J", 4); //clear whole screen
  write(STDOUT_FILENO, "\x1b[H", 3);  //cursor top left
  
  perror(s);
  exit(1); // error exit code
}


void disableRawMode() {  //puts terminal into raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) // catch terminal attribute errors
    die("tcsetattr");
}


void enableRawMode() {  //puts terminal into raw mode
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) // catch terminal attribute errors
    die("tcsetattr");
  
  atexit(disableRawMode); // set disableRawMode to be called upon exit

  // various flags to completely enable raw mode
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);        //sets raw input flag | canoncial flag | disable literal input | siginit/sigtstp

  // indexes for c_cc field which controls behavior of read, sets timeout to 1/10 of a sec
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;


  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)    //catch terminal attribute errors
    die("tcsetattr");
}


int editorReadKey() {  // waits and reads in a valid char and returns it
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {// arrow keys behave like wasd
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) 
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) 
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') { // handles pageup, pagedown
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      }
      else { 
        switch (seq[1]) { // handes arrow key input
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    }
    else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  }
  else {
    return c;
  }
}


int getCursorPosition(int *rows, int *cols) { // get positon of cursor in terminal:
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


int getWindowSize(int *rows, int *cols) { // gets size of terminal
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)  // puts curor bottom left
      return -1;
    return getCursorPosition(rows, cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

void editorAppendRow(char *s, size_t len) { // add a row to screen
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** file i/o ***/

void editorOpen(char *filename) { // open and read a file line by line and pass it to append
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == '\r'))
      linelen--;

    editorAppendRow(line, linelen);
  }

  free(line);
  fclose(fp);
}


/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) { // putting chars into a buffer
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return ;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { // frees space/destructure
  free(ab->b);
}

/*** output ***/

void editorScroll() {
  if (E.cy < E.rowoff) { // check if above visible window and scolls accordingly
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) { // check if below visible window and scolls accordingly
    E.rowoff = E.cy - E.screenrows + 1;
  }
}


void editorDrawRows(struct abuf *ab) { // draws rows
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) { // draw welcome and version
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
            "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;

        int padding  = (E.screencols - welcomelen) / 2; // centering version
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);

        abAppend(ab, welcome, welcomelen);
      }
      else {
        abAppend(ab, "~", 1); // draws tilde like vim
      }
    }
    else {
      int len = E.row[filerow].size;
      if (len > E.screencols)
        len = E.screencols;
      abAppend(ab, E.row[filerow].chars, len);
    }

    abAppend(ab, "\x1b[K", 3); // clear line
    if (y < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}


void editorRefreshScreen() {
  editorScroll();
  
  struct abuf ab = ABUF_INIT;
  
  abAppend(&ab, "\x1b[?25l", 6); // hide cursor so dpesnt flicker
  abAppend(&ab, "\x1b[H", 3);  // cursor top left

  editorDrawRows(&ab);

  char buf[32]; // put cursor at E.cx and E.cy
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // show cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** input ***/

void editorMoveCursor(int key) {// increments/decrements cursor position
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0)
        E.cx--;
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1)
        E.cx++;
      break;
    case ARROW_UP:
      if (E.cy != 0)
        E.cy--;
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows)
        E.cy++;
      break;
  }
}


void editorProcessKeypress() { // process char from editorReadKey()
  int c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4); // escape sequence (x1b), then clear whole screen
      write(STDOUT_FILENO, "\x1b[H", 3);  // cursor top left
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      E.cx = E.screencols - 1;
      break;
      
    case PAGE_UP:
    case PAGE_DOWN: {
        int times = E.screenrows;
        while (times--) // sroll a whole page/ move cursor a whole page
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
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
  E.cx = 0;      // cursor x
  E.cy = 0;      // cursor y
  E.rowoff = 0;  // row offset
  E.numrows = 0; // rows in file
  E.row = NULL;  // file row array

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) 
    die("getWindowSize");
}


int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
