import sys
import time
import threading
import math

# Definitions
MAX_SNR_DB = 20

class Logger(threading.Thread):
    """Logger Class

    Periodically calls an instance-defined print routine

    Attributes:
        block_obj : The flowgraph object to be passed to the print function
        period    : Interval between prints
        print_fcn : The specific print function to be used every period
        lock      : A mutex used to synchronize prints across instances
        start_en  : Flag to start the thread in enabled state
        label_setter : Setter function for the variable used in a GUI label
    """
    def __init__(self, block_obj, period, print_fcn, lock, start_en=True,
                 label_setter=None):
        ''' Init Logger '''

        # Init daemon thread
        threading.Thread.__init__(self)
        self.daemon = True

        self.block_obj = block_obj
        self.period    = period
        self.log       = print_fcn
        self.lock      = lock
        self.enabled   = start_en
        self.label_setter = label_setter

    def run(self):

        next_print = time.time() + self.period

        while(True):

            # Spin until the logger is enabled
            while (not self.enabled):
                time.sleep(2)
                next_print = time.time() + self.period

            # Sleep until next scheduled log
            current_time = time.time()

            if (next_print > current_time):
                time.sleep((next_print - current_time))

            # Prevent module resolution issue during teardown of parent
            # processs (see https://stackoverflow.com/a/21844166)
            if (time is None):
                break

            # Schedule next print
            next_print = time.time() + self.period

            # Call Print
            self.lock.acquire()
            try:
                self.log(self.block_obj, self.label_setter)
            finally:
                self.lock.release()

    def enable(self):
        """Enable the logger """
        self.enabled = True

    def disable(self):
        """Disable the logger """
        self.enabled = False


