#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "hash.hpp"
#include "hvectors.hpp"
#include "slice.hpp"

struct Block {
	hash256_t hash = {};
	hash256_t prevBlockHash = {};
	uint32_t bits;

	Block () {}
	Block (const hash256_t& hash, const hash256_t& prevBlockHash, const uint32_t bits) : hash(hash), prevBlockHash(prevBlockHash), bits(bits) {}
};

// find all blocks who are not parents to any other blocks (aka, a chain tip)
auto findChainTips(const HMap<hash256_t, Block>& blocks) {
	std::map<hash256_t, bool> parents;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		if (blocks.find(block.prevBlockHash) == blocks.end()) continue;

		parents[block.prevBlockHash] = true;
	}

	std::vector<Block> tips;
	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		if (parents.find(block.hash) != parents.end()) continue;

		tips.emplace_back(block);
	}

	return tips;
}

auto determineWork(const HMap<hash256_t, size_t>& workCache, const HMap<hash256_t, Block>& blocks, const Block source) {
	auto visitor = source;
	size_t totalWork = source.bits;

	// naively walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);
		if (prevBlockIter == blocks.end()) break;

		const auto prevBlockWorkCacheIter = workCache.find(visitor.prevBlockHash);
		if (prevBlockWorkCacheIter != workCache.end()) {
			totalWork += prevBlockWorkCacheIter->second;
			break;
		}

		visitor = prevBlockIter->second;
		totalWork += visitor.bits;
	}

	return totalWork;
}

auto findBest(const HMap<hash256_t, Block>& blocks) {
	auto bestBlock = Block();
	size_t mostWork = 0;

	HMap<hash256_t, size_t> workCache;

	for (const auto& blockIter : blocks) {
		const auto& block = blockIter.second;
		const auto work = determineWork(workCache, blocks, block);

		workCache.insort(block.hash, work);

		if (work > mostWork) {
			bestBlock = block;
			mostWork = work;
		}
	}

	auto visitor = bestBlock;
	std::vector<Block> blockchain;
	blockchain.push_back(visitor);

	// walk the chain
	while (true) {
		const auto prevBlockIter = blocks.find(visitor.prevBlockHash);
		if (prevBlockIter == blocks.end()) break;

		visitor = prevBlockIter->second;
		blockchain.push_back(visitor);
	}

	std::reverse(blockchain.begin(), blockchain.end());
	return blockchain;
}

void printerr_hash32 (hash256_t hash) {
	std::cerr << std::hex;
	for (size_t i = 31; i < 32; --i) {
		std::cerr << std::setw(2) << std::setfill('0') << (uint32_t) hash[i];
	}
	std::cerr << std::dec;
}

int main () {
	HMap<hash256_t, Block> blocks;

	// read block headers from stdin until EOF
	{
		while (true) {
			uint8_t rbuf[80];
			const auto read = fread(rbuf, sizeof(rbuf), 1, stdin);

			// EOF?
			if (read == 0) break;

			hash256_t hash, prevBlockHash;
			uint32_t bits;

			hash256(hash.begin(), rbuf, 80);
			memcpy(prevBlockHash.begin(), rbuf + 4, 32);
			memcpy(&bits, rbuf + 72, 4);

			blocks.emplace_back(std::make_pair(hash, Block(hash, prevBlockHash, bits)));
		}

		std::cerr << "Read " << blocks.size() << " headers" << std::endl;
		blocks.sort();
		std::cerr << "Sorted " << blocks.size() << " headers" << std::endl;
	}

	// how many tips exist?
	{
		const auto chainTips = findChainTips(blocks);
		std::cerr << "Found " << chainTips.size() << " chain tips" << std::endl;
	}

	// what is the best?
	const auto bestBlockChain = findBest(blocks);

	// print some general information
	{
		const auto genesis = bestBlockChain.front();
		const auto tip = bestBlockChain.back();

		// output
		std::cerr << "Best chain" << std::endl;
		std::cerr << "- Height: " << bestBlockChain.size() - 1 << std::endl;
		std::cerr << "- Genesis: ";
		printerr_hash32(genesis.hash);
		std::cerr << std::endl << "- Tip: ";
		printerr_hash32(tip.hash);
		std::cerr << std::endl;
	}

	// output the best chain [in order]
	{
		HMap<hash256_t, uint32_t> blockChainMap;

		int32_t height = 0;
		for (const auto& block : bestBlockChain) {
			blockChainMap.push_back(std::make_pair(block.hash, height));
			++height;
		}
		blockChainMap.sort();

		uint8_t sbuf[36];
		for (auto&& blockIter : blockChainMap) {
			memcpy(blockIter.first.begin(), sbuf, 32);
			Slice(sbuf + 32, sbuf + 36).put(blockIter.second);

			fwrite(sbuf, sizeof(sbuf), 1, stdout);
		}
	}

	return 0;
}
