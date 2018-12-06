#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2017 <+YOU OR YOUR COMPANY+>.
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

import numpy, time, math, struct
import pmt
from datetime import datetime
from gnuradio import gr
import blocksat
from pipe import Pipe


# Constants
BLOCKSAT_PKT_HEADER_FORMAT = '!c7x'
BLOCKSAT_PKT_HEADER_LEN    = 8
TYPE_BLOCK                 = b'\x00'
TYPE_API_DATA              = b'\x01'
OUT_DATA_HEADER_FORMAT     = '32sQ'
OUT_DATA_HEADER_LEN        = 40
OUT_DATA_DELIMITER         = 'vyqzbefrsnzqahgdkrsidzigxvrppato'

def convert_size(size_bytes):
    """Convert a number of bytes to a string with dynamic byte units"""
    if size_bytes == 0:
        return "0B"
    unit = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
    i = int(math.floor(math.log(size_bytes, 1024)))
    p = math.pow(1024, i)
    s = round(size_bytes / p, 2)
    return "%4s %2s" % (s, unit[i])


class InvalidHeader(Exception):
    """Invalid header exception"""
    pass


class BlocksatPacket():
    def __init__(self, raw_packet):
        try:
            header_data = struct.unpack(BLOCKSAT_PKT_HEADER_FORMAT,
                                        raw_packet[:BLOCKSAT_PKT_HEADER_LEN])
        except:
            raise InvalidHeader('Invalid header.')

        # Fill in the information from the header
        self.pkt_type = chr(ord(header_data[0]) & ord(b'\x01'))

        # Check the more fragments field
        self.more_fragments = (ord(header_data[0]) & ord(b'\x80')) != 0

        # Fill the payload
        self.payload = raw_packet[BLOCKSAT_PKT_HEADER_LEN:]


class protocol_sink(gr.basic_block):
    """Blockstream Satellite Protocol Sink

    Receives incoming GNU Radio PDUs containing Blocksat Packet and
    demultiplexes the incoming payload into separate outputs, one for blocks and
    the other for user data transmitted via API.

    This module is associated to one input **message port**, which receives the
    Blocksat Packets as GNU Radio PDUs. The module has no output port on the GNU
    Radio flowgrpah, but it creates two named pipes and outputs data to them.

    """
    def __init__(self, blocks_pipe, user_pipe, protocol_version, disable_api):
        gr.basic_block.__init__(self,
                                name="protocol_sink",
                                in_sig=[],
                                out_sig=[])
        # Input parameters
        self.blocks_pipe      = Pipe(blocks_pipe)
        self.user_pipe        = Pipe(user_pipe) if (not disable_api) else None
        self.protocol_version = protocol_version
        self.disable_api      = disable_api

        # Buffer to hold API data
        self.api_buffer = b''

        # Stats
        self.blocks_cnt   = 0
        self.api_data_cnt = 0

        # Print control
        self.print_period = 10
        self.next_print   = time.time() + 2*self.print_period

        # Register the message port
        self.message_port_register_in(pmt.intern('async_pdu'))
        self.set_msg_handler(pmt.intern('async_pdu'), self.handle_msg)

    def output_packet_data(self, packet):
        """Output the content of a Blocksat Packet to named pipe(s)
        """
        if packet.pkt_type == TYPE_API_DATA and (not self.disable_api):
            # Save data in buffer
            self.api_buffer += packet.payload

            # Wait the last fragment and then output the data within a structure
            if (not packet.more_fragments):
                # Struct has delimiter and message length
                out_data_header = struct.pack(OUT_DATA_HEADER_FORMAT,
                                              OUT_DATA_DELIMITER,
                                              len(self.api_buffer))

                # Output to pipe
                self.user_pipe.write(out_data_header + self.api_buffer)

                # And clean the API data buffer
                self.api_buffer = b''

            self.api_data_cnt += len(packet.payload)

        elif packet.pkt_type == TYPE_BLOCK:
            self.blocks_pipe.write(packet.payload)
            self.blocks_cnt += len(packet.payload)

        # Is it time to print data stats?
        current_t = time.time()
        if (current_t > self.next_print):
            # Print data counts with timestamp
            timestamp  = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print("--------------------------------------------------------------------------------")
            print('[%s] Rx Data\t Blocks: %7s\tUser API: %7s' %(
                timestamp, convert_size(self.blocks_cnt),
                convert_size(self.api_data_cnt)))
            print("--------------------------------------------------------------------------------")

            # Schedule next print
            self.next_print = current_t + self.print_period

    def handle_msg(self, msg_pmt):
        """Processing incoming GNU Radio PDU with Blocksat PDU

        Args:
            msg_pmt : the message containing the Blocksat PDU
        """

        # Get metadata and information from message
        meta = pmt.to_python(pmt.car(msg_pmt))
        msg  = pmt.cdr(msg_pmt)

        # Validate (message should be a vector of u8, i.e. bytes)
        if not pmt.is_u8vector(msg):
            print "[ERROR] Received invalid message type.\n"
            return

        # Convert incoming packet to a bytes string
        raw_pkt = b''.join([chr(x) for x in pmt.u8vector_elements(msg)])

        # Read the
        if (self.protocol_version > 1):
            # Parse Blocksat Packet
            packet = BlocksatPacket(raw_pkt)

            # Output packet content
            self.output_packet_data(packet)
        else:
            # Write raw data directly to named pipe
            self.blocks_pipe.write(raw_pkt)

