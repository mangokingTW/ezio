#include <libtorrent/file_storage.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/storage.hpp>

namespace libtorrent{

struct single_partition_storage : storage_interface {
	single_partition_storage(file_storage const& fs, const std::string tp) : m_files(fs), target_partition(tp) {}

	// Open disk fd
	void initialize(storage_error& se);

	// assume no resume
	bool has_any_file(storage_error& ec);

	int readv(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec);

	int writev(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec);

	// Not need
	void rename_file(int index, std::string const& new_filename, storage_error& ec);

	int move_storage(std::string const& save_path, int flags, storage_error& ec);
	bool verify_resume_data(bdecode_node const& rd
					, std::vector<std::string> const* links
					, storage_error& error);
	void write_resume_data(entry& rd, storage_error& ec) const;
	void set_file_priority(std::vector<boost::uint8_t> const& prio, storage_error& ec);
	void release_files(storage_error& ec);
	void delete_files(int i, storage_error& ec);
	bool tick ();

	private:

	file_storage m_files;
	int fd;
	const std::string target_partition;
};

}

libtorrent::storage_interface* single_partition_storage_constructor(libtorrent::storage_params const& params);
