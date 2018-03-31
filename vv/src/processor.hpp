#pragma once
#include "kissfft.hh"
#include <boost/math/constants/constants.hpp>
#include <boost/optional.hpp>
#include <cstddef>

namespace vv
{

	template <class T, class U>
	auto lerp(T x0, T x1, U ratio)
	{
		return x0 + (x1 - x0) * ratio;
	}

	class processor
	{
	public:

		static const std::size_t buffer_size = 4096;
		static const std::size_t nsdf_size = buffer_size / 2;

		explicit processor(double sampleRate)
			: sampleRate_(sampleRate)
			, v1_(buffer_size)
			, v2_(buffer_size + nsdf_size)
			, v3_(buffer_size + nsdf_size)
			, v4_(buffer_size + nsdf_size)
			, v5_(buffer_size + nsdf_size)
			, v6_(buffer_size)
		{
		}

		void operator ()(const float* input, float* output, double pitch_shift, double formant_shift)
		{
			for (std::size_t i = 0; i < buffer_size; ++i)
				v1_[i] = std::complex<float>(input[i], 0.0f);

			for (std::size_t i = 0; i < buffer_size; ++i)
			{
				auto r = static_cast<float>(i) / static_cast<float>(buffer_size);
				auto w = 0.5f - 0.5f * std::cos(boost::math::constants::two_pi<float>() * r);
				v2_[i] = v1_[i] * w;
			}

			fft_.transform(v2_.data(), v3_.data());

			for (std::size_t i = 0; i < buffer_size + nsdf_size; ++i)
				v4_[i] = std::norm(v3_[i]);

			ifft_.transform(v4_.data(), v5_.data());

			float m = 0.0;

			for (std::size_t i = 0; i < buffer_size; ++i)
			{
				auto j = buffer_size - i - 1;

				auto x1 = v2_[i].real();
				m += x1 * x1;

				auto x2 = v2_[j].real();
				m += x2 * x2;

				if (m < std::numeric_limits<double>::min())
					v6_[j] = 0.0f;
				else
					v6_[j] = 2.0f * v5_[j].real() / m;
			}

			auto minimum_hz = 50.0;
			auto maximum_hz = 1000.0;

			auto minimum_index = static_cast<std::size_t>(std::round(sampleRate_ / maximum_hz));
			auto maximum_index = static_cast<std::size_t>(std::round(sampleRate_ / minimum_hz));

			double maximum_value = 0.0;

			for (std::size_t i = minimum_index; i < maximum_index; ++i)
			{
				auto p1 = v6_[i - 1];
				auto p2 = v6_[i];
				auto p3 = v6_[i + 1];

				if (p1 < p2 && p2 > p3 && p2 > maximum_value)
					maximum_value = p2;
			}

			boost::optional<std::size_t> peak_index;
			double peak_value = 0.0;

			for (std::size_t i = minimum_index; i < maximum_index; ++i)
			{
				auto p1 = v6_[i - 1];
				auto p2 = v6_[i];
				auto p3 = v6_[i + 1];

				if (p1 < p2 && p2 > p3 && p2 > maximum_value * 0.95)
				{
					peak_index = i;
					peak_value = p2;
					break;
				}
			}

			bool enable = false;

			if (peak_index)
			{
				auto virtual_peak_index = *peak_index;

				std::size_t last_dst = 0;
				std::size_t last_src1 = 0;
				std::size_t last_src2 = 0;
				double last_src_ratio = 0.0;

				auto cubed = [](double x)
				{
					return x * x * x;
				};

				auto interpolate = [&](double x1, double x2, double ratio)
				{
					if (ratio < 0.5)
						return lerp(x1, x2, cubed(ratio) * 4.0);
					else
						return lerp(x1, x2, 1.0 - cubed(1.0 - ratio) * 4.0);
				};

				auto get_value = [&](double indexf) -> double
				{
					if (indexf < 0.0)
						return input[0];

					auto index = static_cast<std::size_t>(std::floor(indexf));
					if (index >= buffer_size - 1)
						return input[buffer_size - 1];

					auto ratio = indexf - std::floor(indexf);

					return interpolate(input[index], input[index + 1], ratio);
				};

				auto overlap = [&](std::size_t dst, std::size_t src1, std::size_t src2, double src_ratio)
				{
					for (std::size_t i = last_dst; i < dst; ++i)
					{
						auto ratio = static_cast<double>(i - last_dst) / static_cast<double>(dst - last_dst);

						auto p1_1 = get_value(static_cast<double>(last_src1) + static_cast<double>(i - last_dst) * formant_shift);
						auto p1_2 = get_value(static_cast<double>(last_src2) + static_cast<double>(i - last_dst) * formant_shift);
						auto p1 = interpolate(p1_1, p1_2, last_src_ratio);

						auto p2_1 = get_value(static_cast<double>(src1) - static_cast<double>(dst - i) * formant_shift);
						auto p2_2 = get_value(static_cast<double>(src2) - static_cast<double>(dst - i) * formant_shift);
						auto p2 = interpolate(p2_1, p2_2, src_ratio);

						output[i] = static_cast<float>(interpolate(p1, p2, ratio));
					}

					last_dst = dst;
					last_src1 = src1;
					last_src2 = src2;
					last_src_ratio = src_ratio;
				};

				auto q = buffer_size / virtual_peak_index;
				auto r = buffer_size % virtual_peak_index;

				auto nf = (static_cast<double>(buffer_size) * pitch_shift - static_cast<double>(r)) / static_cast<double>(virtual_peak_index);
				auto n = static_cast<std::size_t>(std::max(0.0, std::round(nf)));

				if (q != 0 && n != 0)
				{
					auto actual_pitch_shift = static_cast<double>(n * virtual_peak_index + r) / static_cast<double>(buffer_size);

					for (std::size_t i = 1; i <= n; ++i)
					{
						double frame_indexf = 1.0;

						if (n != 1)
							frame_indexf = static_cast<double>((i - 1) * (q - 1)) / static_cast<double>(n - 1) + 1;

						auto frame_index = static_cast<std::size_t>(std::floor(frame_indexf));

						auto dst = static_cast<std::size_t>(std::floor(static_cast<double>(i * virtual_peak_index) / actual_pitch_shift));
						auto src = frame_index * virtual_peak_index;

						if (frame_index == q)
						{
							overlap(dst, src, src, 0.0);
						}
						else
						{
							auto src_ratio = frame_indexf - std::floor(frame_indexf);
							overlap(dst, src, src + virtual_peak_index, src_ratio);
						}
					}

					overlap(buffer_size, buffer_size, buffer_size, 0.0);

					enable = true;
				}
			}

			if (!enable)
				std::copy(input, input + buffer_size, output);
		}

	private:

		double sampleRate_;

		kissfft<float> fft_{ buffer_size + nsdf_size, false };
		kissfft<float> ifft_{ buffer_size + nsdf_size, true };

		std::vector<std::complex<float>> v1_;
		std::vector<std::complex<float>> v2_;
		std::vector<std::complex<float>> v3_;
		std::vector<std::complex<float>> v4_;
		std::vector<std::complex<float>> v5_;
		std::vector<float> v6_;

	};

}
