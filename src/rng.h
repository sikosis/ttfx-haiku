#ifndef TTFX_RNG_H
#define TTFX_RNG_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace ttfx {

class Rng {
public:
	explicit Rng(std::uint64_t seed);
	static Rng from_entropy();

	std::uint64_t next_u64();
	double random();
	std::uint64_t below(std::uint64_t upper_bound);
	int integer(int first, int last);
	double uniform(double first, double last);

	template<typename T>
	void shuffle(std::vector<T>& values)
	{
		for (std::size_t index = values.size(); index > 1; --index) {
			const std::size_t other = static_cast<std::size_t>(below(index));
			std::swap(values[index - 1], values[other]);
		}
	}

private:
	std::uint64_t state_[4];
};

} // namespace ttfx

#endif
