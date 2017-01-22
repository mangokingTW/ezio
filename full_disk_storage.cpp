#include "include/full_disk_storage.hpp"

namespace libtorrent{

	void full_disk_storage::initialize(storage_error& se)
	{
		//traverse every disk partitions and open file descriptor for each one
		std::cerr << "Initialize storage..." << std::endl;
		for( int i=0 ; i < m_files.num_files() ; i++ ){
			std::string partition = find_partition(i);
			if( fd_map[partition] != 0 ) continue;
			if( (fd_map[partition] = open( (target_partition+partition).c_str() , O_RDWR | O_CREAT )) < 0 ){
				std::cerr << "Failed to open " << partition << std::endl;
				exit(-1);
			}
			std::cerr << target_partition << partition << " " << fd_map[partition] << std::endl;
		}
		return;
	}

	// assume no resume
	bool full_disk_storage::has_any_file(storage_error& ec) { return false; }

	int full_disk_storage::readv(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec)
	{
		int index = 0;
		int i = 0;
		int ret = 0;
		int fd = -1;
		unsigned long long device_offset = 0;
		unsigned long long fd_offset = 0; // A fd' point we read data from fd from 
		unsigned long long cur_offset = 0; // A pieces' point we have to write data until
		unsigned long long remain_len = 0;
		unsigned long long piece_sum = 0;
		unsigned long long data_len = 0;
		char *data_buf, *data_ptr = NULL;
		char filename[33]; // Should be the max length of file name

		// Get file name from offset
		index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset);
		memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
		filename[m_files.file_name_len(index)] = 0;
		sscanf(filename,"%llx", &device_offset);

		fd = fd_map[find_partition(index)];

		// Caculate total piece size of previous files
		for( i = 0 ; i < index; i++ )
			piece_sum += m_files.file_size(i);

		// Caculate the length of all bufs
		for( i = 0 ; i < num_bufs ; i ++){
			data_len += bufs[i].iov_len;
		}
		data_buf = (char *)malloc(data_len);

		// Read fd to data_buf
		cur_offset = piece * std::uint64_t(m_files.piece_length()) + offset;
		fd_offset = device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum;
		remain_len = m_files.file_size(index) - (offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
		data_ptr = data_buf;
		while(data_len > 0){
			if( data_len > remain_len ){
				ret += pread(fd, data_ptr, remain_len, fd_offset);
				data_len -= remain_len;
				data_ptr += remain_len;
				cur_offset += remain_len;
				index = m_files.file_index_at_offset( cur_offset );
				memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
				filename[m_files.file_name_len(index)] = 0;
				sscanf(filename,"%llx", &fd_offset);
				remain_len = m_files.file_size(index);
			}
			else{
				ret += pread(fd, data_ptr, data_len, fd_offset);
				data_len -= data_len;
			}
		}

		// Copy data_buf to bufs
		data_ptr = data_buf;
		for( i = 0 ; i < num_bufs ; i ++){
			memcpy(bufs[i].iov_base, data_ptr, bufs[i].iov_len);
			data_ptr += bufs[i].iov_len;
		}

		free(data_buf);
		return ret;
	}

	int full_disk_storage::writev(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec)
	{
		int index = 0;
		int i = 0;
		int ret = 0;
		int fd = -1;
		unsigned long long device_offset = 0;
		unsigned long long fd_offset = 0; // A fd' point we write data to fd from 
		unsigned long long cur_offset = 0; // A pieces' point we have to read data until
		unsigned long long remain_len = 0;
		unsigned long long piece_sum = 0;
		unsigned long long data_len = 0;
		char *data_buf = NULL, *data_ptr = NULL;
		char filename[33]; // Should be the max length of file name

		// Get file name from offset
		index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset);
		memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
		filename[m_files.file_name_len(index)] = 0;
		sscanf(filename,"%llx", &device_offset);

		fd = fd_map[find_partition(index)];

		// Caculate total piece size of previous files
		for( i = 0 ; i < index; i++ )
			piece_sum += m_files.file_size(i);

		// Caculate the length of all bufs
		for( i = 0 ; i < num_bufs ; i ++){
			data_len += bufs[i].iov_len;
		}

		// Merge all bufs into data_buf
		data_buf = (char *)malloc(data_len);
		data_ptr = data_buf;
		for( i = 0 ; i < num_bufs ; i ++){
			memcpy(data_ptr, bufs[i].iov_base, bufs[i].iov_len);
			data_ptr += bufs[i].iov_len;
		}

		// Write data_buf to fd
		cur_offset = piece * std::uint64_t(m_files.piece_length()) + offset;
		fd_offset = device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum;
		remain_len = m_files.file_size(index) - (offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
		data_ptr = data_buf;
		while(data_len > 0){
			if( data_len > remain_len ){
				ret += pwrite(fd, data_ptr, remain_len, fd_offset);
				data_len -= remain_len;
				data_ptr += remain_len;
				cur_offset += remain_len;
				index = m_files.file_index_at_offset( cur_offset );
				memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
				filename[m_files.file_name_len(index)] = 0;
				sscanf(filename,"%llx", &fd_offset);
				remain_len = m_files.file_size(index);
			}
			else{
				ret += pwrite(fd, data_ptr, data_len, fd_offset);
				data_len -= data_len;
			}
		}
		free(data_buf);
		return ret;
	}

	// Not need
	void full_disk_storage::rename_file(int index, std::string const& new_filename, storage_error& ec)
	{ assert(false); return ; }

	int full_disk_storage::move_storage(std::string const& save_path, int flags, storage_error& ec) { return 0; }
	bool full_disk_storage::verify_resume_data(bdecode_node const& rd
					, std::vector<std::string> const* links
					, storage_error& error) { return false; }
	void full_disk_storage::write_resume_data(entry& rd, storage_error& ec) const { return ; }
	void full_disk_storage::set_file_priority(std::vector<boost::uint8_t> const& prio, storage_error& ec) {return ;}
	void full_disk_storage::release_files(storage_error& ec) { return ; }
	void full_disk_storage::delete_files(int i, storage_error& ec) { return ; }

	bool full_disk_storage::tick () { return false; };

	std::string full_disk_storage::find_partition(int index){
		std::string partition = m_files.file_path(index);
		partition.erase(partition.find_last_of('/'));
		partition.erase(0,partition.find('/')+1);
		return partition;
	}
}


libtorrent::storage_interface* full_disk_storage_constructor(libtorrent::storage_params const& params)
{
	return new libtorrent::full_disk_storage(*params.files, params.path);
}

