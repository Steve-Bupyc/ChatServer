#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>

#include "ChatServerConfig.h"
#include "chat_message.h"

using boost::asio::ip::tcp;

void parse_console_parameters(int argc, char** argv, ChatServerConfig& config);
bool help_opt = false;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant {
 public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;
std::string name;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room {
  friend class chat_server;

 public:
  chat_room(size_t userlimit) : userlimit(userlimit), users_num(0) {}

  void join(chat_participant_ptr participant, std::string ip_addr, unsigned short port) {    
    users_num++;
    participant->name = ip_addr + " " + std::to_string(port);
    std::string append_msg = "user " + participant->name + " joined the chat";
    chat_message msg_append = get_message(append_msg);
    for (auto participant : participants_) participant->deliver(msg_append);

    std::string welcome_msg = "Welcome, user " + participant->name + "!";
    chat_message msg_welcome = get_message(welcome_msg);
    participant->deliver(msg_welcome);

    participants_.insert(participant);
    for (const auto& msg : recent_msgs_) participant->deliver(msg);
  }

  void leave(chat_participant_ptr participant) {
    users_num--;
    participants_.erase(participant);

    std::string left_msg = "user " + participant->name + " left the chat";
    chat_message msg_left = get_message(left_msg);
    for (auto participant_ : participants_) participant_->deliver(msg_left);
  }

  void deliver(const chat_message& msg) {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs) recent_msgs_.pop_front();

    for (auto participant : participants_) participant->deliver(msg);
  }

  chat_message get_message(std::string message_str) {
    char line[chat_message::max_body_length + 1];
    strcpy(line, message_str.c_str());
    chat_message msg;
    msg.body_length(std::strlen(line));
    std::memcpy(msg.body(), line, msg.body_length());
    msg.encode_header();
    return msg;
  }

 private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
  std::atomic<size_t> users_num;
  size_t userlimit;
};

//----------------------------------------------------------------------

class chat_session : public chat_participant,
                     public std::enable_shared_from_this<chat_session> {
 public:
  chat_session(tcp::socket socket, chat_room& room)
      : socket_(std::move(socket)), room_(room) {}

  void start() {
    auto ip_addr = socket_.remote_endpoint().address().to_string();
    auto port = socket_.remote_endpoint().port();

    room_.join(shared_from_this(), ip_addr, port);
    do_read_header();
  }

  void deliver(const chat_message& msg) {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      do_write();
    }
  }

 private:
  void do_read_header() {
    auto self(shared_from_this());
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec && read_msg_.decode_header()) {
            do_read_body();
          } else {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_read_body() {
    auto self(shared_from_this());
    boost::asio::async_read(
        socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            /// <time>
            std::string msg_with_time(currentDateTime());
            msg_with_time += " ";
            msg_with_time += name;
            msg_with_time += " ";
            msg_with_time += read_msg_.body();
            msg_with_time += " ";            

            std::cout << msg_with_time;
            /// </time>
            chat_message message_with_time(room_.get_message(msg_with_time));
            read_msg_.clear();
            room_.deliver(/*read_msg_*/ message_with_time);
            do_read_header();
          } else {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_write() {
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front().data(),
                            write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              do_write();
            }
          } else {
            room_.leave(shared_from_this());
          }
        });
  }

  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;

  const std::string currentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
  }
};

//----------------------------------------------------------------------

class chat_server {
 public:
  chat_server(boost::asio::io_context& io_context,
              const tcp::endpoint& endpoint, ChatServerConfig config)
      : acceptor_(io_context, endpoint),
        room_(config.userlimit),
        verbose(config.verbose) {
    do_accept();
  }

 private:
  void do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec) {            
            if (room_.users_num < room_.userlimit) {
              std::cout << "user joined\n";
              std::make_shared<chat_session>(std::move(socket), room_)->start();
            } else {
              if (verbose) {
                std::cout << "Number of users has reached its limit.\n";
              }
              socket.close();              
            }
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
  chat_room room_;
  bool verbose;
};