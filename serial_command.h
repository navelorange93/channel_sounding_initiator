#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

/**************************************************************************//**
 * Initialize the serial command parser.
 *****************************************************************************/
void serial_command_init(void);

/**************************************************************************//**
 * Poll the VCOM stream for control commands.
 *****************************************************************************/
void serial_command_process_action(void);

#endif // SERIAL_COMMAND_H
