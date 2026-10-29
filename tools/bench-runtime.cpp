// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Runtime micro-benchmarks over REAL rAthena common infrastructure.
// No database, no network, no config files: safe anywhere.
//
//   build: <cxx> -std=c++23 -O2 -Isrc tools/bench-runtime.cpp
//          <libcommon*.a from the measured build> -o bench-runtime
//   (needs GCC 13+/Clang 18+ for <format>, same floor as the project)
//   run:   ./bench-runtime [reps=5]
//
// Kernels (median of `reps` runs after 1 warmup, same fixed workload
// every run — the workload is identical across -O2/-O3/LTO A/B legs):
//   db-idb      idb hash: N puts + 5 lookup passes      (ns/op)
//   memmgr      aMalloc/aFree churn via memmgr          (ns/op)
//   atoi/a2i/stoi/atol/a2l/atof/a2f                     (ns/call)
//   strbuf/snprintf/std::format same payload            (ns/iter)
//
// This measures micro-kernels inside common/, NOT end-to-end server
// throughput (that needs a DB + clients and is out of scope). Treat
// deltas below the machine's noise band as zero.

#include <common/db.hpp>
#include <common/malloc.hpp>
#include <common/strlib.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <vector>

using clk = std::chrono::steady_clock;

static volatile uint64_t g_sink = 0;

// Server-side hook referenced by common/cli.cpp — every real server
// (map/char/login/web) defines its own; the benchmark is not a server.
void display_helpscreen(bool) {}

// Runs `fn` once (warmup), then `reps` timed runs; returns {median, min}
// nanoseconds per op where one run performs `ops_per_run` operations.
template <typename F>
static void bench(const char* name, size_t ops_per_run, int reps, F&& fn) {
	fn(); // warmup (untimed)
	std::vector<double> samples;
	samples.reserve(reps);
	for (int r = 0; r < reps; ++r) {
		const auto t0 = clk::now();
		fn();
		const auto t1 = clk::now();
		const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		samples.push_back(ns / static_cast<double>(ops_per_run));
	}
	std::sort(samples.begin(), samples.end());
	const double median = samples[samples.size() / 2];
	std::printf("%-28s %12.2f %12.2f %12zu\n",
		name, median, samples.front(), ops_per_run);
}

