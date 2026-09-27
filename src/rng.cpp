#include "rng.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace ttfx {
namespace {

std::uint64_t rotate_left(std::uint64_t value, int shift)
{
	return (value << shift) | (value >> (64 - shift));
}

std::uint64_t splitmix64(std::uint64_t& state)
{
	state += UINT64_C(0x9e3779b97f4a7c15);
	std::uint64_t value = state;
	value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
	return value ^ (value >> 31);
}

} // namespace

Rng::Rng(std::uint64_t seed)
{
	for (std::uint64_t& value : state_)
		value = splitmix64(seed);
}

Rng Rng::from_entropy()
{
	std::uint64_t seed = 0;
	std::ifstream random("/dev/urandom", std::ios::binary);
	if (random.read(reinterpret_cast<char*>(&seed), sizeof(seed)))
		return Rng(seed);
	seed = static_cast<std::uint64_t>(
		std::chrono::high_resolution_clock::now().time_since_epoch().count());
	return Rng(seed);
}

std::uint64_t Rng::next_u64()
{
	const std::uint64_t result = rotate_left(state_[0] + state_[3], 23) + state_[0];
	const std::uint64_t temporary = state_[1] << 17;
	state_[2] ^= state_[0];
	state_[3] ^= state_[1];
	state_[1] ^= state_[2];
	state_[0] ^= state_[3];
	state_[2] ^= temporary;
	state_[3] = rotate_left(state_[3], 45);
	return result;
}

double Rng::random()
{
	return static_cast<double>(next_u64() >> 11)
		* (1.0 / static_cast<double>(UINT64_C(1) << 53));
}

std::uint64_t Rng::below(std::uint64_t upper_bound)
{
	if (upper_bound == 0)
		throw std::invalid_argument("random upper bound is zero");
	unsigned bits = 0;
	for (std::uint64_t value = upper_bound - 1; value != 0; value >>= 1)
		++bits;
	bits = std::max(bits, 1u);
	for (;;) {
		const std::uint64_t value = next_u64() >> (64 - bits);
		if (value < upper_bound)
			return value;
	}
}

int Rng::integer(int first, int last)
{
	if (last < first)
		throw std::invalid_argument("empty random integer range");
	return first + static_cast<int>(below(static_cast<std::uint64_t>(last - first) + 1));
}

double Rng::uniform(double first, double last)
{
	return first + (last - first) * random();
}

} // namespace ttfx
