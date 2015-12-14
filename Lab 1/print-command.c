// UCLA CS 111 Lab 1 command printing, for debugging

#include "command.h"
#include "command-internals.h"

#include <stdio.h>
#include <stdlib.h>

static void
command_indented_print (int indent, command_t c)
{
  switch (c->type)
    {
    case AND_COMMAND:
    case SEQUENCE_COMMAND:
    case OR_COMMAND:
    case PIPE_COMMAND:
      {
	command_indented_print (indent + 2 * (c->u.command[0]->type != c->type),
				c->u.command[0]);
	static char const command_label[][3] = { "&&", ";", "||", "|" };
	printf (" \\\n%*s%s\n", indent, "", command_label[c->type]);
	command_indented_print (indent + 2 * (c->u.command[1]->type != c->type),
				c->u.command[1]);
	break;
      }

    case SIMPLE_COMMAND:
      {
	char **w = c->u.word;
	printf ("%*s%s", indent, "", *w);
	while (*++w)
	  printf (" %s", *w);
	break;
      }

    case SUBSHELL_COMMAND:
      printf ("%*s(\n", indent, "");
      command_indented_print (indent + 1, c->u.subshell_command);
      printf ("\n%*s)", indent, "");
      break;

    default:
      abort ();
    }

  if (c->input)
    printf ("<%s", c->input);
  if (c->output)
    printf (">%s", c->output);
  if (c->input2)
    printf ("<&%s", c->input2);
  if (c->output2)
    printf (">&%s", c->output2);
  if (c->open)
    printf ("<>%s", c->open);
  if (c->append)
    printf (">>%s", c->append);
  if (c->output_c)
    printf (">|%s", c->output_c);
}

void
print_command (command_t c)
{
  command_indented_print (2, c);
  putchar ('\n');
}
