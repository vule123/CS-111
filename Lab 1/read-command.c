// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <ctype.h>		// for function isalnum()
#include <error.h>
#include <limits.h>		// for INT_MAX check
#include <stdbool.h>	// for boolean functions
#include <stdio.h>		// for EOF
#include <stdlib.h>		// for function to free memory
#include <string.h>		// for strchr()

// Implementation of a stack for a command tree
typedef struct stack *stack_t;
struct stack
{
	command_t commands[4];
	int cmd_num;
};

int size(stack_t s)
{
	return s->cmd_num;
}

int is_empty(stack_t s)
{
	return s->cmd_num == 0;
}

command_t push(stack_t s, command_t cmd)
{
	s->commands[s->cmd_num] = cmd;
	s->cmd_num++;

	return cmd;
}

command_t pop (stack_t s)
{
	s->cmd_num--;
	return s->commands[s->cmd_num];
}

command_t peek(stack_t s)
{
	return s->commands[s->cmd_num - 1];
}
// End of stack implementation

// the linked list of tokens, storing type, line number, and content (content only for subshells and words)
typedef struct token *token_t;
struct token
{
	enum token_type type;
	char* content;
	int line;
	token_t next;
};

// the linked list of token lists
typedef struct token_stream *token_stream_t;
struct token_stream
{
	token_t head;
	token_stream_t next;
};

// Create a new token with specified type and pointer to content string
token_t new_token (enum token_type type, char* content, int line)
{
	token_t a_token = checked_malloc(sizeof(struct token));

	a_token->type = type;
	a_token->content = content;
	a_token->line = line;
	a_token->next = NULL;

	return a_token;
}

// Determine if a character can be part of a simple word
bool is_valid_word (char c)
{
	if (isalnum(c) || strchr("!%+,-./:@^_", c) != NULL)
		return true;

	return false;
}

// Free memory associated with a token stream
void free_token (token_stream_t head_stream)
{
	token_stream_t cur_stream = head_stream;
	token_stream_t prev_stream;

	while (cur_stream != NULL)
	{
		token_t cur = cur_stream->head;
		token_t prev;

		while (cur != NULL)
		{
			prev = cur;
			cur = cur->next;
			free(prev);
		}

		prev_stream = cur_stream;
		cur_stream = cur_stream->next;
		free(prev_stream);
	}
}

// Free memory associated with a command tree
void free_command (command_t cmd)
{
	int i = 1;
	switch(cmd->type)
	{
		case SUBSHELL_COMMAND:
			free(cmd->input); 
			free(cmd->output);
			free_command(cmd->u.subshell_command);
			free(cmd->u.subshell_command);
			break;

		case SIMPLE_COMMAND:
			free(cmd->input); 
			free(cmd->output);
			while(cmd->u.word[i])
			{
				free(cmd->u.word[i]);
				i++;
			}
			break;

		case AND_COMMAND:
		case OR_COMMAND:
		case PIPE_COMMAND:
		case SEQUENCE_COMMAND:
			free_command(cmd->u.command[0]); 
			free(cmd->u.command[0]);
			free_command(cmd->u.command[1]); 
			free(cmd->u.command[1]);
			break;

		default:
			error(1, 0, "Invalid command type");
			break;
	};
}

