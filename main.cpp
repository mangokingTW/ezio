#include <iostream>
#include <thread>
#include <chrono>
#include <getopt.h>
#include <stddef.h>

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/io.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>

#include <boost/progress.hpp>

#include "include/single_partition_storage.hpp"
#include "include/full_disk_storage.hpp"

namespace lt = libtorrent;

int timeout_ezio = 15; // Default timeout (min)
int seed_limit_ezio = 3; // Default seeding ratio limit
int max_upload_ezio = 4;
int max_contact_tracker_times = 10; // Max error times for scrape tracker


void usage()
{
      std::cerr << "Usage: ezio [OPTIONS] <magnet-url/torrent-file> <target-partition-path>\n"
                << "OPTIONS:\n"
                << "	-e N: assign seeding ratio limit as N. Default value is " << seed_limit_ezio <<"\n"
                << "	-k N: assign maxminum failure number to contact tracker as N. Default value is " << max_contact_tracker_times<< "\n"
                << "	-m N: assign maxminum upload number as N. Default value is " << max_upload_ezio <<"\n"
                << "	-s: enable sequential download\n"
                << "	-t N: assign timeout as N min(s). Default value " << timeout_ezio <<"\n"
		<< "	-a: enable the entire disk clone mode"
      		<< std::endl;
}

int main(int argc, char ** argv)
{
	lt::add_torrent_params atp;
	int opt;
	int opt_n = 0;
	int seq_flag = 0;
	int disk_flag = 0;

	opterr = 0;
	while (( opt = getopt (argc, argv, "ae:m:st:")) != -1)
	  switch (opt)
	    {
	    case 'a':
	      disk_flag = 1;
              ++opt_n;
	      break;
	    case 'e':
	      seed_limit_ezio = atoi(optarg);
	      ++opt_n;
	      ++opt_n;
	      break;
	    case 'm':
	      max_upload_ezio = atoi(optarg);
	      ++opt_n;
	      ++opt_n;
	      break;
	    case 's':
	      seq_flag = 1;
	      ++opt_n;
	      break;
	    case 't':
	      timeout_ezio = atoi(optarg);
	      ++opt_n;
	      ++opt_n;
	      break;
	    case '?':
	      usage();
	      return 1;
	    default:
	      usage();
	      exit(EXIT_FAILURE);
	}

	if (argc - opt_n != 3) {
		usage();
		return 1;
	}
	std::string bt_info = argv[optind];
	++optind;;
	atp.save_path = argv[optind];

	if (seq_flag) {
	  std::cout << "//NOTE// Sequential download is enabled!" << std::endl;
	}

	lt::session ses;
	lt::error_code ec;
	lt::settings_pack set;

	// setting
	// we don't need DHT
	set.set_bool(lt::settings_pack::enable_dht, false);
	ses.apply_settings(set);

	// magnet or torrent
	// TODO find a better way
	if(bt_info.substr(bt_info.length() - 8, 8) == ".torrent"){
		atp.ti = boost::make_shared<lt::torrent_info>(bt_info, boost::ref(ec), 0);
	}
	else{
		atp.url = bt_info;
	}
	atp.storage = disk_flag ? full_disk_storage_constructor : single_partition_storage_constructor;

	lt::torrent_handle handle = ses.add_torrent(atp);
	handle.set_max_uploads(max_upload_ezio);
	handle.set_sequential_download(seq_flag);
	//boost::progress_display show_progress(100, std::cout);
	unsigned long last_progess = 0, progress = 0;
	lt::torrent_status status;

	std::cout << "Start downloading" << std::endl;

	for(;;){
		std::vector<lt::alert*> alerts;
		ses.pop_alerts(&alerts);

		status = handle.status();
		// progress
		last_progess = progress;
		progress = status.progress * 100;
		//show_progress += progress - last_progess;
		std::cout << "\r"
			<< "[P: " << progress << "%] "
			<< "[D: " << std::setprecision(2) << (float)status.download_payload_rate / 1024 / 1024 /1024 * 60 << " GB/min] "
			<< "[DT: " << (int)status.active_time  << " secs] "
			<< "[U: " << std::setprecision(2) << (float)status.upload_payload_rate / 1024 / 1024 /1024 *60 << " GB/min] "
			<< "[UT: " << (int)status.seeding_time  << " secs] "
			<< std::flush;

		for (lt::alert const* a : alerts) {
			// std::cout << a->message() << std::endl;
			// if we receive the finished alert or an error, we're done
			if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
				goto done;
			}
			if (status.is_finished) {
				goto done;
			}
			if (lt::alert_cast<lt::torrent_error_alert>(a)) {
				std::cerr << "Error" << std::endl;
				return 1;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	done:
	std::cout << std::endl;


	// Start high performance seed
	lt::high_performance_seed(set);
	ses.apply_settings(set);
	std::cout << "Start high-performance seeding" << std::endl;

	// seed until idle (secs)
	int timeout = timeout_ezio * 60;

	// seed until seed rate
	boost::int64_t seeding_rate_limit = seed_limit_ezio;
	boost::int64_t total_size = handle.torrent_file()->total_size();

	int fail_contact_tracker = 0;
	for (;;) {
		status = handle.status();
		int utime = status.time_since_upload;
		int dtime = status.time_since_download;
		boost::int64_t total_payload_upload = status.total_payload_upload;
		// ses.set_alert_mask(lt::alert::tracker_notification | lt::alert::error_notification);
		std::vector<lt::alert*> alerts;
		ses.pop_alerts(&alerts);

		std::cout << "\r"
			/*
			<< "[P: " << progress << "%] "
			<< "[D: " << std::setprecision(2) << (float)status.download_payload_rate / 1024 / 1024 /1024 * 60 << " GB/min] "
			<< "[T: " << (int)status.active_time  << " secs] "
			*/
			<< "[U: " << std::setprecision(2) << (float)status.upload_payload_rate / 1024 / 1024 /1024 * 60 << " MB/min] "
			<< "[T: " << (int)status.seeding_time  << " secs] "
			<< std::flush;

		if(utime == -1 && timeout < dtime){
			break;
		}
		else if(timeout < utime){
			break;
		}
		else if(seeding_rate_limit < (total_payload_upload / total_size)){
			break;
		}

		handle.scrape_tracker();
		for (lt::alert const* a : alerts) {
			if (lt::alert_cast<lt::scrape_failed_alert>(a)) {
				++fail_contact_tracker;;
			}
		}

		if(fail_contact_tracker > max_contact_tracker_times){
	                std::cout << "\nTracker is gone! Finish seeding!" << std::endl;
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout << "\nDone, shutting down" << std::endl;

	return 0;
}
