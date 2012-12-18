/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  $Id$

  Author(s):  Zihan Chen

  (C) Copyright 2011-2012 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#ifndef __AMPIO_H__
#define __AMPIO_H__

#include "BoardIO.h"

class AmpIO : public BoardIO
{
public:
    AmpIO(unsigned char board_id, unsigned int numAxes = 4);
    ~AmpIO();

    void DisplayReadBuffer();

    //  Interface methods
    unsigned long GetStatus();
    unsigned long GetTimestamp();
    unsigned long GetMotorCurrent(    unsigned int );
    unsigned long GetAnalogPosition(  unsigned int );
    unsigned long GetEncoderPosition( unsigned int );
    unsigned long GetEncoderVelocity( unsigned int );
    unsigned long GetEncoderFrequency(unsigned int );

    bool SetPowerControl(                  const unsigned long );
    bool SetMotorCurrent(    unsigned int, const unsigned long );
    bool SetEncoderPreload(  unsigned int, const unsigned long );

private:

    unsigned int NumAxes;   // not currently used

    // Number of channels in the node
    // Only 4 axes exist on 1 QLA board;
    // Due to FPGA firmware implementation, NUM_CHANNELS is set to 7 (soon to be 8)
    enum { NUM_CHANNELS = 7 };

    enum { ReadBufSize = 2+4*NUM_CHANNELS,
           WriteBufSize = NUM_CHANNELS };

    quadlet_t read_buffer[ReadBufSize];     // buffer for real-time reads
    quadlet_t write_buffer[WriteBufSize];   // buffer for real-time writes

    // Offsets of real-time read buffer contents, 30 quadlets
    enum {
        TIMESTAMP_OFFSET  =  0,    // one quadlet
        STATUS_OFFSET     =  1,    // one quadlet
        MOTOR_CURR_OFFSET =  2,    // half quadlet x7 channels (lower half)
        ANALOG_POS_OFFSET =  2,    // half quadlet x7 channels (upper half)
        ENC_POS_OFFSET    =  9,    // one quadlet x7 channels
        ENC_VEL_OFFSET    = 16,    // one quadlet x7 channels
        ENC_FRQ_OFFSET    = 23     // one quadlet x7 channels
    };      // negate for sign to match that of motor command voltage

    // offsets of real-time write buffer contents, 7 quadlets
    enum {
        WB_CURR_OFFSET = 0         // one quadlet x7 channels
    };

    // Hardware device address offsets, not to be confused with buffer offsets.
    // Format is [4-bit channel address (1-7) | 4-bit device offset]
    // Applies only to quadlet transactions--block transactions use fixed
    // real-time formats described above
    enum {
        ADC_DATA_OFFSET = 0,       // adc data register
        DAC_CTRL_OFFSET = 1,       // dac control register
        POT_CTRL_OFFSET = 2,       // pot control register
        POT_DATA_OFFSET = 3,       // pot data register
        ENC_LOAD_OFFSET = 4,       // enc control register (preload)
        POS_DATA_OFFSET = 5,       // enc data register (position)
        VEL_DATA_OFFSET = 6,       // enc data register (velocity)
        FRQ_DATA_OFFSET = 7        // enc data register (frequency)
    };
};

#endif // __AMPIO_H__
