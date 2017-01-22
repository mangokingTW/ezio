#include <iostream>
#include <thread>
#include <chrono>

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
#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>
#include <libtorrent/peer_connection.hpp>

#include <boost/progress.hpp>

using namespace libtorrent;

struct compress_peer_plugin : peer_plugin
{
	compress_peer_plugin(peer_connection_handle const& spc) : pc(spc){
	};

	virtual bool on_request(peer_request const& r) TORRENT_OVERRIDE{
		//do_request(r);
		//return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		return false;
	};
	
	void do_request(peer_request const& r){
		boost::shared_ptr<peer_connection> rpc = pc.native_handle();
		TORRENT_ASSERT(rpc);
	};
	
	int i=0;
	peer_connection_handle pc;
};

struct compress_torrent_plugin
	: torrent_plugin
	, boost::enable_shared_from_this<compress_torrent_plugin>
{
	virtual boost::shared_ptr<peer_plugin> new_connection(peer_connection_handle const& pc) TORRENT_OVERRIDE{
		return boost::shared_ptr<compress_peer_plugin>(new compress_peer_plugin(pc));
	}
};

boost::shared_ptr<compress_torrent_plugin> plugin_func(torrent_handle const& th, void* userdata){
	return boost::shared_ptr<compress_torrent_plugin>(new compress_torrent_plugin());
}

int main(int argc, char const* argv[])
{
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <magnet-url/torrent-file> <target-partition-path>" << std::endl;
		return 1;
	}
	session ses;
	error_code ec;
	settings_pack set;

	// setting
	// we don't need DHT
	set.set_bool(settings_pack::enable_dht, false);
	set.set_str(settings_pack::listen_interfaces, "0.0.0.0:6666");
	ses.apply_settings(set);
	//ses.add_extension(plugin_func);

	add_torrent_params atp;

	// magnet or torrent
	// TODO find a better way
	std::string bt_info = argv[1];
	if(bt_info.substr(bt_info.length() - 8, 8) == ".torrent"){
		atp.ti = boost::make_shared<torrent_info>(bt_info, boost::ref(ec), 0);
	}
	else{
		atp.url = argv[1];
	}
	atp.save_path = argv[2];
	atp.flags |= atp.flag_seed_mode;

	torrent_handle handle = ses.add_torrent(atp);
	//handle.set_max_uploads(4);
	//handle.set_sequential_download(1);
	//boost::progress_display show_progress(100, std::cout);
	unsigned long last_progess = 0, progress = 0;
	torrent_status status;

	for(;;){
		std::vector<alert*> alerts;
		ses.pop_alerts(&alerts);

		status = handle.status();
		// progress
		last_progess = progress;
		progress = status.progress * 100;
		//show_progress += progress - last_progess;
		std::cout << "\r"
			<< "[T: " << progress << "%] "
			<< "[D: " << (float)status.download_payload_rate / 1024 / 1024 << "MB/s] "
			<< "[U: " << (float)status.upload_payload_rate / 1024 / 1024 << "MB/s] "
			<< std::flush;

		for (alert const* a : alerts) {
			// std::cout << a->message() << std::endl;
			// if we receive the finished alert or an error, we're done
			if (alert_cast<torrent_finished_alert>(a)) {
				goto done;
			}
			if (status.is_finished) {
				goto done;
			}
			if (alert_cast<torrent_error_alert>(a)) {
				std::cerr << a->message() << std::endl;
				return 1;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	done:
	std::cout << std::endl;


	// Start high performance seed
	high_performance_seed(set);
	ses.apply_settings(set);
	std::cout << "Start seeding" << std::endl;

	// seed until idle 15mins
	int timeout = 15 * 60;

	// seed until seed rate 300%
	boost::int64_t seeding_rate_limit = 3;
	boost::int64_t total_size = handle.torrent_file()->total_size();

	for (;;) {
		status = handle.status();
		int utime = status.time_since_upload;
		int dtime = status.time_since_download;
		boost::int64_t total_payload_upload = status.total_payload_upload;

		std::cout << "\r"
			<< "[T: " << progress << "%] "
			<< "[D: " << (float)status.download_payload_rate / 1024 / 1024 << "MB/s] "
			<< "[U: " << (float)status.upload_payload_rate / 1024 / 1024 << "MB/s] "
			<< std::flush;

		if(utime == -1 && timeout < dtime){
			break;
		}
		else if(timeout < utime){
			// idle 15mins
			break;
		}
		else if(seeding_rate_limit < (total_payload_upload / total_size)){
			// seeding 300%
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout << std::endl;
	std::cout << "done, shutting down" << std::endl;

	return 0;
}
