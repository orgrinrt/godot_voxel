#include "voxel_memory_pool.h"
#include "../util/macros.h"
#include "../util/profiling.h"

#include <core/os/os.h>
#include <core/print_string.h>
#include <core/variant.h>

namespace {
VoxelMemoryPool *g_memory_pool = nullptr;
} // namespace

void VoxelMemoryPool::create_singleton() {
	CRASH_COND(g_memory_pool != nullptr);
	g_memory_pool = memnew(VoxelMemoryPool);
}

void VoxelMemoryPool::destroy_singleton() {
	CRASH_COND(g_memory_pool == nullptr);
	VoxelMemoryPool *pool = g_memory_pool;
	g_memory_pool = nullptr;
	memdelete(pool);
}

VoxelMemoryPool *VoxelMemoryPool::get_singleton() {
	CRASH_COND(g_memory_pool == nullptr);
	return g_memory_pool;
}

VoxelMemoryPool::VoxelMemoryPool() {
}

VoxelMemoryPool::~VoxelMemoryPool() {
#ifdef TOOLS_ENABLED
	if (OS::get_singleton()->is_stdout_verbose()) {
		debug_print();
	}
#endif
	clear();
}

uint8_t *VoxelMemoryPool::allocate(size_t size) {
	VOXEL_PROFILE_SCOPE();
	CRASH_COND(size == 0);
	uint8_t *block = nullptr;
	// Not calculating `pot` immediately because the function we use to calculate it uses 32 bits,
	// while `size_t` can be larger than that.
	if (size > get_highest_supported_size()) {
		// Sorry, memory is not pooled past this size
		block = (uint8_t *)memalloc(size * sizeof(uint8_t));
	} else {
		const unsigned int pot = get_pool_index_from_size(size);
		Pool &pool = _pot_pools[pot];
		pool.mutex.lock();
		if (pool.blocks.size() > 0) {
			block = pool.blocks.back();
			pool.blocks.pop_back();
			pool.mutex.unlock();
		} else {
			pool.mutex.unlock();
			block = (uint8_t *)memalloc(size * sizeof(uint8_t));
		}
	}
#ifdef DEBUG_ENABLED
	debug_add_allock(block);
#endif
	++_used_blocks;
	_used_memory += size;
	return block;
}

void VoxelMemoryPool::recycle(uint8_t *block, size_t size) {
	CRASH_COND(size == 0);
	CRASH_COND(block == nullptr);
#ifdef DEBUG_ENABLED
	debug_remove_alloc(block);
#endif
	// Not calculating `pot` immediately because the function we use to calculate it uses 32 bits,
	// while `size_t` can be larger than that.
	if (size > get_highest_supported_size()) {
		memfree(block);
	} else {
		const unsigned int pot = get_pool_index_from_size(size);
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		pool.blocks.push_back(block);
	}
	--_used_blocks;
	_used_memory -= size;
}

void VoxelMemoryPool::clear_unused_blocks() {
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		for (unsigned int i = 0; i < pool.blocks.size(); ++i) {
			void *block = pool.blocks[i];
			memfree(block);
		}
		_total_memory -= get_size_from_pool_index(pot) * pool.blocks.size();
		pool.blocks.clear();
	}
}

void VoxelMemoryPool::clear() {
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		for (unsigned int i = 0; i < pool.blocks.size(); ++i) {
			void *block = pool.blocks[i];
			memfree(block);
		}
		pool.blocks.clear();
	}
	_used_memory = 0;
	_total_memory = 0;
	_used_blocks = 0;
}

void VoxelMemoryPool::debug_print() {
	print_line("-------- VoxelMemoryPool ----------");
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		print_line(String("Pool {0}: {1} blocks (capacity {2})")
						   .format(varray(pot,
								   SIZE_T_TO_VARIANT(pool.blocks.size()),
								   SIZE_T_TO_VARIANT(pool.blocks.capacity()))));
	}
}

unsigned int VoxelMemoryPool::debug_get_used_blocks() const {
	//MutexLock lock(_mutex);
	return _used_blocks;
}

size_t VoxelMemoryPool::debug_get_used_memory() const {
	//MutexLock lock(_mutex);
	return _used_memory;
}

size_t VoxelMemoryPool::debug_get_total_memory() const {
	//MutexLock lock(_mutex);
	return _total_memory;
}
