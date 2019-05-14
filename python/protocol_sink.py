#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2019 Blockstream Corp.
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
from threading import Thread, Lock, Event
from gnuradio import gr
import blocksat
from .pipe import Pipe


# Constants
BLOCKSAT_PKT_HEADER_FORMAT = '!c7x'
BLOCKSAT_PKT_HEADER_LEN    = 8
TYPE_BLOCK                 = b'\x00'
TYPE_API_DATA              = b'\x01'
OUT_DATA_HEADER_FORMAT     = '64sQ'
OUT_DATA_HEADER_LEN        = 64 + 8
OUT_DATA_DELIMITER         = 'vyqzbefrsnzqahgdkrsidzigxvrppato' + \
                             '\xe0\xe0$\x1a\xe4["\xb5Z\x0bv\x17\xa7\xa7\x9d' + \
                             '\xa5\xd6\x00W}M\xa6TO\xda7\xfaeu:\xac\xdc'
MAX_BUFFER_SIZE            = 10 * 2 ** 20
OVERFLOW_WARNING_POINT     = 5

def convert_size(size_bytes):
    """Convert a number of bytes to a string with dynamic byte units"""
    if size_bytes == 0:
        return "0 B"
    unit = ("B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
    i = int(math.floor(math.log(size_bytes, 1024)))
    p = math.pow(1024, i)
    s = round(size_bytes / p, 2)
    return "%5s %2s" % (s, unit[i])


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
        self.pkt_type = bytes([header_data[0][0] & b'\x01'[0]])

        # Check the more fragments field
        self.more_fragments = ([header_data[0][0] & b'\x80'[0]]) != 0

        # Fill the payload
        self.payload = raw_packet[BLOCKSAT_PKT_HEADER_LEN:]


class RateAvg():
    """Compute the moving average byte rate

    Args:
        window_len : Observation window length

    """
    def __init__(self, window_len):
        self.byte_cnt    = list()
        self.tstamp_hist = list()
        self.window_len  = window_len
        self.value       = None

    def update(self, new_total, tstamp):
        """Save byte consumption snapshots and update average"""

        # Move window - pop oldest value
        if (len(self.tstamp_hist) >= self.window_len):
            self.tstamp_hist.pop(0)
            self.byte_cnt.pop(0)

        # Append new value
        self.tstamp_hist.append(tstamp)
        self.byte_cnt.append(new_total)

        # Return if this is still the first sample
        if (len(self.tstamp_hist) == 1):
            return

        # Check observation interval represented in the window
        interval = self.tstamp_hist[-1] - self.tstamp_hist[0]

        # Total number of bytes consumed during the window
        bytes_per_window = self.byte_cnt[-1] - self.byte_cnt[0]

        # Average rate
        self.value = float(bytes_per_window) / interval


class protocol_sink(gr.basic_block):
    """Blockstream Satellite Protocol Sink

    Receives incoming GNU Radio PDUs containing Blocksat Packet and
    demultiplexes the incoming payload into separate outputs, one for blocks and
    the other for user data transmitted via API.

    This module is associated to one input **message port**, which receives the
    Blocksat Packets as GNU Radio PDUs. The module has no output port on the GNU
    Radio flowgrpah, but it creates two named pipes and outputs data to them.

    """
    def __init__(self, blk_pipe, api_pipe, protocol_version, disable_api,
                 disable_blk):
        gr.basic_block.__init__(self,
                                name="protocol_sink",
                                in_sig=[],
                                out_sig=[])
        # Validate parameters
        if (protocol_version < 1 or protocol_version > 2):
            raise ValueError("Protocol version should be either 1 or 2")

        # Input parameters
        self.protocol_version = protocol_version
        self.disable_api      = disable_api
        self.disable_blk      = disable_blk

        # Open named pipes
        if (not disable_blk):
            self.blk_pipe = Pipe(blk_pipe)

        if ((self.protocol_version > 1) and (not disable_api)):
            self.api_pipe = Pipe(api_pipe)
        else:
            self.api_pipe = None

        # Buffer to accumulate incoming API data (from API Blocksat packets)
        # until the entire user-transmitted data is accumulated
        self.api_buffer = b''

        # Stats
        self.blk_data_cnt  = 0
        self.api_data_cnt  = 0
        self.blk_byte_rate = RateAvg(5) # Compute over 5 secs. sample ~ once/sec
        self.api_byte_rate = RateAvg(5) # Compute over 5 secs, sample ~ once/sec
        self.stats_period  = 1
        self.next_stats    = time.time()

        # Print control
        self.print_period            = 10
        self.next_print              = time.time() + 2*self.print_period
        self.last_api_warning        = 0
        self.last_blk_warning        = 0
        self.api_ovflw_print_pending = False
        self.blk_ovflw_print_pending = False

        # Initialize buffers to store output API and blocks packets
        self.api_data_buffer          = list()
        self.blk_data_buffer          = list()
        self.api_data_buffer_data_cnt = 0
        self.blk_data_buffer_data_cnt = 0

        # Count the amount of data that has been deleted from the data buffer
        # and not yet informed via a console log
        self.api_data_buffer_del_cnt  = 0
        self.blk_data_buffer_del_cnt  = 0

        # Define threads to consume API and blocks data separately
        self.api_consumer_en      = True
        self.blk_consumer_en      = True
        self.api_data_available   = Event()
        self.blk_data_available   = Event()
        self.api_data_buffer_lock = Lock()
        self.blk_data_buffer_lock = Lock()
        api_consumer              = Thread(target=self.api_consumer)
        blk_consumer              = Thread(target=self.blk_consumer)
        api_consumer.daemon       = True
        blk_consumer.daemon       = True

        # Start threads
        if (not disable_api):
            api_consumer.start()

        if (not disable_blk):
            blk_consumer.start()

        # Register the message port
        self.message_port_register_in(pmt.intern('async_pdu'))
        self.set_msg_handler(pmt.intern('async_pdu'), self.handle_msg)

    def handle_pkt(self, packet):
        """Process an incoming Blocksat Packet
        """
        if packet.pkt_type == TYPE_API_DATA and (not self.disable_api):
            # Accumulate data from the packet's payload in a buffer
            self.api_buffer += packet.payload

            # Wait until the last fragment and then save the data for output
            if (not packet.more_fragments):
                # Read the API data buffer occupancy in the beginning
                self.api_data_buffer_lock.acquire()
                api_data_buffer_occ = len(self.api_data_buffer)

                # Save the entire buffer, now containing all of the data
                # transmitted by an API user that was carried over one or more
                # Blocksat packets
                self.api_data_buffer.append(self.api_buffer)
                self.api_data_buffer_data_cnt += len(self.api_buffer)
                self.api_data_buffer_lock.release()

                # Warn about an imminent buffer overflow and in the case of
                # overflow, throw away old data
                buffer_occ_ratio = (float(self.api_data_buffer_data_cnt) /
                                    MAX_BUFFER_SIZE) * 100
                if (buffer_occ_ratio > OVERFLOW_WARNING_POINT and
                    buffer_occ_ratio > (self.last_api_warning + 1) and
                    buffer_occ_ratio < 100):
                    print("%s API data buffer is %d%% full" %(
                        "[" + time.strftime("%Y-%m-%d %H:%M:%S") + "]",
                        buffer_occ_ratio
                    ))
                    print("Is your application running and connected to \"%s\"?"
                          %(self.api_pipe.name))
                    self.last_api_warning = buffer_occ_ratio

                elif (buffer_occ_ratio >= 100):
                    # Throw away old data until it fits
                    self.api_data_buffer_lock.acquire()
                    throw_away_cnt = 0
                    while (self.api_data_buffer_data_cnt > MAX_BUFFER_SIZE):
                        api_data = self.api_data_buffer.pop(0)
                        throw_away_cnt += len(api_data)
                        self.api_data_buffer_data_cnt -= len(api_data)

                    self.api_data_buffer_lock.release()

                    # Keep track of the overall amount of data deleted
                    self.api_data_buffer_del_cnt += throw_away_cnt

                    # Set to print buffer overflow message to console
                    self.api_ovflw_print_pending = True

                # Flag data availability if buffer was empty previously
                if (api_data_buffer_occ == 0):
                    self.api_data_available.set()

                # Clean the API data buffer
                self.api_buffer = b''

            # Global and monotonic API data counter
            self.api_data_cnt += len(packet.payload)

        elif packet.pkt_type == TYPE_BLOCK and (not self.disable_blk):
            # Read the API data buffer occupancy and save the blocks data
            self.blk_data_buffer_lock.acquire()
            blk_data_buffer_occ = len(self.blk_data_buffer)
            self.blk_data_buffer.append(packet.payload)
            self.blk_data_buffer_data_cnt += len(packet.payload)
            self.blk_data_buffer_lock.release()

            # Warn about an imminent buffer overflow
            buffer_occ_ratio = (float(self.blk_data_buffer_data_cnt) /
                                MAX_BUFFER_SIZE) * 100
            if (buffer_occ_ratio > OVERFLOW_WARNING_POINT and
                buffer_occ_ratio > (self.last_blk_warning + 1) and
                buffer_occ_ratio < 100):
                print("%s Blocks data buffer is %d%% full" %(
                    "[" + time.strftime("%Y-%m-%d %H:%M:%S") + "]",
                    buffer_occ_ratio
                ))
                print("Is Bitcoin FIBRE running and listening to \"%s\"?"
                      %(self.blk_pipe.name))
                self.last_blk_warning = buffer_occ_ratio

            elif (buffer_occ_ratio >= 100):
                # Throw away old data until it fits
                self.blk_data_buffer_lock.acquire()
                throw_away_cnt = 0
                while (self.blk_data_buffer_data_cnt > MAX_BUFFER_SIZE):
                    blk_data = self.blk_data_buffer.pop(0)
                    throw_away_cnt += len(blk_data)
                    self.blk_data_buffer_data_cnt -= len(blk_data)

                self.blk_data_buffer_lock.release()

                # Keep track of the overall amount of data deleted
                self.blk_data_buffer_del_cnt += throw_away_cnt

                # Set to print buffer overflow message to console
                self.blk_ovflw_print_pending = True

            # Flag data availability if buffer was empty previously
            if (blk_data_buffer_occ == 0):
                self.blk_data_available.set()

            # Global and monotonic blocks data counter
            self.blk_data_cnt += len(packet.payload)

        # Is it time to update data stats?
        current_t = time.time()
        if (current_t > self.next_stats):
            self.update_stats()
            # Schedule next
            self.next_stats = current_t + self.stats_period

        # Is it time to print data stats?
        if (current_t > self.next_print):
            self.log_stats()
            # Schedule next print
            self.next_print = current_t + self.print_period

    def update_stats(self):
        """Update rate statistics"""

        # Update rate stats
        t_tstamp = time.time()
        self.api_byte_rate.update(self.api_data_cnt, t_tstamp)
        self.blk_byte_rate.update(self.blk_data_cnt, t_tstamp)

    def log_stats(self):
        """Print rate/rx data statistics"""
        if self.api_ovflw_print_pending:
            print("%s WARNING: API data buffer full - lost %d bytes" %(
                "[" + time.strftime("%Y-%m-%d %H:%M:%S") + "]",
                self.api_data_buffer_del_cnt
            ))
            print("NOTE: If not reading API data from \"%s\", "
                  %(self.api_pipe.name) + "run with \"--no-api\"" )
            self.api_data_buffer_del_cnt = 0
            self.api_ovflw_print_pending = False

        elif self.blk_ovflw_print_pending:
            print("%s WARNING: Blocks data buffer full - lost %d bytes" %(
                "[" + time.strftime("%Y-%m-%d %H:%M:%S") + "]",
                self.blk_data_buffer_del_cnt
            ))
            print("NOTE: If not reading blocks data from \"%s\", "
                  %(self.blk_pipe.name) + "run with \"--no-blocks\"" )
            self.blk_data_buffer_del_cnt = 0
            self.blk_ovflw_print_pending = False

        # Print data counts with timestamp
        dt_tstamp  = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        if (self.disable_api):
            api_rx_print = "Disabled"
        else:
            if (self.api_byte_rate.value is not None):
                api_rx_print = "%8s (%3.1f kB/sec)" %(
                    convert_size(self.api_data_cnt),
                    self.api_byte_rate.value / 1e3)
            else:
                api_rx_print = "%8s" %(convert_size(self.api_data_cnt))

        if (self.disable_blk):
            blk_rx_print = "Disabled"
        else:
            if (self.blk_byte_rate.value is not None):
                blk_rx_print = "%8s (%3.1f kB/sec)" %(
                    convert_size(self.blk_data_cnt),
                    self.blk_byte_rate.value / 1e3)
            else:
                blk_rx_print = "%8s" %(convert_size(self.blk_data_cnt))

        print("--------------------------------------------------------------------------------")
        print('[%s] Rx Data\t%6s: %s\n' %(dt_tstamp, "Blocks", blk_rx_print) +
                  '%29s\t%6s: %s' %("", "API", api_rx_print))
        print("--------------------------------------------------------------------------------")

    def handle_msg(self, msg_pmt):
        """Process incoming GNU Radio PDU containing a Blocksat Packet

        Args:
            msg_pmt : the message containing a Blocksat Packet
        """

        # Get metadata and information from message
        meta = pmt.to_python(pmt.car(msg_pmt))
        msg  = pmt.cdr(msg_pmt)

        # Validate (message should be a vector of u8, i.e. bytes)
        if not pmt.is_u8vector(msg):
            print("[ERROR] Received invalid message type.\n")
            return

        # Convert incoming packet to a bytes string
        raw_pkt = bytes(pmt.u8vector_elements(msg))

        if (self.protocol_version > 1):
            # Parse Blocksat Packet
            packet = BlocksatPacket(raw_pkt)

            # Process the parsed packet
            self.handle_pkt(packet)
        else:
            # Write raw data directly to named pipe
            self.blk_pipe.write(raw_pkt)

    def api_consumer(self):
        """Consumes data from the API data buffer and outputs to pipe
        """
        api_data_buffer_occ = 0

        while self.api_consumer_en:
            # Wait for data if the buffer is empty
            if (api_data_buffer_occ == 0):
                self.api_data_available.wait()
                self.api_data_available.clear()

            self.api_data_buffer_lock.acquire()

            # Pop data
            api_data = self.api_data_buffer.pop(0)

            # Keep track of data count in bytes
            self.api_data_buffer_data_cnt -= len(api_data)

            # Keep track of the occupancy in terms of data blobs
            api_data_buffer_occ = len(self.api_data_buffer)

            self.api_data_buffer_lock.release()

            # Prepare output struct header w/ delimiter and length
            out_data_header = struct.pack(OUT_DATA_HEADER_FORMAT,
                                          OUT_DATA_DELIMITER,
                                          len(api_data))

            # Output structure to pipe
            self.api_pipe.write(out_data_header + api_data)

    def blk_consumer(self):
        """Consumes data from the blocks data buffer and outputs to pipe
        """

        blk_data_buffer_occ = 0

        while self.blk_consumer_en:
            # Wait for data if the buffer is empty
            if (blk_data_buffer_occ == 0):
                self.blk_data_available.wait()
                self.blk_data_available.clear()

            self.blk_data_buffer_lock.acquire()

            # Pop data
            blk_data = self.blk_data_buffer.pop(0)

            # Keep track of data count in bytes
            self.blk_data_buffer_data_cnt -= len(blk_data)

            # Keep track of the occupancy in terms of data blobs
            blk_data_buffer_occ = len(self.blk_data_buffer)

            self.blk_data_buffer_lock.release()

            # Output blocks data to pipe
            # NOTE: output data structure is only used for API data
            self.blk_pipe.write(blk_data)

