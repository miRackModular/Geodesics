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
extern Model *modelBlankLogo;
extern Model *modelBlankInfo;



// General constants
static const bool clockIgnoreOnRun = false;
//static const bool retrigGatesOnReset = true; no need yet, since no geodesic sequencers emit gates
static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
static const float lightLambda = 0.075f;
static const std::string lightPanelID = "White light edition";
static const std::string darkPanelID = "Dark matter edition";
static const unsigned int displayRefreshStepSkips = 256;
static const unsigned int userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
// above value should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers

static const float blurRadiusRatio = 0.06f;


// Variations on existing knobs, lights, etc


// Ports

struct GeoPort : DynamicSVGPort {
	GeoPort() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Jack-WL.svg")));
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
	GeoPushButton() {// only one skin for now
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/PushButton1_0.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/PushButton1_1.svg")));
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
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Knob-WL.svg")));
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
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/C-WL.svg")));
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
	
	void drawLight(const DrawArgs &args) override { // from app/LightWidget.cpp (only nvgStrokeWidth of border was changed)
		float radius = box.size.x / 2.0;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, radius, radius, radius);

		// Background
		if (bgColor.a > 0.0) {
			nvgFillColor(args.vg, bgColor);
			nvgFill(args.vg);
		}

		// Foreground
		if (color.a > 0.0) {
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);
		}

		// Border
		if (borderColor.a > 0.0) {
			nvgStrokeWidth(args.vg, 1.0);//0.5);
			nvgStrokeColor(args.vg, borderColor);
			nvgStroke(args.vg);
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
struct GeoBlueYellowWhiteLight : GeoGrayModuleLight {
	GeoBlueYellowWhiteLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
		addBaseColor(SCHEME_WHITE);
	}
};struct GeoBlueYellowRedWhiteLight : GeoGrayModuleLight {
	GeoBlueYellowRedWhiteLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
		addBaseColor(SCHEME_RED);
		addBaseColor(SCHEME_WHITE);
	}
};
struct GeoBlueYellowLight : GeoGrayModuleLight {
	GeoBlueYellowLight() {
		addBaseColor(SCHEME_BLUE);
		addBaseColor(SCHEME_YELLOW);
	}
};


// Other

struct Trigger : dsp::SchmittTrigger {
	// implements a 0.1V - 1.0V SchmittTrigger (include/dsp/digital.hpp) instead of 
	//   calling SchmittTriggerInstance.process(math::rescale(in, 0.1f, 1.f, 0.f, 1.f))
	bool process(float in);
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

#endif
