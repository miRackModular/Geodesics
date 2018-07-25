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



// General constants
static const std::string lightPanelID = "Light";
static const std::string darkPanelID = "Dark";



// Variations on existing knobs, lights, etc


// Ports

struct GeoPort : DynamicSVGPort {
	GeoPort() {
		shadow->blurRadius = 10.0;
		shadow->opacity = 0.8;
		addFrame(SVG::load(assetPlugin(plugin, "res/light/comp/Jack.svg")));
		//addFrame(SVG::load(assetPlugin(plugin, "res/dark/comp/Jack.svg")));// no dark ports in Geodesics for now
	}
};


// Buttons and switches

struct GeoPushButton : DynamicSVGSwitch, MomentarySwitch {
	GeoPushButton() {// only one skin for now
		addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/PushButton1_0.svg")));
		addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/PushButton1_1.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_0.svg"))); // no dark buttons in Geodesics for now
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_1.svg"))); // no dark buttons in Geodesics for now
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
		addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/Knob.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/Knob.svg")));// no dark knobs in Geodesics for now
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



// Lights

struct OrangeLight : GrayModuleLightWidget {
	OrangeLight() {
		addBaseColor(COLOR_ORANGE);
	}
};
struct WhiteLight : GrayModuleLightWidget {
	WhiteLight() {
		addBaseColor(COLOR_WHITE);
	}
};



// Other

#endif
