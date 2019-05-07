import sys
import os
import threading
import time
import datetime
import math
from numpy import arange

# Definitions
min_cfo_rec_alpha_scan_mode = 0.1
max_n_scan_attemps = 2

class tuning_control(threading.Thread):
  """Tuning Control

  Runtime control of the RF center frequency or RF frequency scanning helper.

  Attributes:
    samp_rate         :  Samples rate
    sps               :  Samples-per-symbol
    M                 :  Constellation order
    scan_mode         :  Activates scan mode
    n_steps           :  Number iterations used to sweep the scan range
    cfo_poll_interval :  CFO polling interval in runtime control mode
    scan_interval     :  Interval between each scanned frequency in scan mode
    frame_sync_obj    :  Object of the top-level frame synchronizer module
    cfo_rec_obj       :  Object of the top-level frequency (CFO) recovery module
    logger_obj        :  Object of the top-level Rx Logger module
    freq_label_obj    :  Object of the top-level GUI label (if any)
    rf_freq_getter    :  Getter for the top-level RF center frequency
    rf_freq_setter    :  Setter for the top-level RF center frequency
  """

  def __init__(self, samp_rate, sps, M, scan_mode, n_steps, cfo_poll_interval,
               scan_interval, frame_sync_obj, cfo_rec_obj, logger_obj,
               freq_label_obj, rf_freq_getter, rf_freq_setter):

    self.samp_rate   = samp_rate
    self.sps         = sps
    self.const_order = M
    self.scan_mode   = scan_mode
    self.rf_freq     = rf_freq_getter()

    self.frame_sync_obj = frame_sync_obj
    self.cfo_rec_obj    = cfo_rec_obj
    self.rx_logger      = logger_obj

    # Detect which CFO recovery scheme is adopted
    if (self.cfo_rec_obj is None):
      raise ValueError("Not freq. recovery module")

    # Interaction with the SDR
    self.get_hw_freq    = rf_freq_getter
    self.set_hw_freq    = rf_freq_setter

    self.cfo_poll_interval   = cfo_poll_interval
    self.scan_interval       = scan_interval

    # Launch either the frequency scan or CFO control thread
    if (self.scan_mode):
      if (self.const_order == 4):
        self.freq_step = (self.samp_rate / 4)
      elif (self.const_order == 2):
        self.freq_step = (self.samp_rate / 2)
      else:
        raise ValueError("Constellation order %d is not supported" %(M))
      self.scan_range   = n_steps * self.freq_step
      self.curr_freq    = self.rf_freq
      self.freqs        = self.get_freq_scan_array(self.rf_freq,
                                                   self.scan_range,
                                                   n_steps)
      self.n_scan_steps = n_steps

      self.scan_thread = threading.Thread(target=self.freq_scanner,
                                          args=())
      self.scan_thread.daemon = True
      self.scan_thread.start()

    else:
      self.cfo_ctrl_thread = threading.Thread(target=self.cfo_controller,
                                         args=())
      self.cfo_ctrl_thread.daemon = True
      self.cfo_ctrl_thread.start()

  def freq_scanner(self):
    """ Frequency scan mode """

    # A scan is only complete once one of the tested frequencies can acquire
    # frame lock:
    frame_lock_acquired = False
    # If scan finishes and the signal is not found, try again until a maximum
    # number of attempts:
    n_attempts = 0

    # Sleep shortly just to allow the initialization logs to clear
    time.sleep(3)

    print("\n========================= Scan Mode =========================")
    print("Sweep %.2f MHz from %f MHz to %f MHz, in steps of %.2f kHz" %(
      self.scan_range/1e6, min(self.freqs)/1e6, max(self.freqs)/1e6,
      self.freq_step/1e3))
    self.print_scan_duration()

    # Change to the starting frequency
    self.set_scan_start_freq()

    # Start schedule of wake-ups
    next_work = time.time() + self.scan_interval

    while self.scan_mode:

      sys.stdout.write("[Scanning %6.3f MHz] " %(self.curr_freq/1e6))
      sys.stdout.flush()

      # Sleep and show progress until next scheduled work
      self.sleep_with_progress_dots(next_work, self.scan_interval)

      # Schedule next work
      next_work = time.time() + self.scan_interval

      # Each iteration evaluates the timing metric that can be
      # obtained with the chosen RF center frequency
      self.check_timing_metric()

      # Check if the current frequency could achieve frame lock:
      if (self.frame_sync_obj.get_state()):
        frame_lock_acquired = True

      # Stop when frame lock is achieved or all frequencies are scanned
      retry_start = False
      if (self.freq_idx == self.n_scan_steps or frame_lock_acquired):

        # Best frequency:
        max_idx     = self.metric.index(max(self.metric))
        best_freq   = self.freqs[max_idx]
        best_metric = self.metric[max_idx]

        self.print_scan_results(best_freq, best_metric, frame_lock_acquired)

        # Exit immediatelly if frame lock was achieved
        if (frame_lock_acquired):
          self.scan_mode = False
        else:
          print("Error: signal was not detected in any of the frequencies.")

          if (n_attempts == max_n_scan_attemps):
            print("Maximum number of scan attempts reached.")
            print("Check your hardware and/or try a wider scan range.")

            # Exit scan
            self.scan_mode = False

            print('\033[91m' + "ERROR:" + '\033[0m' + " Signal not found.")

            os._exit(1)
          else:
            # Flag that we are restarting the scan for a retry
            retry_start = True

            # Double the scan interval
            self.scan_interval = 2 * self.scan_interval

            # Go back to starting frequency
            self.set_scan_start_freq()

            if (n_attempts == 0):
              print("A second scan will be attempted.")
            elif (n_attempts == (max_n_scan_attemps - 1)):
              print("Scan will be attempted one last time.")
            else:
              print("Another scan round will be attempted.")

            print("Observation of each frequency will now take %.2f sec" %(
              self.scan_interval))
            self.print_scan_duration()

          n_attempts += 1

      # Proceed to analyze the next frequency
      #
      # NOTE: If this is after reseting the frequency for another scan attempt
      # (on "retry_start""), don't move to the next frequency yet, as the
      # initial scan frequency was just reset and is not tested yet.
      if (self.scan_mode and not retry_start):
        self.scan_next_freq()

    # ------- Scan exit routine -------

    # Then exit:
    self.exit_scan_mode(best_freq)

  def cfo_controller(self):
    """ Interaction with Runtime CFO controller """

    # CFO threshold
    #
    # For the FFT method, the theoretical limit is samp_rate/8 for QPSK and
    # samp_rate/4 for BPSK. We use a slightly lower value.
    if (self.const_order == 4):
      self.abs_cfo_threshold = 0.95 * (self.samp_rate / 8)
    elif (self.const_order == 2):
      self.abs_cfo_threshold = 0.95 * (self.samp_rate / 4)
    else:
      raise ValueError("Constellation order %d is not supported" %(
        self.const_order))

    next_work = time.time() + self.cfo_poll_interval

    while True:

      # Sleep until next scheduled work
      current_time = time.time()

      if (next_work > current_time):
        time.sleep(next_work - current_time)

      # Prevent module resolution issue during teardown of parent
      # processs (see https://stackoverflow.com/a/21844166)
      if (time is None):
        break

      # Schedule next work
      next_work = time.time() + self.cfo_poll_interval

      # Check the current target RF center frequency according to the runtime
      # CFO controller module and the RF center frequency that is currently
      # being used in the SDR board
      current_rf_center_freq = self.get_hw_freq()

      # Also check whether the frame recovery algorithm is locked, to ensure a
      # frequency shift is only carried while the system is running already and
      # prevent shifts during the initialization.
      frame_sync_state = self.frame_sync_obj.get_state()

      # Update the radio RF center frequency if a new one is requested
      current_cfo_est       = self.get_cfo_estimation()
      update_freq           = (abs(current_cfo_est) > self.abs_cfo_threshold)
      target_rf_center_freq = current_rf_center_freq + current_cfo_est

      if (update_freq and frame_sync_state != 0):

        print("\n--- Carrier Tracking Mechanism ---");
        print("[" + datetime.datetime.now().strftime(
          "%A, %d. %B %Y %I:%M%p" + "]"))
        print("RF center frequency update");
        print("From:\t %d Hz (%6.2f MHz)" %(
          current_rf_center_freq, current_rf_center_freq/1e6));
        print("To:\t %d Hz (%6.2f MHz)" %(
          target_rf_center_freq, target_rf_center_freq/1e6));
        print("Frequency shifted by %.2f kHz" %(
          (target_rf_center_freq - current_rf_center_freq)/1e3
        ))
        print("----------------------------------\n");

        # Update the RF frequency
        self.set_freq(target_rf_center_freq)

        # After an udpdate, sleep for a few more seconds to prevent a second
        # polling of CFO before locking
        next_work = next_work + 4

  def check_timing_metric(self):

    # Get the timing metric from the frame synchronizer
    timing_metric = self.frame_sync_obj.get_mag_pmf_peak()

    self.metric.append(timing_metric)

    sys.stdout.write(" Signal Strength: %.4f\n" %(timing_metric))
    sys.stdout.flush()

  def sleep_with_progress_dots(self, wake_up_time, sleep_interval):
    """Sleep and print progress in terms of dots

    Args:
        wake_up_time   : time scheduled for the task wake-up
        sleep_interval : target sleep interval
    """

    n_complete_dots  = 10
    printed_progress = 0
    current_time     = time.time()

    # Print dots until the wake-up time is reached
    while (wake_up_time > current_time):

      progress        = 1 - ((wake_up_time - current_time) / sleep_interval)
      n_progress_dots = int(round(progress * n_complete_dots))

      for i in range(printed_progress, n_progress_dots):
        sys.stdout.write('..')
        printed_progress = n_progress_dots
        sys.stdout.flush()

      time.sleep(1)
      current_time = time.time()

    # Make sure the number of printed dots is always the same
    missing_dots = n_complete_dots - printed_progress

    for i in range(0, missing_dots):
      sys.stdout.write('..')
      sys.stdout.flush()

  def exit_scan_mode(self, target_freq):
    """Routine for exiting the scan mode and turning into normal mode

    Args:
        target_freq    : the target RF center frequency obtained through scan

    """

    # Launch runtime control interaction thread:
    self.cfo_ctrl_thread = threading.Thread(target=self.cfo_controller,
                                            args=())
    self.cfo_ctrl_thread.daemon = True

    print("Starting receiver...")
    print("----------------------------------------")
    self.cfo_ctrl_thread.start()

    # Enable the logger module
    self.rx_logger.enable()

  def get_freq_scan_array(self, center_freq, scan_range, n_iter):
    """Prepare the array of frequencies to scan

    Returns the vector of frequencies sorted such that the first frequency to be
    scanned is the nominal center frequency and, then, the adjcent frequencies
    (w/ positive and negative offsets) are unfolded. This ensures that the
    extreme case of bigger offsets is tested later and the frequencies with
    lower error are tested first.

    Args:
        center_freq : The nominal RF center frequency (starting point)
        scan_range  : Range of frequencies to scan
        n_iter      : Number of scan iterations

    """

    freq_step  = scan_range/n_iter
    freq_array = []

    for i in range(0, n_iter + 1):

      i_abs_offset = i/2 + (i % 2)

      if (i % 2 == 0):
        freq = center_freq + (i_abs_offset * freq_step)
      else:
        freq = center_freq - (i_abs_offset * freq_step)

      freq_array.append(freq)

    return freq_array

  def set_scan_start_freq(self):
    """Set the starting point for the scan"""

    self.metric    = []
    self.freq_idx  = 0;
    self.curr_freq = self.freqs[0]
    self.set_freq(self.freqs[0])

  def scan_next_freq(self):

    # Move to a new frequency from the list
    self.freq_idx += 1;
    self.curr_freq = self.freqs[self.freq_idx]

    # Set it in the top-level variable (used by the RF board) and inform the CFO
    # controller:
    self.set_freq(self.curr_freq)

  def set_freq(self, freq):
    """Wrapper for setting the frequency in HW as int """

    self.set_hw_freq(int(round(freq)))
    self.cfo_rec_obj.reset()

  def get_cfo_estimation(self):
    """Get the CFO corresponding to the current freq. correction """

    norm_angular_freq = self.cfo_rec_obj.get_frequency()
    norm_freq         = norm_angular_freq / (2.0 * math.pi)
    return (norm_freq * self.samp_rate)

  def print_scan_duration(self):
    """Compute and print the maximum duration expected for the complete scan """

    n_scan_steps  = self.n_scan_steps
    step_interval = self.scan_interval

    duration_sec  = step_interval * n_scan_steps
    d_min, d_sec  = divmod(duration_sec, 60)

    sys.stdout.write("The scan can take up to ")
    if (d_min > 0):
      sys.stdout.write("{:>d} min and {:2d} sec.\n\n".format(int(d_min),
                                                           int(d_sec)))
    else:
      sys.stdout.write("{:2d} sec.\n\n".format(int(d_sec)))

    sys.stdout.flush()

  def print_scan_results(self, best_freq, best_metric, sig_found):
    """Print Results"""

    delta_from_nominal = best_freq - self.rf_freq

    sorted_idx = [i[0] for i in sorted(enumerate(self.metric),
                                           key=lambda x:x[1],
                                           reverse=True)]

    print("============================================================")
    if (sig_found):
      print("Blockstream Satellite signal was found\n--")
    else:
      print("Frequency scan complete\n--")


    print("_%21s___%30s" %(
      "_____________________", "______________________________"))

    if (sig_found):
      print("| %21s | %-10d Hz (%6.2f MHz) |" %(
        "Frequency", best_freq, best_freq/1e6))
    else:
      print("| %21s | %-10d Hz (%6.2f MHz) |" %(
        "Best frequency", best_freq, best_freq/1e6))
    print("| %21s | %-7.4f %19s |" %(
      "Average timing metric", best_metric, ""))
    print("| %21s | %-27s |" %(
      "Offset from nominal", "%d Hz" %delta_from_nominal +
      "(%.2f kHz)" %(delta_from_nominal/1e3)))
    print("|%21s___%29s|" %(
      "_____________________", "_____________________________"))

    print("\nRanking of scanned frequencies (first is the best):\n")
    print("%-4s | %-27s | %-13s" %("Rank", "Frequency", "Timing Metric"))
    print("%-4s | %-27s | %-13s" %(
      "____", "___________________________", "_____________"))
    for i, metric_idx in enumerate(sorted_idx):
      print("%4d | %10d Hz (%6.2f MHz) | %-5.4f" %(
        i, self.freqs[metric_idx], self.freqs[metric_idx]/1e6,
        self.metric[metric_idx]))
    print("============================================================\n")