// Convert a command input into a token stream
token_stream_t make_token_stream (char* script, size_t script_size)
{
	token_t head_token = new_token(HEAD, NULL, -1);
	token_t cur_token = head_token;
	token_t prev_token = NULL;

	token_stream_t head_stream = checked_malloc(sizeof(struct token_stream));
	token_stream_t cur_stream = head_stream;
	cur_stream->head = head_token;

	int line = 1;
	size_t index = 0;
	char c = *script;
	while (index < script_size)
	{
		if (c == '(') // SUBSHELL
		{
			int subshell_line = line;
			int nested = 1;

			size_t count = 0;
			size_t subshell_size = 64;
			char* subshell = checked_malloc(subshell_size);

			// grab contents until subshell is closed
			while (nested > 0)
			{
				script++; 
				index++; 
				c = *script;
				if (index == script_size)
				{
					error(1, 0, "Line %d: We reached EOF without closing the subshell.", subshell_line);
					return NULL;
				}

				if (c == '\n')
				{
					char next_c = *(++script);
					// consume all following whitespace
					while (next_c == ' ' || next_c == '\t' || next_c == '\n')
					{
						if (next_c == '\n')
							line++;

						next_c = *(++script);
						index++;
					}

					// pass semicolon
					//c = ';';
					line++;
				}
				else if (c == '(') 
					nested++;
				else if (c == ')') 
				{
					nested--;

					if (nested == 0) // all subshells are already closed
					{
						script++; 
						index++; 
						c = *script; 
						break;
					}
				}

				// load into subshell buffer
				subshell[count] = c;
				count++;

				// expand subshell buffer if necessary
				if (count == subshell_size)
				{
					subshell_size = subshell_size * 2;
					subshell = checked_grow_alloc (subshell, &subshell_size);
				}
			}

			// create a subshell token
			cur_token->next = new_token(SUBSHELL, subshell, subshell_line);
			cur_token = cur_token->next;
		}
		else if (c == ')') // CLOSE PARENTHESIS
		{
			error(1, 0, "Line %d: Error! There is no matching open parenthesis.", line);
			return NULL;
		}
		else if (c == '<') 
		{
			// get the next character
			script++; 
			index++; 
			c = *script; 

			if(c == '&')	// INPUT2
			{
				cur_token->next = new_token(INPUT2, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;	
			}
			else if (c == '>')	// OPEN
			{
				cur_token->next = new_token(OPEN, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;	
			}
			else	// LEFT REDIRECT
			{
				cur_token->next = new_token(LEFT, NULL, line);
				cur_token = cur_token->next;
			}
		}
		else if (c == '>') 
		{
			// get the next character
			script++; 
			index++; 
			c = *script; 

			if(c == '&')	// OUTPUT2
			{
				cur_token->next = new_token(OUTPUT2, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;	
			}
			else if (c == '>')	// APPEND
			{
				cur_token->next = new_token(APPEND, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;	
			}
			else if (c == '|')	// OUTPUT_C
			{
				cur_token->next = new_token(OUTPUT_C, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;	
			}
			else	// RIGHT REDIRECT
			{
				cur_token->next = new_token(RIGHT, NULL, line);
				cur_token = cur_token->next;
			}
		}
		else if (c == '&') 
		{
			// get the next character
			script++; 
			index++; 
			c = *script;

			if (c == '&') // AND
			{
				cur_token->next = new_token(AND, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;
			}
			else	// error! & can't be alone
			{
				error(1, 0, "Line %d: Error! & can't be alone.", line);
				return NULL;
			}
		}
		else if (c == '|') 
		{
			// get the next character
			script++; 
			index++; 
			c = *script; 

			if (c == '|') // OR
			{
				cur_token->next = new_token(OR, NULL, line);
				cur_token = cur_token->next;

				script++; 
				index++; 
				c = *script;
			}
			else // PIPE
			{
				cur_token->next = new_token(PIPE, NULL, line);
				cur_token = cur_token->next;
			}
		}
		else if (c == ';') // SEMICOLON
		{
			cur_token->next = new_token(SEMICOLON, NULL, line);
			cur_token = cur_token->next;

			script++; 
			index++; 
			c = *script;
		}
		else if (c == ' ' || c == '\t') // SPACES
		{
			// skip all spaces
			script++; 
			index++; 
			c = *script;
		}
		else if (c == '\n') // NEWLINE
		{
			line++;

			// check the previous character
			if (cur_token->type == LEFT ||	cur_token->type == RIGHT || cur_token->type == APPEND || cur_token->type == INPUT2 || 
				cur_token->type == OUTPUT2 || cur_token->type == OPEN || cur_token->type == OUTPUT_C)
			{
				error(1, 0, "Line %d: Error! Newline character can't be after redirects.", line);
				return NULL;
			}
			else if (cur_token->type == WORD || cur_token->type == SUBSHELL)
			{
				// the command tree is complete, start a new command tree
				if (cur_token->type != HEAD)
				{
					cur_stream->next = checked_malloc(sizeof(struct token_stream));
					cur_stream = cur_stream->next;
					cur_stream->head = new_token(HEAD, NULL, -1);
					cur_token = cur_stream->head;
				}
			}
			else	// command is not complete, keep checking the next character
			{
				script++;
				index++;
				c = *script;
			}
		}
		else if (is_valid_word(c)) // WORD
		{
			size_t count = 0;
			size_t word_size = 8;
			char* word = checked_malloc(word_size);

			do
			{
				word[count] = c;
				count++;

				// if not enough space, expand word buffer
				if (count == word_size)
				{
					word_size = word_size * 2;
					word = checked_grow_alloc(word, &word_size);
				}

				script++; 
				index++; 
				c = *script;
			} while (is_valid_word(c) && index < script_size);

			// create word token
			cur_token->next = new_token(WORD, word, line);
			cur_token = cur_token->next;
		}
		else // INVALID CHARACTER
		{
			error(1, 0, "Line %d: Error! Encounter an invalid character.", line);
			return NULL;
		}
	}

	return head_stream;
}

// To create a branch, first pop one operator and two operands and put the branch in the operand stack
bool create_branch (stack_t operators, stack_t operands)
{
	if (size(operands) < 2)		// error, the number of operands can't be less than 2
		return false;
		

	// pop 2 operands from the operand stack
	command_t right_operand = pop(operands);
	command_t left_operand = pop(operands);

	// pop 1 operator from the operator stack
	command_t result_cmd = pop(operators);

	result_cmd->u.command[0] = left_operand;
	result_cmd->u.command[1] = right_operand;

	// push the result command on the operand stack
	push(operands, result_cmd);

	return true;
}

// Convert a list of tokens into a command tree
command_t make_command_tree (token_t head_token)
{
	token_t cur_token = head_token;

	int line = cur_token->line; 

	// create the operator stack and the operand stack
	stack_t operators = checked_malloc(sizeof(struct stack));	
	operators->cmd_num = 0;
	stack_t operands = checked_malloc(sizeof(struct stack)); 
	operands->cmd_num = 0;

	command_t prev_cmd = NULL;
	command_t cur_cmd;


	// go through all tokens
	do
	{
		if(!(cur_token->type == LEFT || cur_token->type == RIGHT || cur_token->type == INPUT2 || cur_token->type == OUTPUT2 
			|| cur_token->type == OPEN || cur_token->type == OUTPUT_C)) 
			cur_cmd = checked_malloc(sizeof(struct command));
			

		switch (cur_token->type)
		{
			case SUBSHELL:
				cur_cmd->type = SUBSHELL_COMMAND;

				// process subshell command tree
				cur_cmd->u.subshell_command = make_command_tree(make_token_stream(cur_token->content, strlen(cur_token->content))->head);

				// push the subshell command on the operand stack
				push(operands, cur_cmd);
				break;

			case LEFT:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					error(1, 0, "Line %d: Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Error! Previous command already has output. ", line);
					return NULL;
				}
				else if (prev_cmd->input != NULL)
				{
					error(1, 0, "Line %d: Error! Previous command already has input.", line);
					return NULL;
				}

				// go to the next token
				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the left redirect
				{
					prev_cmd->input = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Error! Words must follow the left redirect.", line);
					return NULL;
				}
	
				break;
						
			case RIGHT:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{

					error(1, 0, "Line %d: Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Error! Previous command already has output.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->output = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Error! Words must follow the right redirect.", line);
					return NULL;
				}

				break;

			case APPEND:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					
					error(1, 0, "Line %d: Append Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Append Error! Previous command already has output.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->append = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Append Error! Words must follow the append redirect.", line);
					return NULL;
				}
				
				break;

			case INPUT2:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					error(1, 0, "Line %d: Input2 Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Input2 Error! Previous command already has output.", line);
					return NULL;
				}
				else if (prev_cmd->input != NULL)
				{
					error(1, 0, "Line %d: Error! Previous command already has input.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->input2 = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Input2 Error! Words must follow the input2 redirect.", line);
					return NULL;
				}
				
				break;

			case OUTPUT2:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					
					error(1, 0, "Line %d: Output2 Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Output2 Error! Previous command already has output.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->output2 = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Output2 Error! Words must follow the output2 redirect.", line);
					return NULL;
				}
				
				break;

			case OUTPUT_C:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					
					error(1, 0, "Line %d: Output_c Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}
				else if (prev_cmd->output != NULL)
				{
					error(1, 0, "Line %d: Output_c Error! Previous command already has output.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->output_c = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Output_c Error! Words must follow the output_c redirect.", line);
					return NULL;
				}
				
				break;

			case OPEN:
				// check if the previous command is a simple command or a subshell command
				if (prev_cmd == NULL || !(prev_cmd->type == SIMPLE_COMMAND || prev_cmd->type == SUBSHELL_COMMAND))
				{
					
					error(1, 0, "Line %d: Open Error! Redirects must follow a simple or subshell command.", line);
					return NULL;
				}

				cur_token = cur_token->next;
				if (cur_token->type == WORD) // if a word follows the right redirect
				{
					prev_cmd->open = cur_token->content;
				}
				else
				{
					error(1, 0, "Line %d: Open Error! Words must follow the open redirect.", line);
					return NULL;
				}
				
				break;
				
			case AND:
				cur_cmd->type = AND_COMMAND;

				// if the precedence of AND <= the precedence of the top op in the stack, pop the top op to make a branch
				if (!is_empty(operators) && (peek(operators)->type == PIPE_COMMAND || peek(operators)->type == OR_COMMAND || peek(operators)->type == AND_COMMAND))
				{
					if (!create_branch(operators, operands))
					{
						error(1, 0, "Line %d: Error! Can't create a new branch.", line);
						return NULL;
					}
				}

				// push AND on the operator stack
				push(operators, cur_cmd);
				break;

			case OR:
				cur_cmd->type = OR_COMMAND;

				// if the precedence of OR <= the precedence of the top op in the stack, pop the top op to make a branch
				if (!is_empty(operators) && (peek(operators)->type == PIPE_COMMAND || peek(operators)->type == OR_COMMAND || peek(operators)->type == AND_COMMAND))
				{
					if (!create_branch(operators, operands))
					{
						error(1, 0, "Line %d: Error! Can't create a new branch.", line);
						return NULL;
					}
				}

				// push OR on the operator stack
				push(operators, cur_cmd);
				break;

			case PIPE:
				cur_cmd->type = PIPE_COMMAND;
				// if the precedence of PIPE <= the precedence of the top op in the stack, pop the top op to make a branch
				if (!is_empty(operators) && peek(operators)->type == PIPE_COMMAND)
				{
					if (!create_branch(operators, operands))
					{
						error(1, 0, "Line %d: Error! Can't create a new branch.", line);
						return NULL;
					}
				}

				// push PIPE on the operator stack
				push(operators, cur_cmd);
				break;

			case SEMICOLON:
				cur_cmd->type = SEQUENCE_COMMAND;

				// the precedence of SEMICOLON <= all other operators, always pop the top op in the stack to make a branch
				if (!is_empty(operators))
				{
					if (!create_branch(operators, operands))
					{
						error(1, 0, "Line %d: Error! Can't create a new branch.", line);
						return NULL;
					}
				}

				// push SEMICOLON on the operator stack
				push(operators, cur_cmd);
				break;

			case WORD:
				cur_cmd->type = SIMPLE_COMMAND;

				// count number of following words
				size_t num_words = 1;
				token_t cur_c = cur_token;
				while (cur_c->next != NULL && cur_c->next->type == WORD)
				{
					num_words++;
					cur_c = cur_c->next;
				}

				cur_cmd->u.word = checked_malloc((num_words+1) * sizeof(char*));

				// put words into word array in the command
				cur_cmd->u.word[0] = cur_token->content;
				size_t i;
				for (i = 1; i < num_words; i++)
				{
					cur_token = cur_token->next;
					cur_cmd->u.word[i] = cur_token->content;
				}

				cur_cmd->u.word[num_words] = NULL;

				// push the word on the oprand stack
				push(operands, cur_cmd);
				break;

			default:
				break;
		};
		prev_cmd = cur_cmd; // save the curent command in prev_cmd for checking in the next while loop

	} while(cur_token != NULL && (cur_token = cur_token->next) != NULL);

	// when the tree is complete, the operator stack should have at least 1 operator
	while(size(operators) > 0)
	{
		if (!create_branch(operators, operands))
		{
			error(1, 0, "Line %d: Error! Can't create a new branch.", line);
			return NULL;
		}
	}

	// check for single root
	if (size(operands) != 1)
	{
		error(1, 0, "Line %d: Error! There is more than 1 operand left at the end.", line);
		return NULL;
	}

	command_t tree_root = pop(operands); // the root points to the only operand that remains in the stack
	// free memory of the 2 stacks
	free(operators); 
	free(operands);

	return tree_root;
}

// Traverse a command tree and return a list of all redirect files.
input_files_t get_dependencies (command_t c)
{
	input_files_t list = NULL;
	input_files_t shell_list = NULL;
	input_files_t cur = NULL;
	int count;

	switch (c->type)
	{
		case SUBSHELL_COMMAND:
			shell_list = get_dependencies(c->u.subshell_command);

		case SIMPLE_COMMAND:
			if (c->input)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->input;
				cur->next = NULL;
				list = cur;
			}

			if (c->output)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->output;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
					
			}

			if (c->append)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->append;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
			}

			if (c->input2)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->input2;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
			}

			if (c->output2)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->output2;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
			}

			if (c->output_c)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->output_c;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
			}

			if (c->open)
			{
				cur = checked_malloc(sizeof(struct input_files));
				cur->file = c->open;
				cur->next = NULL;

				if (list) {
					list->next = cur;
				}	
				else {
					list = cur;
				}
			}

			if (shell_list)
			{
				cur = list;
				list = shell_list;
				list->next = cur;
			}
			break;			

		case AND_COMMAND:
		case OR_COMMAND:
		case SEQUENCE_COMMAND:
		case PIPE_COMMAND:
			list = get_dependencies(c->u.command[0]);	// get the first list from u.command[0]
			if (list) // if the first list is not empty, go to the end of the first list followed by the second list from u.command[1]
			{
				cur = list;
				while (cur->next != NULL)
					cur = cur->next;

				cur->next = get_dependencies(c->u.command[1]);
			}
			else {
				list = get_dependencies(c->u.command[1]);	// get the second list from u.command[1]
			}
				
			break;

		default:
			error(1, 0, "Error! Invalid command type.");
			break;
	}
	return list;
}

