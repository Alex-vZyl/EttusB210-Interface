// ================================================================================================================================================================================ //
//  Includes.                                                                                                                                                                       //
// ================================================================================================================================================================================ //

#include "wavetable.hpp"
#include <string>									// String handling.
#include <vector>									// C++ vectors.
#include <uhd/exception.hpp>						// -- Ettus UHD.
#include <uhd/types/tune_request.hpp>				// \/
#include <uhd/usrp/multi_usrp.hpp>					// \/
#include <uhd/utils/safe_main.hpp>					// \/
#include <uhd/utils/static.hpp>						// \/
#include <uhd/utils/thread.hpp>						// --
#include <boost/algorithm/string.hpp>				// -- Boost.
#include <boost/filesystem.hpp>						// \/ 
#include <boost/format.hpp>							// \/
#include <boost/math/special_functions/round.hpp>	// \/ 
#include <boost/program_options.hpp>				// \/ 
#include <boost/thread/thread.hpp>					// --
#include <csignal>
#include <fstream>
#include <functional>
#include <iostream>
#include "../External/Misc/ConsoleColor.h"
#include <numbers>
#include <stdio.h>
#include "Interface.h"								//  Class running the app.
#include <chrono>

// ================================================================================================================================================================================ //
//  SDR Setup.                                                                                                                                                                       //
// ================================================================================================================================================================================ //

