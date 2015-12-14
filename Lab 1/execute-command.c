// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int command_status (command_t c)
{
	return c->status;
}

// Execute a command
void execute_command (command_t cmd, int time_travel)
{
	pid_t child;
	int fd[2];
	int read = 0;
	int write = 1;

	switch (cmd->type)
	{
		case SIMPLE_COMMAND:
			child = fork();

			if (child == 0)		// for child process
			{
				if (cmd->input)
				{
					int in;
					if ((in = open(cmd->input, O_RDONLY, 0666)) == -1)
						error(1, 0, "Can't open input file.");

					if (dup2(in, 0) == -1)
						error(1, 0, "Can't do input redirect.");

					close(in);
				}

				if (cmd->output)
				{
					int out;
					if ((out = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1)
						error(1, 0, "Can't open output file.");

					if (dup2(out, 1) == -1)
						error(1, 0, "Can't do output redirect.");

					close(out);
				}

				// execute the command
				execvp(cmd->u.word[0], cmd->u.word);
				error(1, 0, "Can't execute the command.");
			}
			else if (child > 0)		// for parent process
			{
				int status;
				waitpid(child, &status, 0);		// wait for the child process to complete
				cmd->status = status;
			}
			else
				error(1, 0, "Can't create a child process.");
			break;

		case AND_COMMAND:	// if the first command succeeds, execute the second command
			execute_command(cmd->u.command[0], time_travel);
			cmd->status = cmd->u.command[0]->status;

			if (!cmd->status)
			{
				execute_command(cmd->u.command[1], time_travel);
				cmd->status = cmd->u.command[1]->status;
			}
			break;

		case OR_COMMAND:	// if the first command fails, execute the second command
			execute_command(cmd->u.command[0], time_travel);
			cmd->status = cmd->u.command[0]->status;

			if (cmd->status)
			{
				execute_command(cmd->u.command[1], time_travel);
				cmd->status = cmd->u.command[1]->status;
			}
			break;
			
		case PIPE_COMMAND:
			if (pipe(fd) == -1)
				error(1, 0, "Can't create a pipe.");

			child = fork();
			if (child == 0) 
			{
				close(fd[read]); 

				if (dup2(fd[write], write) == -1)
					error(1, 0, "Can't write to pipe.");

				execute_command(cmd->u.command[0], time_travel);
				cmd->status = cmd->u.command[0]->status;

				close(fd[write]); 
				exit(0);
			}
			else if (child > 0) 
			{
				int status;
				waitpid(child, &status, 0);

				close(fd[write]); 

				if (dup2(fd[read], read) == -1)
					error(1, 0, "Can't read from pipe.");

				execute_command(cmd->u.command[1], time_travel);
				cmd->status = cmd->u.command[1]->status;

				close(fd[read]); 
			}
			else
				error(1, 0, "Can't create a child process.");

			break;

		case SUBSHELL_COMMAND:
			execute_command(cmd->u.subshell_command, time_travel);
			cmd->status = cmd->u.subshell_command->status;
			break;

		case SEQUENCE_COMMAND:
			if (time_travel && !(common_dependencies(get_dependencies(cmd->u.command[0]), get_dependencies(cmd->u.command[1]))))
			{
				child = fork();
				if (child == 0) 
				{
					execute_command(cmd->u.command[0], time_travel);
					exit(0);
				}
				if (child > 0) 
				{
					execute_command(cmd->u.command[1], time_travel);
				}
				else
					error(1, 0, "Can't create a child process.");

				waitpid(child, NULL, 0);	// wait for the child process to finish
			}
			else
			{
				execute_command(cmd->u.command[0], time_travel);
				execute_command(cmd->u.command[1], time_travel);
			}

			cmd->status = cmd->u.command[1]->status;
			break;

		default:
				error(1, 0, "Invalid command type.");
			break;
	}
}

// Time travel parallelism to execute command stream
int execute_time_travel(command_stream_t cmd_stream, int N)
{
	while(cmd_stream != NULL)
	{
		command_stream_t list = NULL;
		command_stream_t list_cur = NULL;
		command_stream_t prev_cmd = NULL;
		command_stream_t cur_cmd = cmd_stream;
		int runnable = 0;
		
		while(cur_cmd != NULL)
		{
			if(list == NULL)
			{
				list = cur_cmd;
				list_cur = cur_cmd;
				cmd_stream = cur_cmd->next;
				cur_cmd = cur_cmd->next;
				runnable = 1;
			}
			else
			{
				// check if 2 command streams have a dependent
				int dependents = 0;
				command_stream_t stream = list;
				while(stream != NULL)
				{
					if(common_dependencies(cur_cmd->dependencies, stream->dependencies))
					{
						dependents = 1;
						break;
					}
					stream = stream->next;
				}
				
				// if there is no dependent
				if(dependents == 0)
				{
					if(prev_cmd == NULL)
					{
						list_cur->next = cur_cmd;
						list_cur = cur_cmd;
						cmd_stream = cur_cmd->next;
						cur_cmd = cur_cmd->next;
					}
					else
					{
						list_cur->next = cur_cmd;
						list_cur = cur_cmd;
						prev_cmd->next = cur_cmd->next;
						cur_cmd = cur_cmd->next;
					}
					if(runnable < N)
						runnable++;
				}
				else
				{
					prev_cmd = cur_cmd;
					cur_cmd = cur_cmd->next;
				}
			}
			list_cur->next = NULL;
		}
		
		pid_t* children = checked_malloc(runnable * sizeof(pid_t));
		int i = 0;
		
		if(list != NULL)
		{
			command_t cmd;
			cur_cmd = list;
			while(cur_cmd)
			{
				pid_t child = fork();
				if(child == 0)
				{
					execute_command(cur_cmd->cmd, 1);
					exit(0);
				}
				else if(child > 0)
				{
					children[i] = child;
				}
				else
					error(1, 0, "Can't create a child process");
				
				i++;
				cur_cmd = cur_cmd->next;
			}
			
			// parent process waits for children to complete
			int wait;	
			do
			{
				wait = 0;
				int j;
				for(j = 0; j < runnable; j++)
				{
					if(children[j] > 0)
					{
						if(waitpid(children[j], NULL, 0) != 0)
							children[j] = 0;
						else
							wait = 1;
					}
					sleep(0);
				}
			} while(wait == 1);
		}
		
		free(children);	// free memory allocated for children
		
		// free command stream
		cur_cmd = list;
		prev_cmd = NULL;
		while(cur_cmd)
		{
			// free command tree
			free_command(cur_cmd->cmd);
			free(cur_cmd->cmd);
			
			// free input files
			input_files_t file_cur = cur_cmd->dependencies;
			input_files_t file_prev = NULL;
			while(file_cur)
			{
				file_prev = file_cur;
				file_cur = file_cur->next;
				free(prev_cmd);
			}
			free(cur_cmd->dependencies);
			prev_cmd = cur_cmd;
			cur_cmd = cur_cmd->next;
		}
	}
	return 0;
}