// Return 1 if the list of input files and a command stream have some common dependencies
int common_dependencies (input_files_t f1, input_files_t f2)
{
	if (f1 == NULL || f2 == NULL)
		return 0;
	else
	{
		input_files_t f1_cur = f1;
		input_files_t f2_cur = NULL;

		while (f1_cur != NULL)
		{
			f2_cur = f2;
			while (f2_cur != NULL)
			{
				if (strcmp(f1_cur->file, f2_cur->file) == 0)
					return 1;
				f2_cur = f2_cur->next;
			}
			f1_cur = f1_cur->next;
		}
	}
	return 0;
}

// Convert a token stream into a command forest
command_stream_t make_command_forest (token_stream_t token_stream)
{
	token_stream_t cur_stream = token_stream;

	// initialize a command forest
	command_stream_t head_tree = NULL;
	command_stream_t cur_tree = NULL;
	command_stream_t prev_tree = NULL;

	while (cur_stream != NULL && cur_stream->head->next != NULL)
	{
		token_t cur = cur_stream->head->next;	// skip the head token and begin with the first token
		command_t cmd = make_command_tree(cur);	

		cur_tree = checked_malloc(sizeof(struct command_stream));
		cur_tree->cmd = cmd;
		cur_tree->dependencies = get_dependencies(cmd);

		if (!head_tree)
		{
			head_tree = cur_tree;
			prev_tree = head_tree;
		}
		else
		{
			prev_tree->next = cur_tree;
			prev_tree = cur_tree;
		}
		cur_stream = cur_stream->next;
	}
	return head_tree;
}