void Interface::setupSDR() 
{
    m_status = "Setting up SDR...";
	clear();
	systemInfo();
    std::cout << "\n";

    // ------------------------------------- //
    //  C R E A T E   U S R P   D E V I C E  //
    // ------------------------------------- //

    // Create a usrp device.
    std::cout << std::endl;
    std::cout << boost::format("Creating the transmit usrp device with: %s...") % tx_args
        << std::endl;
    uhd::usrp::multi_usrp::sptr tx_usrp = uhd::usrp::multi_usrp::make(tx_args);
    std::cout << std::endl;
    std::cout << boost::format("Creating the receive usrp device with: %s...") % rx_args
        << std::endl;
    uhd::usrp::multi_usrp::sptr rx_usrp = uhd::usrp::multi_usrp::make(rx_args);
     
    // --------------------- //
    //  S U B D E V I C E S  //
    // --------------------- //

    // Always select the subdevice first, the channel mapping affects the other settings.
    if (tx_subdev.size()) { tx_usrp->set_tx_subdev_spec(tx_subdev); }
    if (rx_subdev.size()) { rx_usrp->set_rx_subdev_spec(rx_subdev); }

    //===================//
    //  Print new frame  //
    //===================//
    clear();
    systemInfo();
    std::cout << blue << "\n\n[SDR] [INFO]: " << white << "TX & RX USRP devices created.\n";

    // ----------------- //
    //  C H A N N E L S  //
    // ----------------- //

    // Detect which channels to use.
    std::vector<std::string> tx_channel_strings;
    std::vector<size_t> tx_channel_nums;
    boost::split(tx_channel_strings, tx_channels, boost::is_any_of("\"',"));
    for (size_t ch = 0; ch < tx_channel_strings.size(); ch++) {
        size_t chan = std::stoi(tx_channel_strings[ch]);
        if (chan >= tx_usrp->get_tx_num_channels()) {
            throw std::runtime_error("Invalid TX channel(s) specified.");
        }
        else
            tx_channel_nums.push_back(std::stoi(tx_channel_strings[ch]));
    }
    std::vector<std::string> rx_channel_strings;
    std::vector<size_t> rx_channel_nums;
    boost::split(rx_channel_strings, rx_channels, boost::is_any_of("\"',"));
    for (size_t ch = 0; ch < rx_channel_strings.size(); ch++) {
        size_t chan = std::stoi(rx_channel_strings[ch]);
        if (chan >= rx_usrp->get_rx_num_channels()) {
            throw std::runtime_error("Invalid RX channel(s) specified.");
        }
        else
            rx_channel_nums.push_back(std::stoi(rx_channel_strings[ch]));
    }

    // ------------------------- //
    //  C L O C K   S O U R C E  //
    // ------------------------- //
    
    tx_usrp->set_clock_source(ref);
    rx_usrp->set_clock_source(ref);

    /*std::cout << boost::format("Using TX Device: %s") % tx_usrp->get_pp_string()
        << std::endl;
    std::cout << boost::format("Using RX Device: %s") % rx_usrp->get_pp_string()
        << std::endl;*/

    // ---------------- //
    // S A M P L I N G  //
    // ---------------- //

    std::cout << boost::format("Setting TX Rate: %f Msps...") % (tx_rate / 1e6)
        << std::endl;
    tx_usrp->set_tx_rate(tx_rate);
    std::cout << boost::format("Actual TX Rate: %f Msps...")
        % (tx_usrp->get_tx_rate() / 1e6)
        << std::endl
        << std::endl;

    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rx_rate / 1e6)
        << std::endl;
    rx_usrp->set_rx_rate(rx_rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...")
        % (rx_usrp->get_rx_rate() / 1e6)
        << std::endl
        << std::endl;

    //===================//
    //  Print new frame  //
    //===================//
    clear();
    systemInfo();
    std::cout << blue << "\n\n[SDR] [INFO]: " << white << "TX & RX USRP devices created.\n";
    std::cout << blue << "[SDR] [INFO]: " << white << "Sampling frequencies set up.\n";


    //for (size_t ch = 0; ch < tx_channel_nums.size(); ch++) {
    //    size_t channel = tx_channel_nums[ch];
    //    if (tx_channel_nums.size() > 1) {
    //        std::cout << "Configuring TX Channel " << channel << std::endl;
    //    }
    //    std::cout << boost::format("Setting TX Freq: %f MHz...") % (tx_freq / 1e6)
    //        << std::endl;
    //    uhd::tune_request_t tx_tune_request(tx_freq);
    //    if (vm.count("tx-int-n"))
    //        tx_tune_request.args = uhd::device_addr_t("mode_n=integer");
    //    tx_usrp->set_tx_freq(tx_tune_request, channel);
    //    std::cout << boost::format("Actual TX Freq: %f MHz...")
    //        % (tx_usrp->get_tx_freq(channel) / 1e6)
    //        << std::endl
    //        << std::endl;

    //    // set the rf gain
    //    if (vm.count("tx-gain")) {
    //        std::cout << boost::format("Setting TX Gain: %f dB...") % tx_gain
    //            << std::endl;
    //        tx_usrp->set_tx_gain(tx_gain, channel);
    //        std::cout << boost::format("Actual TX Gain: %f dB...")
    //            % tx_usrp->get_tx_gain(channel)
    //            << std::endl
    //            << std::endl;
    //    }

    //    // set the analog frontend filter bandwidth
    //    if (vm.count("tx-bw")) {
    //        std::cout << boost::format("Setting TX Bandwidth: %f MHz...") % tx_bw
    //            << std::endl;
    //        tx_usrp->set_tx_bandwidth(tx_bw, channel);
    //        std::cout << boost::format("Actual TX Bandwidth: %f MHz...")
    //            % tx_usrp->get_tx_bandwidth(channel)
    //            << std::endl
    //            << std::endl;
    //    }

    //    // set the antenna
    //    if (vm.count("tx-ant"))
    //        tx_usrp->set_tx_antenna(tx_ant, channel);
    //}

    //for (size_t ch = 0; ch < rx_channel_nums.size(); ch++) {
    //    size_t channel = rx_channel_nums[ch];
    //    if (rx_channel_nums.size() > 1) {
    //        std::cout << "Configuring RX Channel " << channel << std::endl;
    //    }

    //    std::cout << boost::format("Setting RX Freq: %f MHz...") % (rx_freq / 1e6)
    //        << std::endl;
    //    uhd::tune_request_t rx_tune_request(rx_freq);
    //    if (vm.count("rx-int-n"))
    //        rx_tune_request.args = uhd::device_addr_t("mode_n=integer");
    //    rx_usrp->set_rx_freq(rx_tune_request, channel);
    //    std::cout << boost::format("Actual RX Freq: %f MHz...")
    //        % (rx_usrp->get_rx_freq(channel) / 1e6)
    //        << std::endl
    //        << std::endl;

    //    // set the receive rf gain
    //    if (vm.count("rx-gain")) {
    //        std::cout << boost::format("Setting RX Gain: %f dB...") % rx_gain
    //            << std::endl;
    //        rx_usrp->set_rx_gain(rx_gain, channel);
    //        std::cout << boost::format("Actual RX Gain: %f dB...")
    //            % rx_usrp->get_rx_gain(channel)
    //            << std::endl
    //            << std::endl;
    //    }

    //    // set the receive analog frontend filter bandwidth
    //    if (vm.count("rx-bw")) {
    //        std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (rx_bw / 1e6)
    //            << std::endl;
    //        rx_usrp->set_rx_bandwidth(rx_bw, channel);
    //        std::cout << boost::format("Actual RX Bandwidth: %f MHz...")
    //            % (rx_usrp->get_rx_bandwidth(channel) / 1e6)
    //            << std::endl
    //            << std::endl;
    //    }

    //    // set the receive antenna
    //    if (vm.count("rx-ant"))
    //        rx_usrp->set_rx_antenna(rx_ant, channel);
    //}

    //// for the const wave, set the wave freq for small samples per period
    //if (wave_freq == 0 and wave_type == "CONST") {
    //    wave_freq = tx_usrp->get_tx_rate() / 2;
    //}

    //// error when the waveform is not possible to generate
    //if (std::abs(wave_freq) > tx_usrp->get_tx_rate() / 2) {
    //    throw std::runtime_error("wave freq out of Nyquist zone");
    //}
    //if (tx_usrp->get_tx_rate() / std::abs(wave_freq) > wave_table_len / 2) {
    //    throw std::runtime_error("wave freq too small for table");
    //}

    //// pre-compute the waveform values
    //const wave_table_class wave_table(wave_type, ampl);
    //const size_t step =
    //    boost::math::iround(wave_freq / tx_usrp->get_tx_rate() * wave_table_len);
    //size_t index = 0;

    //// create a transmit streamer
    //// linearly map channels (index0 = channel0, index1 = channel1, ...)
    //uhd::stream_args_t stream_args("fc32", otw);
    //stream_args.channels = tx_channel_nums;
    //uhd::tx_streamer::sptr tx_stream = tx_usrp->get_tx_stream(stream_args);

    //// allocate a buffer which we re-use for each channel
    //if (spb == 0)
    //    spb = tx_stream->get_max_num_samps() * 10;
    //std::vector<std::complex<float>> buff(spb);
    //int num_channels = tx_channel_nums.size();

    //// setup the metadata flags
    //uhd::tx_metadata_t md;
    //md.start_of_burst = true;
    //md.end_of_burst = false;
    //md.has_time_spec = true;
    //md.time_spec = uhd::time_spec_t(0.5); // give us 0.5 seconds to fill the tx buffers

    //// Check Ref and LO Lock detect
    //std::vector<std::string> tx_sensor_names, rx_sensor_names;
    //tx_sensor_names = tx_usrp->get_tx_sensor_names(0);
    //if (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "lo_locked")
    //    != tx_sensor_names.end()) {
    //    uhd::sensor_value_t lo_locked = tx_usrp->get_tx_sensor("lo_locked", 0);
    //    std::cout << boost::format("Checking TX: %s ...") % lo_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(lo_locked.to_bool());
    //}
    //rx_sensor_names = rx_usrp->get_rx_sensor_names(0);
    //if (std::find(rx_sensor_names.begin(), rx_sensor_names.end(), "lo_locked")
    //    != rx_sensor_names.end()) {
    //    uhd::sensor_value_t lo_locked = rx_usrp->get_rx_sensor("lo_locked", 0);
    //    std::cout << boost::format("Checking RX: %s ...") % lo_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(lo_locked.to_bool());
    //}

    //tx_sensor_names = tx_usrp->get_mboard_sensor_names(0);
    //if ((ref == "mimo")
    //    and (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "mimo_locked")
    //        != tx_sensor_names.end())) {
    //    uhd::sensor_value_t mimo_locked = tx_usrp->get_mboard_sensor("mimo_locked", 0);
    //    std::cout << boost::format("Checking TX: %s ...") % mimo_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(mimo_locked.to_bool());
    //}
    //if ((ref == "external")
    //    and (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "ref_locked")
    //        != tx_sensor_names.end())) {
    //    uhd::sensor_value_t ref_locked = tx_usrp->get_mboard_sensor("ref_locked", 0);
    //    std::cout << boost::format("Checking TX: %s ...") % ref_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(ref_locked.to_bool());
    //}

    //rx_sensor_names = rx_usrp->get_mboard_sensor_names(0);
    //if ((ref == "mimo")
    //    and (std::find(rx_sensor_names.begin(), rx_sensor_names.end(), "mimo_locked")
    //        != rx_sensor_names.end())) {
    //    uhd::sensor_value_t mimo_locked = rx_usrp->get_mboard_sensor("mimo_locked", 0);
    //    std::cout << boost::format("Checking RX: %s ...") % mimo_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(mimo_locked.to_bool());
    //}
    //if ((ref == "external")
    //    and (std::find(rx_sensor_names.begin(), rx_sensor_names.end(), "ref_locked")
    //        != rx_sensor_names.end())) {
    //    uhd::sensor_value_t ref_locked = rx_usrp->get_mboard_sensor("ref_locked", 0);
    //    std::cout << boost::format("Checking RX: %s ...") % ref_locked.to_pp_string()
    //        << std::endl;
    //    UHD_ASSERT_THROW(ref_locked.to_bool());
    //}
    //
    //// Transmit for 2 seconds.
    //std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    //// reset usrp time to prepare for transmit/receive
    //std::cout << boost::format("Setting device timestamp to 0...") << std::endl;
    //tx_usrp->set_time_now(uhd::time_spec_t(0.0));

    //// start transmit worker thread
    //boost::thread_group transmit_thread;
    //transmit_thread.create_thread(std::bind(
    //    &transmit_worker, buff, wave_table, tx_stream, md, step, index, num_channels));

    //// recv to file
    //if (type == "double")
    //    recv_to_file<std::complex<double>>(
    //        rx_usrp, "fc64", otw, file, spb, total_num_samps, settling, rx_channel_nums);
    //else if (type == "float")
    //    recv_to_file<std::complex<float>>(
    //        rx_usrp, "fc32", otw, file, spb, total_num_samps, settling, rx_channel_nums);
    //else if (type == "short")
    //    recv_to_file<std::complex<short>>(
    //        rx_usrp, "sc16", otw, file, spb, total_num_samps, settling, rx_channel_nums);
    //else {
    //    // clean up transmit worker
    //    stop_signal_called = true;
    //    transmit_thread.join_all();
    //    throw std::runtime_error("Unknown type " + type);
    //}

    //// clean up transmit worker
    //stop_signal_called = true;
    //transmit_thread.join_all();

    m_status = "Idle.";
}