def print_frame_sync(block_obj, label_setter):

    # Get data from the frame synchronizer block
    state            = block_obj.get_state()
    timing_indicator = block_obj.get_mag_pmf_peak()

    # Print
    sys.stdout.write("----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.write("[" + time.strftime("%Y-%m-%d %H:%M:%S") + "] ")
    sys.stdout.write("Frame Timing => ")

    if (state):
        state_str = "LOCKED"
    else:
        state_str = "SEARCHING"

    sys.stdout.write(state_str)

    # Set the state string in the label as well
    if (label_setter is not None):
        label_setter(state_str)

    sys.stdout.write("\t Timing Indicator: ")

    if (timing_indicator > 90.0/130):
        level = "STRONG"
    elif (timing_indicator > 30.0/130):
        level = "GOOD"
    else:
        level = "WEAK"

    sys.stdout.write(level)
    sys.stdout.write("\r\n")
    sys.stdout.write("----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.flush()


def print_snr(block_obj, label_setter):

    # Check frame sync state first
    state = block_obj["fs"].get_state()

    # Disable the MER measurement block when frame sync is locked. Re-enable
    # after unlocking.
    if (bool(state) and (block_obj["fs_locked"] == False)):
        block_obj["mer_meter"].disable()
        block_obj["fs_locked"] = True
    elif ((not bool(state)) and (block_obj["fs_locked"] == True)):
        block_obj["mer_meter"].enable()
        block_obj["fs_locked"] = False

    # If locked, get data-aided SNR measurement from phase recovery block. If
    # unlocked, get from the SNR meter block
    if (block_obj["fs_locked"]):
        snr_db = block_obj["phase_rec"].get_snr()
    else:
        snr_db = block_obj["mer_meter"].get_snr()

    # Print SNR with a bar indicator
    # (each "=" indicates 0.5 dB)
    sys.stdout.write("[" + time.strftime("%Y-%m-%d %H:%M:%S") + "] ")
    sys.stdout.write("SNR [")

    bar_pos = round(2*snr_db)

    for i in range(0, 2*MAX_SNR_DB):
        if (i <= bar_pos):
            sys.stdout.write("=")
        else:
            sys.stdout.write(" ")

    sys.stdout.write("] ")
    sys.stdout.write(str("{:2.4f}".format(snr_db)))
    sys.stdout.write(" dB\r\n")
    sys.stdout.flush()


def print_ber(block_obj, label_setter):

    # Get BER
    ber = block_obj.get_ber()

    # Print
    sys.stdout.write("----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.write("[" + time.strftime("%Y-%m-%d %H:%M:%S") + "] ")
    sys.stdout.write("Bit Error Rate: ")
    ber_str = str("{:.2E}".format(ber))
    sys.stdout.write(ber_str)
    sys.stdout.write("\r\n")
    sys.stdout.write("----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.flush()

    # Set the formatted ber in the label as well
    if (label_setter is not None):
        label_setter(ber_str)


def print_cfo(block_obj, label_setter):
    global sample_rate

    norm_angular_freq = block_obj.get_frequency()

    norm_freq = norm_angular_freq / (2.0 * math.pi)

    cfo = norm_freq * sample_rate

    # Print
    sys.stdout.write("----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.write("[" + time.strftime("%Y-%m-%d %H:%M:%S") + "] ")
    sys.stdout.write("Carrier Frequency Offset: ")

    cfo_str = str("{:2.4f}".format(cfo/1e3)) + " kHz "
    sys.stdout.write(cfo_str)

    sys.stdout.write("\n----------------------------------------")
    sys.stdout.write("----------------------------------------\n")
    sys.stdout.flush()

    # Set the frequency offset in the label as well
    if (label_setter is not None):
        label_setter(cfo_str)


class rx_logger():
    """Rx Logger

    Initiates threads to log useful receiver metrics into the console.

    """

    def __init__(self, samp_rate, snr_meter_obj, snr_log_period,
                 frame_synchronizer_obj, frame_sync_log_period,
                 decoder_obj, ber_log_period, cfo_rec_obj,
                 cfo_log_period, phase_rec_obj, enabled_start,
                 fs_label_setter, ber_label_setter, fo_label_setter):

        global sample_rate
        sample_rate = samp_rate

        # Use mutex to coordinate logs
        lock = threading.Lock()

        # Declare loggers

        if (snr_meter_obj and phase_rec_obj and frame_synchronizer_obj
            and snr_log_period > 0):
            # For the SNR, pass along three different top-level objects
            snr_objects = {
                "mer_meter" : snr_meter_obj,
                "phase_rec" : phase_rec_obj,
                "fs"        : frame_synchronizer_obj,
                "fs_locked" : False
            }
            self.snr_logger = Logger(snr_objects,
                                     snr_log_period,
                                     print_snr,
                                     lock,
                                     enabled_start)

        if (frame_synchronizer_obj and (frame_sync_log_period > 0)):
            self.frame_sync_logger = Logger(frame_synchronizer_obj,
                                            frame_sync_log_period,
                                            print_frame_sync,
                                            lock,
                                            enabled_start,
                                            fs_label_setter)

        if (decoder_obj and (ber_log_period > 0)):
            self.ber_logger = Logger(decoder_obj,
                                     ber_log_period,
                                     print_ber,
                                     lock,
                                     enabled_start,
                                     ber_label_setter)

        if (cfo_rec_obj and (cfo_log_period > 0)):
            self.cfo_logger = Logger(cfo_rec_obj,
                                     cfo_log_period,
                                     print_cfo,
                                     lock,
                                     enabled_start,
                                     fo_label_setter)

        # Start loggers

        if (snr_meter_obj and (snr_log_period > 0)):
            self.snr_logger.start()
            time.sleep(0.01)

        if (frame_synchronizer_obj and (frame_sync_log_period > 0)):
            self.frame_sync_logger.start()
            time.sleep(0.01)

        if (decoder_obj and (ber_log_period > 0)):
            self.ber_logger.start()
            time.sleep(0.01)

        if (cfo_rec_obj and (cfo_log_period > 0)):
            self.cfo_logger.start()
            time.sleep(0.01)

    def enable(self):
        """Enable the Rx loggers """
        if (hasattr(self, 'snr_logger')):
            self.snr_logger.enable()

        if (hasattr(self, 'frame_sync_logger')):
            self.frame_sync_logger.enable()

        if (hasattr(self, 'ber_logger')):
            self.ber_logger.enable()

        if (hasattr(self, 'cfo_logger')):
            self.cfo_logger.enable()

    def disable(self):
        """Disable the Rx loggers """
        if (hasattr(self, 'snr_logger')):
            self.snr_logger.disable()

        if (hasattr(self, 'frame_sync_logger')):
            self.frame_sync_logger.disable()

        if (hasattr(self, 'ber_logger')):
            self.ber_logger.disable()

        if (hasattr(self, 'cfo_logger')):
            self.cfo_logger.disable()

