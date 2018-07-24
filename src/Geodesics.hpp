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



// General constants
static const std::string lightPanelID = "Classic";
static const std::string darkPanelID = "Dark";



// Variations on existing knobs, lights, etc


// Screws

struct GeoScrew : DynamicSVGScrew {
	GeoScrew() {
		//addSVGalt(SVG::load(assetPlugin(plugin, "res/dark/comp/ScrewSilver.svg"))); // no dark screws in Geodesics
	}
};


// Ports

struct GeoPort : DynamicSVGPort {
	GeoPort() {
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/PJ301M.svg")));
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/PJ301M.svg"))); // no dark ports in Geodesics for now
		shadow->blurRadius = 10.0;
		shadow->opacity = 0.8;
	}
};


// Buttons and switches

struct GeoBigPushButton : DynamicSVGSwitch, MomentarySwitch {
	GeoBigPushButton() {// only one skin for now
		addFrameAll(SVG::load(assetGlobal("res/ComponentLibrary/CKD6_0.svg")));
		addFrameAll(SVG::load(assetGlobal("res/ComponentLibrary/CKD6_1.svg")));
		
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/CKD6b_0.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/CKD6b_1.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_0.svg")));
		//addFrameAll(SVG::load(assetPlugin(plugin, "res/dark/comp/CKD6b_1.svg")));
	}
};



// Knobs

struct GeoKnob : DynamicSVGKnob {
	GeoKnob() {
		minAngle = -0.83*M_PI;
		maxAngle = 0.83*M_PI;
		shadow->blurRadius = 10.0;
		shadow->opacity = 0.8;
		//shadow->box.pos = Vec(0.0, box.size.y * 0.15); may need this if know is small (taken from IMSmallKnob)

		//addFrameAll(SVG::load(assetPlugin(plugin, "res/light/comp/BlackKnobLargeWithMark.svg")));
		addFrameAll(SVG::load(assetGlobal("res/ComponentLibrary/RoundBlackKnob.svg")));
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



// Other

struct ScrewSilverRandomRot : FramebufferWidget {// location: include/app.hpp and src/app/SVGScrew.cpp [some code also from src/app/SVGKnob.cpp]
	SVGWidget *sw;
	TransformWidget *tw;
	ScrewCircle *sc;
	ScrewSilverRandomRot();
};


#endif
