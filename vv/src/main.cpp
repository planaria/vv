#include "audio_effect.hpp"
#include "edit_controller.hpp"
#include <public.sdk/source/main/pluginfactoryvst3.h>

BEGIN_FACTORY_DEF("suzukikojiro", "https://twitter.com/suzukikojiro", "https://twitter.com/suzukikojiro")

	DEF_CLASS2(
		INLINE_UID_FROM_FUID(::vv::audio_effect_uid),
		PClassInfo::kManyInstances,
		kVstAudioEffectClass,
		"vv pitch shift",
		Vst::kDistributable,
		Vst::PlugType::kFxPitchShift,
		"1.0.0.0",
		kVstVersionString,
		::vv::audio_effect::create_instance)

	DEF_CLASS2(
		INLINE_UID_FROM_FUID(::vv::edit_controller_uid),
		PClassInfo::kManyInstances,
		kVstComponentControllerClass,
		"vv pitch shift controller",
		0,
		"",
		"1.0.0.0",
		kVstVersionString,
		::vv::edit_controller::create_instance)

END_FACTORY

bool InitModule()
{
	return true;
}

bool DeinitModule()
{
	return true;
}