// Make a command stream out of an input shell script file
command_stream_t make_command_stream (int (*getbyte) (void *), void *arg)
{
	size_t count = 0;
	size_t buffer_size = 1024;
	char* buffer = checked_malloc(buffer_size);
	char next_c;

	do
	{
		next_c = getbyte(arg);

		if (next_c == '#') // skip all the comments
		{
			do
			{
				next_c = getbyte(arg);
			} while (next_c != -1 && next_c != EOF && next_c != '\n');
		}

		if (next_c != -1)
		{
			buffer[count] = next_c;
			count++;

			// expand the buffer if not enough space
			if (count == INT_MAX)
			{
				error(1, 0, "Error! The input exceeds INT_MAX.");
			}
			else if (count == buffer_size)
			{
				buffer_size = buffer_size * 2;
				buffer = checked_grow_alloc (buffer, &buffer_size);
			}
		}
	} while(next_c != -1);

	// make the token stream from the buffer
	token_stream_t head_token_stream = make_token_stream(buffer, count);
	
	if (head_token_stream == NULL)
	{
		error(1, 0, "Error! Can't make a command forest.");
		return NULL;
	}
	command_stream_t command_stream = make_command_forest(head_token_stream);

	// free memory 
	free(buffer);
	free_token(head_token_stream);

	return command_stream;
}

// Read the command stream 
command_t read_command_stream (command_stream_t s)
{
	if (s == NULL || s->cmd == NULL)
		return NULL;

	command_t c = s->cmd;

	// move to the next command stream
	if (s->next != NULL)
	{
		command_stream_t next_c = s->next;
		s->cmd = s->next->cmd;
		s->next = s->next->next;

		free(next_c); 
	}
	else
		s->cmd = NULL;
		
	return c;
}
