#pragma once

#include "const.h"

#include "hiredis/hiredis.h"

#include <queue>

#include <atomic>

#include <mutex>

#include <thread>

#include <chrono>

#include <cstring>



class RedisConPool {

public:

	RedisConPool(size_t poolSize, const char* host, int port, const char* pwd)

		: poolSize_(poolSize), host_(host), port_(port), b_stop_(false), pwd_(pwd), counter_(0), fail_count_(0) {

		for (size_t i = 0; i < poolSize_; ++i) {

			auto* context = redisConnect(host, port);

			if (context == nullptr || context->err != 0) {

				if (context != nullptr) {

					redisFree(context);

				}

				continue;

			}



			auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd);

			if (reply->type == REDIS_REPLY_ERROR) {

				std::cout << "璁よ瘉澶辫触" << std::endl;

				freeReplyObject(reply);

				continue;

			}



			freeReplyObject(reply);

			std::cout << "璁よ瘉鎴愬姛" << std::endl;

			connections_.push(context);

		}



		check_thread_ = std::thread([this]() {

			while (!b_stop_) {

				counter_++;

				if (counter_ >= 60) {

					checkThreadPro();

					counter_ = 0;

				}

				std::this_thread::sleep_for(std::chrono::seconds(1));

			}

			});

	}



	~RedisConPool() {



	}



	void ClearConnections() {

		std::lock_guard<std::mutex> lock(mutex_);

		while (!connections_.empty()) {

			auto* context = connections_.front();

			redisFree(context);

			connections_.pop();

		}

	}



	redisContext* getConnection() {

		std::unique_lock<std::mutex> lock(mutex_);

		cond_.wait(lock, [this] {

			if (b_stop_) {

				return true;

			}

			return !connections_.empty();

			});

		if (b_stop_) {

			return  nullptr;

		}

		auto* context = connections_.front();

		connections_.pop();

		return context;

	}



	redisContext* getConNonBlock() {

		std::unique_lock<std::mutex> lock(mutex_);

		if (b_stop_) {

			return nullptr;

		}

		if (connections_.empty()) {

			return nullptr;

		}

		auto* context = connections_.front();

		connections_.pop();

		return context;

	}



	void returnConnection(redisContext* context) {

		std::lock_guard<std::mutex> lock(mutex_);

		if (b_stop_) {

			return;

		}

		connections_.push(context);

		cond_.notify_one();

	}



	void Close() {

		b_stop_ = true;

		cond_.notify_all();

		check_thread_.join();

	}



private:

	bool reconnect() {

		auto context = redisConnect(host_, port_);

		if (context == nullptr || context->err != 0) {

			if (context != nullptr) {

				redisFree(context);

			}

			return false;

		}



		auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd_);

		if (reply->type == REDIS_REPLY_ERROR) {

			std::cout << "璁よ瘉澶辫触" << std::endl;

			freeReplyObject(reply);

			redisFree(context);

			return false;

		}



		freeReplyObject(reply);

		std::cout << "璁よ瘉鎴愬姛" << std::endl;

		returnConnection(context);

		return true;

	}



	void checkThreadPro() {

		size_t pool_size;

		{

			std::lock_guard<std::mutex> lock(mutex_);

			pool_size = connections_.size();

		}



		for (int i = 0; i < (int)pool_size && !b_stop_; ++i) {

			auto* context = getConNonBlock();

			if (context == nullptr) {

				break;

			}



			redisReply* reply = nullptr;

			try {

				reply = (redisReply*)redisCommand(context, "PING");

				if (context->err) {

					std::cout << "Connection error: " << context->err << std::endl;

					if (reply) { freeReplyObject(reply); }

					redisFree(context);

					fail_count_++;

					continue;

				}

				if (!reply || reply->type == REDIS_REPLY_ERROR) {

					std::cout << "reply is null, redis ping failed" << std::endl;

					if (reply) { freeReplyObject(reply); }

					redisFree(context);

					fail_count_++;

					continue;

				}

				freeReplyObject(reply);

				returnConnection(context);

			}

			catch (std::exception&) {

				if (reply) { freeReplyObject(reply); }

				redisFree(context);

				fail_count_++;

			}

		}



		while (fail_count_ > 0) {

			if (reconnect()) {

				fail_count_--;

			} else {

				break;

			}

		}

	}



	std::atomic<bool> b_stop_;

	size_t poolSize_;

	const char* host_;

	const char* pwd_;

	int port_;

	std::queue<redisContext*> connections_;

	std::atomic<int> fail_count_;

	std::mutex mutex_;

	std::condition_variable cond_;

	std::thread check_thread_;

	int counter_;

};

