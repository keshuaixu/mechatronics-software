/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/******************************************************************************
 *
 * (C) Copyright 2018-2021 Johns Hopkins University (JHU), All Rights Reserved.
 *
 * This program is used to read the Dallas DS2505 chip inside a da Vinci instrument
 * via its 1-wire interface. The 1-wire interface is implemented in the FPGA,
 * using bi-redirection digital port DOUT3, available with QLA Rev 1.4+.
 * It depends on the AmpIO library (which depends on libraw1394 and/or pcap).
 *
 * Usage: instrument [-pP] <board num>
 *        where P is the Firewire port number (default 0),
 *        or a string such as ethP and fwP, where P is the port number
 *
 ******************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include <Amp1394/AmpIORevision.h>
#include "Amp1394Time.h"
#include <chrono>

#if Amp1394_HAS_RAW1394
#include "FirewirePort.h"
#endif
#if Amp1394_HAS_PCAP
#include "EthRawPort.h"
#endif
#include "EthUdpPort.h"
#include "AmpIO.h"

void PrintDebugStream(std::stringstream &debugStream)
{
    char line[80];
    while (debugStream.getline(line, sizeof(line)))
        std::cerr << line << std::endl;
    debugStream.clear();
    debugStream.str("");
}

int main(int argc, char** argv)
{
    int i;
#if Amp1394_HAS_RAW1394
    BasePort::PortType desiredPort = BasePort::PORT_FIREWIRE;
#else
    BasePort::PortType desiredPort = BasePort::PORT_ETH_UDP;
#endif
    int port = 0;
    int board = 0;
    std::string IPaddr(ETH_UDP_DEFAULT_IP);

    if (argc > 1) {
        int args_found = 0;
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                if (argv[i][1] == 'p') {
                    if (!BasePort::ParseOptions(argv[i]+2, desiredPort, port, IPaddr)) {
                        std::cerr << "Failed to parse option: " << argv[i] << std::endl;
                        return 0;
                    }
                    std::cerr << "Selected port: " << BasePort::PortTypeString(desiredPort) << std::endl;
                }
                else {
                    std::cerr << "Usage: instrument <board-num> [-pP] [-d]" << std::endl
                              << "       where <board-num> = rotary switch setting (0-15)" << std::endl
                              << "             P = port number (default 0)" << std::endl
                              << "       can also specify -pfwP, -pethP or -pudp" << std::endl;
                    return 0;
                }
            }
            else {
                if (args_found == 0)
                    board = atoi(argv[i]);
                args_found++;
            }
        }
    }

    std::stringstream debugStream(std::stringstream::out|std::stringstream::in);
    BasePort *Port = 0;
    if (desiredPort == BasePort::PORT_FIREWIRE) {
#if Amp1394_HAS_RAW1394
        Port = new FirewirePort(port, debugStream);
#else
        std::cerr << "FireWire not available (set Amp1394_HAS_RAW1394 in CMake)" << std::endl;
        return -1;
#endif
    }
    else if (desiredPort == BasePort::PORT_ETH_UDP) {
        Port = new EthUdpPort(port, IPaddr, debugStream);
    }
    else if (desiredPort == BasePort::PORT_ETH_RAW) {
#if Amp1394_HAS_PCAP
        Port = new EthRawPort(port, debugStream);
#else
        std::cerr << "Raw Ethernet not available (set Amp1394_HAS_PCAP in CMake)" << std::endl;
        return -1;
#endif
    }
    if (!Port || !Port->IsOK()) {
        PrintDebugStream(debugStream);
        std::cerr << "Failed to initialize " << BasePort::PortTypeString(desiredPort) << std::endl;
        return -1;
    }
    AmpIO Board(board);
    Port->AddBoard(&Board);
    const size_t read_len = 0xffffff;
    // const size_t read_len = 0x1000;
    static uint16_t flash_data[read_len];
    auto start = std::chrono::steady_clock::now();
    std::ofstream file0("espm_flash_dump.bin", std::ios::binary);
    file0.seekp(read_len * sizeof(uint16_t));
    file0.write("\0\0", 2);
    file0.close();
    std::ofstream file("espm_flash_dump.bin", std::ios::binary|std::ios::out|std::ios::in);
    for (size_t i=0; i < read_len; i+=0x100) {
        quadlet_t flash_command = (1 << 24) | i ;
        Port->WriteQuadlet(board, 0xa002, flash_command);
        if (i == 0) {
            Amp1394_Sleep(0.5);
        }
        quadlet_t data;
        int retries = 0;
        while ((i & 0xffff) != ((data >> 16) & 0xffff)) {
            Amp1394_Sleep(0.002);
            Port->ReadQuadlet(board, 0xa031, data);
            retries++;
            // printf("... %x %x: %x\n", i, (data >> 16) & 0xffff, data & 0xffff);            
            if (retries > 10) {
                Amp1394_Sleep(0.01);
                Port->WriteQuadlet(board, 0xa002, flash_command);
                retries = 0;
            }
        }
        flash_data[i] = data & 0xffff;
        printf("%x: %x\n", i, data & 0xffff);
        if (flash_data[i] != 0xffff) {

            for (size_t j=1; j < 0x100; j++) {
                int retries_j = 0;
                flash_command = (1 << 24) | (i + j);
                Port->WriteQuadlet(board, 0xa002, flash_command);

                while (((i + j) & 0xffff) != ((data >> 16) & 0xffff)) {
                    Amp1394_Sleep(0.002);
                    Port->ReadQuadlet(board, 0xa031, data);
                    retries_j++;
                    if (retries_j > 10) {
                        Amp1394_Sleep(0.01);
                        Port->WriteQuadlet(board, 0xa002, flash_command);
                        retries_j = 0;
                    }                    
                }
                flash_data[i+j] = data & 0xffff;
                printf(".. %x: %x\n", i + j, data & 0xffff);
            }
        }
        file.seekp(i * 2);
        file.write(((char*)flash_data) + i * 2, 0x100 * 2);
    }
    auto end = std::chrono::steady_clock::now();
    printf("speed %f words/s\n", (double) read_len / std::chrono::duration<double>(end - start).count());

    
    file.close();    

    return 0;
}

// 0 3000
// 20000 30000
// 40000 43000
// 60000 6d000
// 80000 80500
// 100000 101000
// 600400 601000
// 800000 830000
// f20000
// f40000
// f60000
// f80000