// ================================================================================================================================================================================ //
//  File handling.	                                                                                                                                                                        //
// ================================================================================================================================================================================ //

std::string Interface::generate_out_filename(
    const std::string& base_fn, size_t n_names, size_t this_name) 
{
    if (n_names == 1) {
        return base_fn;
    }

    boost::filesystem::path base_fn_fp(base_fn);
    base_fn_fp.replace_extension(boost::filesystem::path(
        str(boost::format("%02d%s") % this_name % base_fn_fp.extension().string())));
    return base_fn_fp.string();
}

template <typename samp_type>
void Interface::recv_to_file(uhd::usrp::multi_usrp::sptr usrp,
    const std::string& cpu_format,
    const std::string& wire_format,
    const std::string& file,
    size_t samps_per_buff,
    int num_requested_samples,
    double settling_time,
    std::vector<size_t> rx_channel_nums)
{
    int num_total_samps = 0;
    // create a receive streamer
    uhd::stream_args_t stream_args(cpu_format, wire_format);
    stream_args.channels = rx_channel_nums;
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

    // Prepare buffers for received samples and metadata
    uhd::rx_metadata_t md;
    std::vector<std::vector<samp_type>> buffs(
        rx_channel_nums.size(), std::vector<samp_type>(samps_per_buff));
    // create a vector of pointers to point to each of the channel buffers
    std::vector<samp_type*> buff_ptrs;
    for (size_t i = 0; i < buffs.size(); i++) {
        buff_ptrs.push_back(&buffs[i].front());
    }

    // Create one ofstream object per channel
    // (use shared_ptr because ofstream is non-copyable)
    std::vector<std::shared_ptr<std::ofstream>> outfiles;
    for (size_t i = 0; i < buffs.size(); i++) {
        const std::string this_filename = generate_out_filename(file, buffs.size(), i);
        outfiles.push_back(std::shared_ptr<std::ofstream>(
            new std::ofstream(this_filename.c_str(), std::ofstream::binary)));
    }
    UHD_ASSERT_THROW(outfiles.size() == buffs.size());
    UHD_ASSERT_THROW(buffs.size() == rx_channel_nums.size());
    bool overflow_message = true;
    double timeout =
        settling_time + 0.1f; // expected settling time + padding for first recv

    // setup streaming
    uhd::stream_cmd_t stream_cmd((num_requested_samples == 0)
        ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS
        : uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = num_requested_samples;
    stream_cmd.stream_now = false;
    stream_cmd.time_spec = uhd::time_spec_t(settling_time);
    rx_stream->issue_stream_cmd(stream_cmd);

    while (not stop_signal_called
        and (num_requested_samples > num_total_samps or num_requested_samples == 0)) {
        size_t num_rx_samps = rx_stream->recv(buff_ptrs, samps_per_buff, md, timeout);
        timeout = 0.1f; // small timeout for subsequent recv

        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << boost::format("Timeout while streaming") << std::endl;
            break;
        }
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            if (overflow_message) {
                overflow_message = false;
                std::cerr
                    << boost::format(
                        "Got an overflow indication. Please consider the following:\n"
                        "  Your write medium must sustain a rate of %fMB/s.\n"
                        "  Dropped samples will not be written to the file.\n"
                        "  Please modify this example for your purposes.\n"
                        "  This message will not appear again.\n")
                    % (usrp->get_rx_rate() * sizeof(samp_type) / 1e6);
            }
            continue;
        }
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            throw std::runtime_error(
                str(boost::format("Receiver error %s") % md.strerror()));
        }

        num_total_samps += num_rx_samps;

        for (size_t i = 0; i < outfiles.size(); i++) {
            outfiles[i]->write(
                (const char*)buff_ptrs[i], num_rx_samps * sizeof(samp_type));
        }
    }

    // Shut down receiver
    stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    rx_stream->issue_stream_cmd(stream_cmd);

    // Close files
    for (size_t i = 0; i < outfiles.size(); i++) {
        outfiles[i]->close();
    }
}

// ================================================================================================================================================================================ //
//  Workers.	                                                                                                                                                                        //
// ================================================================================================================================================================================ //

void Interface::sig_int_handler(int)
{
    stop_signal_called = true;
}

void transmit_worker(std::vector<std::complex<float>> buff,
    wave_table_class wave_table,
    uhd::tx_streamer::sptr tx_streamer,
    uhd::tx_metadata_t metadata,
    size_t step,
    size_t index,
    int num_channels)
{
    std::vector<std::complex<float>*> buffs(num_channels, &buff.front());

    // send data until the signal handler gets called
    while (not stop_signal_called) {
        // fill the buffer with the waveform
        for (size_t n = 0; n < buff.size(); n++) {
            buff[n] = wave_table(index += step);
        }

        // send the entire contents of the buffer
        tx_streamer->send(buffs, buff.size(), metadata);

        metadata.start_of_burst = false;
        metadata.has_time_spec = false;
    }

    // send a mini EOB packet
    metadata.end_of_burst = true;
    tx_streamer->send("", 0, metadata);
}

// ================================================================================================================================================================================ //
//  EOF.	                                                                                                                                                                        //
// ================================================================================================================================================================================ //