/*
Written by Delroy Hiebert
Comp 4430 Operating Systems 2
For Dr. Rasit Eskicioglu
*/
#include "export.h"

int main(int argc, char *argv[])
{
  //Arg1: name, Arg2: Path value
  if (argc != 3) {
    printf(2, "usage: export PATH path1:path2:...\n");
    exit();
  }

  if( strcmp(argv[1], "PATH") != 0)
  {
    printf(1, "Environment variable %s not supported\n", argv[1]);
    exit();
  }

  char * pathValue = argv[2];

  printf(1, "Adding path variable...\n%s\n", pathValue);
  add_directory(pathValue);

  exit();
}

int add_directory(char *pathstr)
{
  char *pathStart = pathstr;
  char *currentChar;

  //Ignore all colons
  while(*pathStart == DELIMETER)
    pathStart++;

  //If there's no characters other than colons, return
  if(*pathStart == NULL_CHAR)
    return - 1;
  currentChar = pathStart;

  //Loop until we hit the end of the string
  while(*currentChar != NULL_CHAR) {
    if(*currentChar == DELIMETER) {
      *currentChar = NULL_CHAR;
      add_dir(pathStart);
      //Move past where the colon was
      pathStart = currentChar + 1;
      //Move forward until we no longer see a colon (usually one char)
      while(*pathStart == DELIMETER)
        pathStart++;
      //Quit if we hit the end
      if(*pathStart == NULL_CHAR)
        exit();
      currentChar = pathStart;
    }

    currentChar++;
  }
  //Add the last directory path
  add_dir(pathStart);
  exit();
}
