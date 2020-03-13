//***********************************************************************************************
//Geodesics: A modular collection for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//  and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#ifndef GEODESICS_HPP
#define GEODESICS_HPP

#include "rack.hpp"
#include "GeoWidgets.hpp"


using namespace rack;


extern Plugin *pluginInstance;


// All modules that are part of plugin go here
extern Model *modelBlackHoles;
extern Model *modelPulsars;
extern Model *modelBranes;
extern Model *modelIons;
extern Model *modelEntropia;
extern Model *modelEnergy;
extern Model *modelTorus;
extern Model *modelFate;
extern Model *modelBlankLogo;
extern Model *modelBlankInfo;



// General constants
//static const bool retrigGatesOnReset = true; no need yet, since no geodesic sequencers emit gates
static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
static const std::string lightPanelID = "White light edition";
static const std::string darkPanelID = "Dark matter edition";

static const float blurRadiusRatio = 0.06f;


// Variations on existing knobs, lights, etc


// Ports

struct GeoPort : DynamicSVGPort {
	GeoPort() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Jack-DM.svg")));
		shadow->blurRadius = 1.0f;
	}
};

struct BlankPort : SvgPort {
	BlankPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/general/Otrsp-01.svg")));
		shadow->opacity = 0.0;
	}
};


// Buttons and switches

struct GeoPushButton : DynamicSVGSwitch {
	GeoPushButton() {
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/PushButton1_0.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/PushButton1_1.svg")));
	}
};



// Knobs

struct GeoKnob : DynamicSVGKnob {
	GeoKnob() {
		minAngle = -0.73*M_PI;
		maxAngle = 0.73*M_PI;
		//shadow->box.pos = Vec(0.0, box.size.y * 0.15); may need this if knob is small (taken from IMSmallKnob)
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Knob-DM.svg")));
		shadow->blurRadius = box.size.y * blurRadiusRatio;
	}
};

struct GeoKnobTopRight : GeoKnob {
	GeoKnobTopRight() {setOrientation(M_PI / 4.0f);}
};
struct GeoKnobRight : GeoKnob {
	GeoKnobRight() {setOrientation(M_PI / 2.0f);}
};
struct GeoKnobBotRight : GeoKnob {
	GeoKnobBotRight() {setOrientation(3.0f * M_PI / 4.0f);}
};
struct GeoKnobBottom : GeoKnob {
	GeoKnobBottom() {setOrientation(M_PI);}
};
struct GeoKnobBotLeft : GeoKnob {
	GeoKnobBotLeft() {setOrientation(5.0f * M_PI / 4.0f);}
};
struct GeoKnobLeft : GeoKnob {
	GeoKnobLeft() {setOrientation(M_PI / -2.0f);}
};
struct GeoKnobTopLeft : GeoKnob {
	GeoKnobTopLeft() {setOrientation(M_PI / -4.0f);}
};


struct BlankCKnob : DynamicSVGKnob {
	BlankCKnob() {
		minAngle = -0.73*M_PI;
		maxAngle = 0.73*M_PI;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/C-DM.svg")));
		shadow->opacity = 0.0;
	}
};



// Lights

struct GeoGrayModuleLight : ModuleLightWidget {
	GeoGrayModuleLight() {
		bgColor = nvgRGB(0x8e, 0x8e, 0x8e);
		borderColor = nvgRGB(0x1d, 0x1d, 0x1b);//nvgRGBA(0, 0, 0, 0x60);
	}
	
	void drawLight(NVGcontext *vg) override { // from app/LightWidget.cpp (only nvgStrokeWidth of border was changed)
		float radius = box.size.x / 2.0;

		nvgBeginPath(vg);
		nvgCircle(vg, radius, radius, radius);

		// Background
		if (bgColor.a > 0.0) {
			nvgFillColor(vg, bgColor);
			nvgFill(vg);
		}

		// Foreground
		if (color.a > 0.0) {
			nvgFillColor(vg, color);
			nvgFill(vg);
		}

		// Border
		if (borderColor.a > 0.0) {
			nvgStrokeWidth(vg, 1.0);//0.5);
			nvgStrokeColor(vg, borderColor);
			nvgStroke(vg);
		}
	}
};

