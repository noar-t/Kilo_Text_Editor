# Kilo Text Editor
This is a C based text editor. This project is following [this guide](http://viewsourcecode.org/snaptoken/kilo/index.html). This is meant to be good practice for making C based command line applications.
## Features
1. Has support for syntax highlighting based on filetype. Only C is native, but more can be added in the provided template.
2. Supports searching through documents.
3. Can create documents or open existing files.
4. Prevents users from closing document if changes are present.
## Screenshot
![kiloscrnsht](https://i.imgur.com/edA9nYd.png)
## Building Kilo
Kilo requires make and gcc to compile. To make use the provided make file.
```
make
./kilo.ex <file>
```
