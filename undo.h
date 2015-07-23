/* (C) 2015 Tom Wright */

/*
 * Record actions for undo.  Start / end are the start and end
 * points of the action in the buffer.  Inserts should be recorded
 * just after the text is added to the buffer.  Deletes should be
 * recorded just before the text is deleted, so that the text can be
 * copied into the undo system.  Returns 0 on success and -1 on failure.
 */
int recinsert(char *start, char *end);
int recdelete(char *start, char *end);
void recstep(void);

/*
 * Perform an undo / redo.  Returns 0 on success and -1 on failure.
 */
int undo(void);
int redo(void);
