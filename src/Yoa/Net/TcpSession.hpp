//
// Created by 404Polaris on 2019/10/15.
//


#pragma once

#include <Yoa/Common/NoCopyAble.hpp>
#include <Yoa/Net/MessageBuffer.hpp>

#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <optional>
#include <asio.hpp>

namespace Yoa {
	namespace net_lib {
		using namespace asio;
		using tcp = asio::ip::tcp;
	}

	template<typename _Msg_reader_t>
	class TcpSession : public std::enable_shared_from_this<TcpSession<_Msg_reader_t>>,
					   public NoCopyAble {
	protected:
		bool work_fine_;
		std::mutex read_mutex_;
		_Msg_reader_t reader_;
		net_lib::tcp::socket socket_;
		net_lib::io_context::strand strand_;
		std::queue<MessageBuffer> read_buffer_queue_;
		std::queue<MessageBuffer> write_buffer_queue_;
	public:
		explicit TcpSession(net_lib::tcp::socket socket, net_lib::io_context &io_context);
		~TcpSession();
	public:
		void Start();
		void Close();
		bool WorkFine();
		void Write(MessageBuffer buffer);
		std::optional<MessageBuffer> Read();
	protected:
		void ReadMsg();
		void WriteMsg();
		void ReadMsgComplete();
	};

	template<typename _Msg_reader_t>
	inline TcpSession<_Msg_reader_t>::TcpSession(net_lib::tcp::socket socket,
													net_lib::io_context &io_context)
		: work_fine_(false), socket_(std::move(socket)), strand_(io_context) {
	}

	template<typename _Msg_reader_t>
	inline TcpSession<_Msg_reader_t>::~TcpSession() {
		socket_.close();
	}

	template<typename _Msg_reader_t>
	inline void TcpSession<_Msg_reader_t>::Start() {
		work_fine_ = true;
		reader_.Init();
		ReadMsg();
	}

	template<typename _Msg_reader_t>
	void TcpSession<_Msg_reader_t>::Close() {
		work_fine_ = false;
	}

	template<typename _Msg_reader_t>
	inline void TcpSession<_Msg_reader_t>::ReadMsg() {
		if (!work_fine_)return;

		auto[no_error, length] = reader_.ShouldRead();

		if (!no_error)return;

		if (no_error && length == 0) {
			ReadMsgComplete();
			reader_.Init();
		}

		auto that = this->shared_from_this();

		asio::async_read(socket_,
						 asio::buffer(reader_.Tail(), length),
						 asio::bind_executor(strand_, [that](std::error_code ec, std::size_t n) {
							 if (!ec) {
								 that->reader_.Write(n);
								 that->ReadMsg();
							 } else {
								 that->work_fine_ = false;
							 }
						 }));
	}

	template<typename _Msg_reader_t>
	void TcpSession<_Msg_reader_t>::ReadMsgComplete() {
		std::lock_guard<std::mutex> lock(read_mutex_);

		auto buffer = reader_.PopBuffer();
		read_buffer_queue_.emplace(std::move(buffer));
	}

	template<typename _Msg_reader_t>
	inline void TcpSession<_Msg_reader_t>::WriteMsg() {
		if (!work_fine_)return;

		auto that = this->shared_from_this();

		asio::async_write(socket_,
						  asio::buffer(write_buffer_queue_.front().Data(), write_buffer_queue_.front().Size()),
						  asio::bind_executor(strand_, [that](std::error_code ec, std::size_t n) {
							  if (!ec) {
								  that->write_buffer_queue_.pop();

								  if (!(that->write_buffer_queue_.empty())) {
									  that->WriteMsg();
								  }
							  } else {
								  that->work_fine_ = false;
							  }
						  }));
	}

	template<typename _Msg_reader_t>
	inline void TcpSession<_Msg_reader_t>::Write(MessageBuffer buffer) {
		if (!work_fine_)return;

		auto that = this->shared_from_this();

		asio::post(asio::bind_executor(strand_, [that, local_buffer = std::move(buffer)]() mutable {
			auto write_flag = !(that->write_buffer_queue_.empty());
			that->write_buffer_queue_.push(std::move(local_buffer));

			if (!write_flag) {
				that->WriteMsg();
			}
		}));
	}

	template<typename _Msg_reader_t>
	inline bool TcpSession<_Msg_reader_t>::WorkFine() {
		return work_fine_;
	}

	template<typename _Msg_reader_t>
	std::optional<MessageBuffer> TcpSession<_Msg_reader_t>::Read() {
		std::lock_guard<std::mutex> lock(read_mutex_);

		if (read_buffer_queue_.empty())return std::nullopt;
		auto buffer = std::move(read_buffer_queue_.front());
		read_buffer_queue_.pop();
		return buffer;
	}
}


