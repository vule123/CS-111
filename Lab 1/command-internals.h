// UCLA CS 111 Lab 1 command internals

// Command types
enum command_type
{
    AND_COMMAND,         // A && B
    SEQUENCE_COMMAND,    // A ; B
    OR_COMMAND,          // A || B
    PIPE_COMMAND,        // A | B
    SIMPLE_COMMAND,      // a simple command
    SUBSHELL_COMMAND,    // ( A )
};

// Token types
enum token_type
{
	HEAD,			// dummy head of a token list
	AND,			// &&
	OR,				// ||
	PIPE,			// |
	SEMICOLON,		// ;
	SUBSHELL,		// (command)
	WORD,			// ASCII, digits, or: ! % + , - . / : @ ^ _ 
	LEFT,			// <
	RIGHT,			// >
	APPEND,			// >>
	INPUT2,			// <&
	OUTPUT2,		// >&
	OPEN,			// <>
	OUTPUT_C		// >|
};

// Data associated with a command.
struct command
{
  enum command_type type;

  // Exit status, or -1 if not known (e.g., because it has not exited yet).
  int status;

  // I/O redirections, or 0 if none.
  char *input;
  char *output;
  
  char *append;		// >>
  char *input2;		// <&
  char *output2;	// >&
  char *open;		// <>
  char *output_c;	// >|

  union
  {
    // for AND_COMMAND, SEQUENCE_COMMAND, OR_COMMAND, PIPE_COMMAND:
    struct command *command[2];

    // for SIMPLE_COMMAND:
    char **word;

    // for SUBSHELL_COMMAND:
    struct command *subshell_command;
  } u;
};
