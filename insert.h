/* (C) 2015 Tom Wright */

/*
 * Enters a mode that allows user to insert text into the buffer.
 * Records undo information for the edits made.  Doesn't record
 * the step for the undo, so you can combine this with other edits
 * into one step.  Returns 0 if everything is ok, and -1 on error.
 */
int insertmode(char *filename, char *t);
