// Link layer protocol implementation

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "link_layer.h"
#include "protocol.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayer connectionParameters;

// Alarm
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
  alarmCount++;
  alarmEnabled = TRUE;

  printf("Alarm Enabled! \n");
}

void alarmDisable()
{
  alarm(0);
  alarmEnabled = FALSE;
  alarmCount = 0;
}

int receiveFrame(uint8_t expectedAddress, uint8_t expectedControl)
{
  t_state state = START;

  printf("\n Waiting for bytes to read... \n");

  while (state != STOP)
  {
    uint8_t buf = 0;
    int bytes = readByteSerialPort(&buf);

    if (bytes < 0)
    {
      printf("Error trying to read frame! \n");
      return -1;
    }
    
    if (bytes > 0)
    {
      printf("Read byte: 0x%02X \n", buf);

      switch (state)
      {
        case START:
          if (buf == FLAG)
            state = FLAG_RCV;
          break;

        case FLAG_RCV:
          if (buf == FLAG)
            continue;
          if (buf == expectedAddress)
            state = A_RCV;
          else
            state = START;
          break;

        case A_RCV:
          if (buf == expectedControl)
            state = C_RCV;
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;

        case C_RCV:
          if (buf == (expectedControl ^ expectedAddress))
            state = BCC_OK;
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;

        case BCC_OK:
          if (buf == FLAG)
            state = STOP;
          else
            state = START;
          break;

        default:
          state = START;
      }
      
      /*
      if (state == START)
        printf("Sent back to start! \n");

      if (state == FLAG_RCV)
        printf("Read flag! \n");

      if (state == A_RCV)
        printf("Read address! \n");

      if (state == C_RCV)
        printf("Read controller! \n");

      if (state == BCC_OK)
        printf("Read BCC! \n");

      if (state == STOP) {
        printf("Read the last flag! \n");
        printf("Correctly read the header! \n");
      }
      */

    }
  }

  return 0;
}

int transmitFrame(uint8_t expectedAddress, uint8_t expectedControl, t_frame_type frameType)
{
  t_state state = START;

  (void)signal(SIGALRM, alarmHandler);

  if (writeBytesSerialPort(frame_buffers[frameType], BUF_SIZE) < 0)
    return -1;
  
  printf("Wrote frame! \n");

  alarm(connectionParameters.timeout);

  while (state != STOP && alarmCount <= connectionParameters.nRetransmissions)
  {
    uint8_t buf = 0;
    int bytes = readByteSerialPort(&buf);

    if (bytes < 0)
    {
      printf("Error trying to read frame! \n");
      return -1;
    }

    if (bytes > 0)
    {
      printf("Read byte: 0x%02X \n", buf);

      switch (state)
      {
        case START:
          if (buf == FLAG)
            state = FLAG_RCV;
          break;

        case FLAG_RCV:
          if (buf == FLAG)
            continue;
          if (buf == expectedAddress)
            state = A_RCV;
          else
            state = START;
          break;

        case A_RCV:
          if (buf == expectedControl)
            state = C_RCV;
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;

        case C_RCV:
          if (buf == (expectedControl ^ expectedAddress))
            state = BCC_OK;
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;

        case BCC_OK:
          if (buf == FLAG)
            state = STOP;
          else
            state = START;
          break;

        default:
          state = START;
      }

      /*
      if (state == START)
        printf("Sent back to start! \n");

      if (state == FLAG_RCV)
        printf("Read flag! \n");

      if (state == A_RCV)
        printf("Read address! \n");

      if (state == C_RCV)
        printf("Read controller! \n");

      if (state == BCC_OK)
        printf("Read BCC! \n");

      if (state == STOP) {
        printf("Read the last flag! \n");
        printf("Correctly read the header! \n");
      }*/

    }

    if (state == STOP)
    {
      alarmDisable();
      return 0;
    }

    if (alarmEnabled)
    {
      alarmEnabled = FALSE;

      if (alarmCount <= connectionParameters.nRetransmissions)
      {
        if (writeBytesSerialPort(frame_buffers[frameType], BUF_SIZE) < 0)
            return -1;

        printf("Retransmiting frame! \n");
        alarm(connectionParameters.timeout);
      }

      state = START;      
    }
  }

  alarmDisable();
  return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopen(LinkLayer connection)
{
  memcpy(&connectionParameters, &connection, sizeof(connection));
  
  if (openSerialPort(connectionParameters.serialPort,
                     connectionParameters.baudRate) < 0)
    return -1;

  switch (connectionParameters.role)
  {
    case LlTx:
      if (transmitFrame(ADDR_SEND, CTRL_UA, SET))
        return -1;
      printf("Connected! \n");
      break;
    case LlRx:
      if (receiveFrame(ADDR_SEND, CTRL_SET))
        return -1;
      if (writeBytesSerialPort(frame_buffers[UA_Rx], BUF_SIZE) < 0)
       return -1;
      printf("Connected! \n");
      break;
  }

  return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
  switch (connectionParameters.role)
  {
    case LlTx:
      if (transmitFrame(ADDR_RCV, CTRL_DISC, DISC_Tx))
        break;
      if (writeBytesSerialPort(frame_buffers[UA_Tx], BUF_SIZE) < 0)
        break;
      printf("Disconnected! \n");
      break;
    case LlRx:
      if (receiveFrame(ADDR_SEND, CTRL_DISC))
        break;
      if (transmitFrame(ADDR_RCV, CTRL_UA, DISC_Rx))
        break;
      printf("Disconnected! \n");
      break;
  }

  return closeSerialPort();
}
