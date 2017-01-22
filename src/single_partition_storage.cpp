#include "include/single_partition_storage.hpp"

namespace libtorrent{

	void single_partition_storage::initialize(storage_error& se)
	{
		this->fd = open(target_partition.c_str(), O_RDWR | O_CREAT);
		if(this->fd < 0){
			// Failed handle
			std::cerr << "Failed to open " << target_partition << std::endl;

			// TODO exit
		}
		return;
	}

	// assume no resume
	bool single_partition_storage::has_any_file(storage_error& ec) { return false; }

	int single_partition_storage::readv(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec)
	{
		int index = 0;
		int i = 0;
		int ret = 0;
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
				ret += pread(this->fd, data_ptr, remain_len, fd_offset);
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
				ret += pread(this->fd, data_ptr, data_len, fd_offset);
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

	int single_partition_storage::writev(file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, storage_error& ec)
	{
		int index = 0;
		int i = 0;
		int ret = 0;
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

		// Caculate total piece size of previous files
		for( i = 0 ; i < index; i++ )
			piece_sum += m_files.file_size(i);

		// Caculate the length of all bufs
		for( i = 0 ; i < num_bufs ; i ++){
			data_len += bufs[i].iov_len;
		}

		printf("data_len: %llu piece: %d offset: %d\n",data_len,piece,offset);

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
				ret += pwrite(this->fd, data_ptr, remain_len, fd_offset);
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
				ret += pwrite(this->fd, data_ptr, data_len, fd_offset);
				data_len -= data_len;
			}
		}
		free(data_buf);
		return ret;
	}

	// Not need
	void single_partition_storage::rename_file(int index, std::string const& new_filename, storage_error& ec)
	{ assert(false); return ; }

	int single_partition_storage::move_storage(std::string const& save_path, int flags, storage_error& ec) { return 0; }
	bool single_partition_storage::verify_resume_data(bdecode_node const& rd
					, std::vector<std::string> const* links
					, storage_error& error) { return false; }
	void single_partition_storage::write_resume_data(entry& rd, storage_error& ec) const { return ; }
	void single_partition_storage::set_file_priority(std::vector<boost::uint8_t> const& prio, storage_error& ec) {return ;}
	void single_partition_storage::release_files(storage_error& ec) { return ; }
	void single_partition_storage::delete_files(int i, storage_error& ec) { return ; }

	bool single_partition_storage::tick () { return false; };

}


libtorrent::storage_interface* single_partition_storage_constructor(libtorrent::storage_params const& params)
{
	return new libtorrent::single_partition_storage(*params.files, params.path);
}

