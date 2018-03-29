#pragma once
#include <public.sdk/source/vst/vsteditcontroller.h>

namespace vv
{

	class edit_controller : public Steinberg::Vst::EditController
	{
	public:

		static const int pitch_tag = 1;
		static const int formant_tag = 2;

		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override
		{
			auto result = Steinberg::Vst::EditController::initialize(context);
			if (result != Steinberg::kResultOk)
				return result;

			this->parameters.addParameter(STR16("Pitch"), STR16(""), 0, 0.5, Steinberg::Vst::ParameterInfo::kCanAutomate, pitch_tag);
			this->parameters.addParameter(STR16("Formant"), STR16(""), 0, 0.5, Steinberg::Vst::ParameterInfo::kCanAutomate, formant_tag);

			return Steinberg::kResultOk;
		}

		static Steinberg::FUnknown* create_instance(void*)
		{
			return static_cast<Steinberg::Vst::IEditController*>(new edit_controller());
		}

	};

	static const Steinberg::FUID edit_controller_uid(0xBB1484A5, 0x2F50482B, 0xA5761149, 0x01A1E1A9);

}
