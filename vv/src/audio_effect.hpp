#pragma once
#include "edit_controller.hpp"
#include "processor.hpp"
#include <public.sdk/source/vst/vstaudioeffect.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/base/ibstream.h>
#include <boost/circular_buffer.hpp>
#include <vector>

namespace vv
{

	class audio_effect : public Steinberg::Vst::AudioEffect
	{
	public:

		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override
		{
			auto result = Steinberg::Vst::AudioEffect::initialize(context);
			if (result != Steinberg::kResultOk)
				return result;

			this->addAudioInput(STR16("AudioInput"), Steinberg::Vst::SpeakerArr::kMono);
			this->addAudioOutput(STR16("AudioOutput"), Steinberg::Vst::SpeakerArr::kMono);
			
			return Steinberg::kResultOk;
		}

		Steinberg::tresult PLUGIN_API setBusArrangements(
			Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
			Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override
		{
			if (numIns != 1 || numOuts != 1 || inputs[0] != Steinberg::Vst::SpeakerArr::kMono || outputs[0] != Steinberg::Vst::SpeakerArr::kMono)
				return Steinberg::kResultFalse;

			return Steinberg::Vst::AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
		}

		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override
		{
			Steinberg::tresult ret;

			ret = state->read(&pitch_shift_raw_, sizeof(pitch_shift_raw_));
			if (ret != Steinberg::kResultOk)
				return ret;

			ret = state->read(&formant_shift_raw_, sizeof(formant_shift_raw_));
			if (ret != Steinberg::kResultOk)
				return ret;

			return Steinberg::kResultOk;
		}

		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override
		{
			Steinberg::tresult ret;

			ret = state->write(&pitch_shift_raw_, sizeof(pitch_shift_raw_));
			if (ret != Steinberg::kResultOk)
				return ret;
			
			ret = state->write(&formant_shift_raw_, sizeof(formant_shift_raw_));
			if (ret != Steinberg::kResultOk)
				return ret;

			return Steinberg::kResultOk;
		}

		Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override
		{
			try
			{
				processor_ = std::make_unique<processor>(setup.sampleRate);
			}
			catch (...)
			{
				return Steinberg::kResultFalse;
			}

			return Steinberg::kResultOk;
		}

		Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override
		{
			if (!processor_)
				return Steinberg::kResultFalse;

			if (data.inputParameterChanges)
			{
				auto count = data.inputParameterChanges->getParameterCount();

				for (Steinberg::int32 i = 0; i < count; ++i)
				{
					if (auto queue = data.inputParameterChanges->getParameterData(i))
					{
						auto tag = queue->getParameterId();

						Steinberg::int32 sampleOffset;
						Steinberg::Vst::ParamValue value;

						if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, value) != Steinberg::kResultOk)
							continue;

						switch (tag)
						{
						case edit_controller::pitch_tag:
							pitch_shift_raw_ = value;
							break;
						case edit_controller::formant_tag:
							formant_shift_raw_ = value;
							break;
						}
					}
				}
			}

			auto in = data.inputs[0].channelBuffers32[0];
			auto out = data.outputs[0].channelBuffers32[0];

			std::size_t remain = data.numSamples;

			while (remain != 0)
			{
				if (in_buffer_.full())
				{
					std::copy(in_buffer_.begin(), in_buffer_.end(), temp_input_.begin());

					auto pitch_shift = std::pow(2.0, (pitch_shift_raw_ - 0.5) * 2.0);
					auto formant_shift = std::pow(2.0, (formant_shift_raw_ - 0.5) * 2.0);

					(*processor_)(temp_input_.data(), temp_output_.data(), pitch_shift, formant_shift);

					out_buffer_.assign(temp_output_.begin(), temp_output_.end());
					in_buffer_.clear();
				}

				auto size = std::min(processor::buffer_size - in_buffer_.size(), remain);

				std::copy(in, in + size, std::back_inserter(in_buffer_));
				std::copy(out_buffer_.begin(), out_buffer_.begin() + size, out);

				out_buffer_.rresize(out_buffer_.size() - size);

				in += size;
				out += size;
				remain -= size;
			}

			return Steinberg::kResultOk;
		}

		static Steinberg::FUnknown* create_instance(void*)
		{
			return static_cast<Steinberg::Vst::IAudioProcessor*>(new audio_effect());
		}

	private:

		audio_effect()
			: in_buffer_(processor::buffer_size)
			, out_buffer_(processor::buffer_size)
			, temp_input_(processor::buffer_size)
			, temp_output_(processor::buffer_size)
		{
			for (std::size_t i = 0; i < processor::buffer_size; ++i)
				in_buffer_.push_back(0.0f);

			this->setControllerClass(edit_controller_uid);
		}

		virtual ~audio_effect()
		{
		}

		boost::circular_buffer<float> in_buffer_;
		boost::circular_buffer<float> out_buffer_;
		std::vector<float> temp_input_;
		std::vector<float> temp_output_;

		double pitch_shift_raw_ = 0.5;
		double formant_shift_raw_ = 0.5;

		std::unique_ptr<processor> processor_;

	};

	static const Steinberg::FUID audio_effect_uid(0x316C3BD0, 0xF6A24644, 0xACD3929A, 0xC46973E5);

}