int main(int argc, char** argv) {
	const int reps = (argc > 1) ? std::atoi(argv[1]) : 5;
	if (reps < 1) {
		std::fprintf(stderr, "usage: %s [reps>=1]\n", argv[0]);
		return 2;
	}

	// Every real server runs this from server_core::Core::start; the
	// benchmark has no core loop, and idb_alloc() dereferences the ERS
	// pool that db_init() creates — without it: NULL->alloc segfault.
	db_init();

	std::printf("%-28s %12s %12s %12s\n", "kernel", "median ns", "min ns", "ops/run");

	// ---- db: int-key hash, production idb_ layer (script constants etc.)
	{
		constexpr int N = 50000;
		constexpr int LOOKUP_PASSES = 5;
		DBMap* db = idb_alloc(DB_OPT_BASE);
		bench("db-idb put+5xlookup", (N + N * LOOKUP_PASSES), reps, [&] {
			for (int i = 0; i < N; ++i)
				idb_put(db, i * 4 + 7, reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
			uint64_t sum = 0;
			for (int p = 0; p < LOOKUP_PASSES; ++p)
				for (int i = 0; i < N; ++i)
					sum += static_cast<uint64_t>(reinterpret_cast<intptr_t>(idb_get(db, i * 4 + 7)));
			g_sink = g_sink + sum;
		});
		db_destroy(db);
	}

	// ---- memory manager churn (the aMalloc path every server uses)
	{
		constexpr int M = 100000;
		static const size_t sizes[6] = {16, 48, 128, 512, 1024, 4096};
		bench("memmgr aMalloc/aFree", M, reps, [&] {
			for (int i = 0; i < M; ++i) {
				const size_t sz = sizes[i % 6];
				char* p = static_cast<char*>(aMalloc(sz));
				p[0] = static_cast<char>(i);
				p[sz - 1] = static_cast<char>(i);
				g_sink = g_sink + static_cast<unsigned char>(p[0]);
				aFree(p);
			}
		});
	}

	// ---- string -> number family (the a2i/a2l/a2f migration surface)
	{
		static const char* const inputs[8] = {
			"12345", "-987654321", "0", "2147483647",
			"-42", "999999999", "garbage12", "  77x",
		};
		std::string str_inputs[8];
		for (int i = 0; i < 8; ++i)
			str_inputs[i] = inputs[i];
		constexpr int K = 250000; // x8 inputs per run
		bench("parse atoi", K * 8, reps, [&] {
			int32_t acc = 0;
			for (int k = 0; k < K; ++k)
				for (int i = 0; i < 8; ++i)
					acc += atoi(inputs[i]);
			g_sink = g_sink + static_cast<uint32_t>(acc);
		});
		bench("parse a2i", K * 8, reps, [&] {
			int32_t acc = 0;
			for (int k = 0; k < K; ++k)
				for (int i = 0; i < 8; ++i)
					acc += a2i(inputs[i]);
			g_sink = g_sink + static_cast<uint32_t>(acc);
		});
		bench("parse std::stoi(string)", K * 8, reps, [&] {
			int32_t acc = 0;
			for (int k = 0; k < K; ++k)
				for (int i = 0; i < 8; ++i) {
					// stoi throws on 2 of the 8 inputs (the crash class the
					// migration removed); catch to keep the workload alive.
					try {
						acc += std::stoi(str_inputs[i]);
					} catch (const std::exception&) {
						acc += 0;
					}
				}
			g_sink = g_sink + static_cast<uint32_t>(acc);
		});
		constexpr int K2 = 100000;
		bench("parse atol", K2 * 8, reps, [&] {
			int64_t acc = 0;
			for (int k = 0; k < K2; ++k)
				for (int i = 0; i < 8; ++i)
					acc += atol(inputs[i]);
			g_sink = g_sink + static_cast<uint64_t>(acc);
		});
		bench("parse a2l", K2 * 8, reps, [&] {
			int64_t acc = 0;
			for (int k = 0; k < K2; ++k)
				for (int i = 0; i < 8; ++i)
					acc += a2l(inputs[i]);
			g_sink = g_sink + static_cast<uint64_t>(acc);
		});
		static const char* const finputs[8] = {
			"1.5", "-2.25", "0.0", "3.14", "-0.5", "999.99", "nan", "1e400",
		};
		bench("parse atof", K2 * 8, reps, [&] {
			double acc = 0;
			for (int k = 0; k < K2; ++k)
				for (int i = 0; i < 8; ++i)
					acc += atof(finputs[i]);
			g_sink = g_sink + static_cast<uint64_t>(acc);
		});
		bench("parse a2f", K2 * 8, reps, [&] {
			double acc = 0;
			for (int k = 0; k < K2; ++k)
				for (int i = 0; i < 8; ++i)
					acc += a2f(finputs[i]);
			g_sink = g_sink + static_cast<uint64_t>(acc);
		});
	}

	// ---- string building: StringBuf (production) vs snprintf vs std::format
	{
		constexpr int I = 30000;
		const char* name = "battleground_2026";
		bench("strbuf StringBuf_Printf", I, reps, [&] {
			StringBuf* sbuf = StringBuf_Malloc();
			size_t len = 0;
			for (int i = 0; i < I; ++i) {
				StringBuf_Clear(sbuf);
				len += StringBuf_Printf(sbuf, "%s|%d|0x%08x", name, i,
					static_cast<uint32_t>(i * 2654435761u));
			}
			g_sink = g_sink + len;
			StringBuf_Free(sbuf);
		});
		bench("snprintf stack", I, reps, [&] {
			size_t len = 0;
			for (int i = 0; i < I; ++i) {
				char b[128];
				len += static_cast<size_t>(std::snprintf(b, sizeof(b), "%s|%d|0x%08x", name, i,
					static_cast<uint32_t>(i * 2654435761u)));
			}
			g_sink = g_sink + len;
		});
		bench("std::format", I, reps, [&] {
			size_t len = 0;
			for (int i = 0; i < I; ++i) {
				std::string s = std::format("{}|{}|{:08x}", name, i,
					static_cast<uint32_t>(i * 2654435761u));
				len += s.size();
			}
			g_sink = g_sink + len;
		});
	}

	std::printf("sink=%llu (sanity: must differ across runs if inputs change)\n",
		static_cast<unsigned long long>(g_sink));
	return 0;
}
