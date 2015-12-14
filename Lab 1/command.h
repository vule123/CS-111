// UCLA CS 111 Lab 1 command interface

typedef struct input_files *input_files_t;
typedef struct command *command_t;
typedef struct command_stream *command_stream_t;

// the linked list of files that are used by a command 
struct input_files
{
	char* file;
	input_files_t next;
};

// the linked list of all commands
struct command_stream
{
	command_t cmd;
	input_files_t dependencies;		// file dependencies
	command_stream_t next;
};

/* Create a command stream from LABEL, GETBYTE, and ARG.  A reader of
   the command stream will invoke GETBYTE (ARG) to get the next byte.
   GETBYTE will return the next input byte, or a negative number
   (setting errno) on failure.  */
command_stream_t make_command_stream (int (*getbyte) (void *), void *arg);

/* Free memory associated with a command tree  */
void free_command (command_t cmd);

/* Go through a command tree and return a list of all redirect files.  */
input_files_t get_dependencies (command_t c);

/* Return 1 if the list of input files and a command stream have some common dependencies  */
int common_dependencies (input_files_t f1, input_files_t f2);

/* Read a command from STREAM; return it, or NULL on EOF.  If there is
   an error, report the error and exit instead of returning.  */
command_t read_command_stream (command_stream_t stream);

/* Print a command to stdout, for debugging.  */
void print_command (command_t);

/* Execute a command.  Use "time travel" if the integer flag is
   nonzero.  */
void execute_command (command_t, int);

/* Return the exit status of a command, which must have previously been executed.
   Wait for the command, if it is not already finished.  */
int command_status (command_t);