struct GeoWhiteLight : GeoGrayModuleLight {
	GeoWhiteLight() {
		addBaseColor(SCHEME_WHITE);
	}
};
struct GeoBlueLight : GeoGrayModuleLight {
	GeoBlueLight() {
		addBaseColor(SCHEME_BLUE);
	}
};
struct GeoRedLight : GeoGrayModuleLight {
	GeoRedLight() {
		addBaseColor(SCHEME_RED);
	}
};
struct GeoYellowLight : GeoGrayModuleLight {
	GeoYellowLight() {
		addBaseColor(SCHEME_YELLOW);
	}
};

struct GeoWhiteRedLight : GeoGrayModuleLight {
	GeoWhiteRedLight() {
		addBaseColor(SCHEME_WHITE);
		addBaseColor(SCHEME_RED);
	}
};
struct GeoWhiteBlueLight : GeoGrayModuleLight {
	GeoWhiteBlueLight() {
		addBaseColor(SCHEME_WHITE);
		addBaseColor(SCHEME_BLUE);
	}
};
struct GeoBlueYellowLight : GeoGrayModuleLight {
	GeoBlueYellowLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
	}
};

struct GeoBlueYellowWhiteLight : GeoGrayModuleLight {
	GeoBlueYellowWhiteLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
		addBaseColor(SCHEME_WHITE);
	}
};

struct GeoBlueYellowRedWhiteLight : GeoGrayModuleLight {
	GeoBlueYellowRedWhiteLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
		addBaseColor(SCHEME_RED);
		addBaseColor(SCHEME_WHITE);
	}
};


// Other

struct RefreshCounter {
	static const unsigned int displayRefreshStepSkips = 256;
	static const unsigned int userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
	// above value should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers
	
	unsigned int refreshCounter = (random::u32() % displayRefreshStepSkips);// stagger start values to avoid processing peaks when many Geo and Impromptu modules in the patch
	
	bool processInputs() {
		return ((refreshCounter & userInputsStepSkipMask) == 0);
	}
	bool processLights() {// this must be called even if module has no lights, since counter is decremented here
		refreshCounter++;
		bool process = refreshCounter >= displayRefreshStepSkips;
		if (process) {
			refreshCounter = 0;
		}
		return process;
	}
};


struct Trigger : dsp::SchmittTrigger {
	// implements a 0.1V - 1.0V SchmittTrigger (include/dsp/digital.hpp) instead of 
	//   calling SchmittTriggerInstance.process(math::rescale(in, 0.1f, 1.f, 0.f, 1.f))
	bool process(float in);
};	


// http://www.earlevel.com/main/2012/12/15/a-one-pole-filter/
// A one-pole filter
// Posted on December 15, 2012 by Nigel Redmon
// Adapted by Marc Boulé
struct OnePoleFilter {
    float b1 = 0.0f;
	float lowout = 0.0f;
	// float lastin = 0.0f;
	
    void setCutoff(float Fc) {
		b1 = std::exp(-2.0f * M_PI * Fc);
	}
    float process(float in) {
		// lastin = in;
		return lowout = in * (1.0f - b1) + lowout * b1;
	}
	// float lowpass() {
		// return lowout;
	// }
	// float highpass() {
		// return lastin - lowout;
	// }
};


struct HoldDetect {
	long modeHoldDetect;// 0 when not detecting, downward counter when detecting
	
	void reset() {
		modeHoldDetect = 0l;
	}
	
	void start(long startValue) {
		modeHoldDetect = startValue;
	}

	bool process(float paramValue);
};

int getWeighted1to8random();


void saveDarkAsDefault(bool darkAsDefault);
bool loadDarkAsDefault();

struct DarkDefaultItem : MenuItem {
	void onAction(event::Action &e) override {
		saveDarkAsDefault(rightText.empty());// implicitly toggled
	}
};	


#endif
