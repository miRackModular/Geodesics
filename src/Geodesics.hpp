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
#include "dsp/digital.hpp"

using namespace rack;


extern Plugin *plugin;

// All modules that are part of plugin go here
extern Model *modelBlackHoles;
extern Model *modelPulsars;
extern Model *modelBranes;
extern Model *modelIons;
extern Model *modelBlankLogo;
extern Model *modelBlankInfo;



// General constants
static const float lightLambda = 0.075f;
static const std::string lightPanelID = "White light edition";
static const std::string darkPanelID = "Dark matter edition";
static const unsigned int displayRefreshStepSkips = 256;
static const unsigned int userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
// above value should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers



// Variations on existing knobs, lights, etc


// Ports

struct GeoPort : DynamicSVGPort {
	GeoPort() {
		shadow->blurRadius = 10.0;
		shadow->opacity = 0.8;
		addFrame(SVG::load(assetPlugin(plugin, "res/WhiteLight/Jack-WL.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/DarkMatter/Jack-DM.svg")));
	}
};

struct BlankPort : SVGPort {
	BlankPort() {
		shadow->opacity = 0.0;
		setSVG(SVG::load(assetPlugin(plugin, "res/general/Otrsp-01.svg")));
	}
};


// Buttons and switches

struct GeoPushButton : DynamicSVGSwitch, MomentarySwitch {
	GeoPushButton() {// only one skin for now
		addFrameAll(SVG::load(assetPlugin(plugin, "res/general/PushButton1_0.svg")));
		addFrameAll(SVG::load(assetPlugin(plugin, "res/general/PushButton1_1.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_0.svg"))); // no dark psuhbutton in Geodesics for now
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_1.svg"))); // no dark psuhbutton in Geodesics for now
	}
};



// Knobs

struct GeoKnob : DynamicSVGKnob {
	GeoKnob() {
		minAngle = -0.73*M_PI;
		maxAngle = 0.73*M_PI;
		shadow->blurRadius = 10.0;
		shadow->opacity = 0.8;
		//shadow->box.pos = Vec(0.0, box.size.y * 0.15); may need this if know is small (taken from IMSmallKnob)
		addFrameAll(SVG::load(assetPlugin(plugin, "res/WhiteLight/Knob-WL.svg")));
		addFrameAll(SVG::load(assetPlugin(plugin, "res/DarkMatter/Knob-DM.svg")));
	}
};

struct GeoKnobRight : GeoKnob {
	GeoKnobRight() {
		orientationAngle = M_PI / 2.0f;
	}
};
struct GeoKnobLeft : GeoKnob {
	GeoKnobLeft() {
		orientationAngle = M_PI / -2.0f;
	}
};
struct GeoKnobBottom : GeoKnob {
	GeoKnobBottom() {
		orientationAngle = M_PI;
	}
};


struct BlankCKnob : DynamicSVGKnob {
	BlankCKnob() {
		minAngle = -0.73*M_PI;
		maxAngle = 0.73*M_PI;
		shadow->opacity = 0.0;
		addFrameAll(SVG::load(assetPlugin(plugin, "res/WhiteLight/C-WL.svg")));
		addFrameAll(SVG::load(assetPlugin(plugin, "res/DarkMatter/C-DM.svg")));
	}
};



// Lights

struct GeoGrayModuleLight : ModuleLightWidget {
	GeoGrayModuleLight() {
		bgColor = nvgRGB(0x8e, 0x8e, 0x8e);
		borderColor = nvgRGB(0x1d, 0x1d, 0x1b);//nvgRGBA(0, 0, 0, 0x60);
	}
	
	void drawLight(NVGcontext *vg) override { // from LightWidget.cpp (only nvgStrokeWidth of border was changed)
		float radius = box.size.x / 2.0;

		nvgBeginPath(vg);
		nvgCircle(vg, radius, radius, radius);

		// Background
		nvgFillColor(vg, bgColor);
		nvgFill(vg);

		// Foreground
		nvgFillColor(vg, color);
		nvgFill(vg);

		// Border
		nvgStrokeWidth(vg, 1.0);// 0.5);
		nvgStrokeColor(vg, borderColor);
		nvgStroke(vg);
	}
	
};

struct GeoWhiteLight : GeoGrayModuleLight {
	GeoWhiteLight() {
		addBaseColor(COLOR_WHITE);
	}
};
struct GeoBlueLight : GeoGrayModuleLight {
	GeoBlueLight() {
		addBaseColor(COLOR_BLUE);
	}
};
struct GeoRedLight : GeoGrayModuleLight {
	GeoRedLight() {
		addBaseColor(COLOR_RED);
	}
};
struct GeoYellowLight : GeoGrayModuleLight {
	GeoYellowLight() {
		addBaseColor(COLOR_YELLOW);
	}
};
struct GeoWhiteRedLight : GeoGrayModuleLight {
	GeoWhiteRedLight() {
		addBaseColor(COLOR_WHITE);
		addBaseColor(COLOR_RED);
	}
};struct GeoBlueYellowWhiteLight : GeoGrayModuleLight {
	GeoBlueYellowWhiteLight() {
		addBaseColor(COLOR_BLUE);
		addBaseColor(COLOR_YELLOW);
		addBaseColor(COLOR_WHITE);
	}
};


// Other

#endif
