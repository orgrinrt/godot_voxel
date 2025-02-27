#ifndef VOXEL_MEMORY_POOL_H
#define VOXEL_MEMORY_POOL_H

#include "../util/fixed_array.h"
#include "../util/math/funcs.h"
#include "core/os/mutex.h"

#include <limits>
#include <unordered_set>
#include <vector>

// Pool based on a scenario where allocated blocks are often the same size.
// A pool of blocks is assigned for each power of two.
// The majority of VoxelBuffers use powers of two so most of the time
// we won't waste memory. Sometimes non-power-of-two buffers are created,
// but they are often temporary and less numerous.
class VoxelMemoryPool {
private:
	struct Pool {
		Mutex mutex;
		// Would a linked list be better?
		std::vector<uint8_t *> blocks;
	};

public:
	static void create_singleton();
	static void destroy_singleton();
	static VoxelMemoryPool *get_singleton();

	VoxelMemoryPool();
	~VoxelMemoryPool();

	uint8_t *allocate(size_t size);
	void recycle(uint8_t *block, size_t size);

	void clear_unused_blocks();

	void debug_print();
	unsigned int debug_get_used_blocks() const;
	size_t debug_get_used_memory() const;
	size_t debug_get_total_memory() const;

private:
	void clear();

#ifdef DEBUG_ENABLED
	void debug_add_allock(void *block) {
		MutexLock lock(_debug_allocs_mutex);
		auto it = _debug_allocs.find(block);
		CRASH_COND(it != _debug_allocs.end());
		_debug_allocs.insert(block);
	}

	void debug_remove_alloc(void *block) {
		MutexLock lock(_debug_allocs_mutex);
		auto it = _debug_allocs.find(block);
		CRASH_COND(it == _debug_allocs.end());
		_debug_allocs.erase(it);
	}
#endif

	inline size_t get_highest_supported_size() const {
		return size_t(1) << (_pot_pools.size() - 1);
	}

	inline unsigned int get_pool_index_from_size(size_t size) const {
#ifdef DEBUG_ENABLED
		// `get_next_power_of_two_32` takes unsigned int
		CRASH_COND(size > std::numeric_limits<unsigned int>::max());
#endif
		return get_shift_from_power_of_two_32(get_next_power_of_two_32(size));
	}

	inline size_t get_size_from_pool_index(unsigned int i) const {
		return size_t(1) << i;
	}

	// We handle allocations with up to 2^20 = 1,048,576 bytes.
	// This is chosen based on practical needs.
	// Each slot in this array corresponds to allocations
	// that contain 2^index bytes in them.
	FixedArray<Pool, 21> _pot_pools;

#ifdef DEBUG_ENABLED
	std::unordered_set<void *> _debug_allocs;
	Mutex _debug_allocs_mutex;
#endif

	unsigned int _used_blocks = 0; // TODO Make atomic?
	size_t _used_memory = 0;
	size_t _total_memory = 0;
};

#endif // VOXEL_MEMORY_POOL_H
