// NereusSDR for iOS: isolated libdatachannel retirement and global cleanup probe
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include "impl/icetransport.hpp"
#include "impl/init.hpp"
#include "impl/threadpool.hpp"

#include <rtc/global.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;

static void require(bool condition, const char *reason) {
	if (!condition)
		throw std::runtime_error(reason);
}

int main() {
	try {
		char directory[] = "/tmp/nereus-retirement-XXXXXX";
		require(mkdtemp(directory) != nullptr, "mkdtemp failed");
		const std::filesystem::path gate = std::filesystem::path(directory) / "resolver";
		struct GateCleanup {
			std::filesystem::path gate;
			~GateCleanup() {
				std::ofstream(gate).close();
				std::filesystem::remove_all(gate.parent_path());
			}
		} cleanup{gate};

		rtc::Preload();
		auto observed = rtc::impl::Init::Instance().token();
		std::weak_ptr<void> retirementToken = observed;
		observed.reset();

		rtc::Configuration config;
		config.iceServers.emplace_back("resolver-barrier.nereus.invalid", 3478,
		                               "gate:" + gate.string(), "test-only");
		auto transport = std::make_unique<rtc::impl::IceTransport>(
		    config, [](const rtc::Candidate &) {},
		    [](rtc::impl::Transport::State) {},
		    [](rtc::impl::IceTransport::GatheringState) {});
		transport->gatherLocalCandidates("0");

		const auto entered = gate.string() + ".entered";
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (!std::filesystem::exists(entered) && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(10ms);
		require(std::filesystem::exists(entered), "resolver did not enter test gate");

		transport.reset(); // actual IceTransport destructor transfers its agent and Init token
		auto cleanupFuture = rtc::Cleanup();
		require(retirementToken.use_count() == 1,
		        "retirement does not exclusively own the remaining Init token");
		require(cleanupFuture.wait_for(0ms) == std::future_status::timeout,
		        "global cleanup completed while resolver and retirement remain pending");

		auto sentinel = rtc::impl::ThreadPool::Instance().enqueue([] { return 17; });
		require(sentinel.wait_for(3s) == std::future_status::ready,
		        "ThreadPool stopped before a pending retirement could finish");
		require(sentinel.get() == 17, "ThreadPool sentinel returned wrong result");
		require(retirementToken.use_count() == 1,
		        "another owner retained the Init token while cleanup was pending");
		require(cleanupFuture.wait_for(0ms) == std::future_status::timeout,
		        "global cleanup completed before resolver release");

		std::ofstream(gate).close();
		require(cleanupFuture.wait_for(4s) == std::future_status::ready,
		        "global cleanup did not complete after resolver exit and final agent destruction");
		cleanupFuture.get();
		require(retirementToken.expired(), "retirement kept its Init token after cleanup");
		std::cout << "retirement token, ThreadPool continuation, and last-agent cleanup passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "retirement probe: " << error.what() << '\n';
		return 1;
	}
}
