#include <processor.hpp>
#include <boost/math/constants/constants.hpp>
#include <cmath>

int main()
{
	vv::processor p(44100.0);

	std::vector<float> input(vv::processor::buffer_size);

	for (std::size_t i = 0; i < vv::processor::buffer_size; ++i)
	{
		auto ratio = static_cast<double>(i) / static_cast<double>(vv::processor::buffer_size);
		auto v = std::sin(10.0 * ratio * boost::math::constants::two_pi<double>());

		input[i] = static_cast<float>(v);
	}

	std::vector<float> output(vv::processor::buffer_size);

	p(input.data(), output.data(), 1.5, 1.2);
}
