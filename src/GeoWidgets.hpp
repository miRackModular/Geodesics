//***********************************************************************************************
//Geodesics: A modular collection for VCV Rack by Pierre Collard and Marc Boulé
//
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#ifndef IM_WIDGETS_HPP
#define IM_WIDGETS_HPP

#include "rack.hpp"

using namespace rack;



// ******** Dynamic Ports ********

// General Dynamic Port creation
template <class TDynamicPort>
TDynamicPort* createDynamicPort(Vec pos, bool isInput, Module *module, int portId, int* mode) {
	TDynamicPort *dynPort = isInput ? 
		createInputCentered<TDynamicPort>(pos, module, portId) :
		createOutputCentered<TDynamicPort>(pos, module, portId);
	dynPort->mode = mode;
	return dynPort;
}

struct DynamicSVGPort : SvgPort {
    int* mode = NULL;
    int oldMode = -1;
    std::vector<std::shared_ptr<Svg>> frames;

    void addFrame(std::shared_ptr<Svg> svg);
    void step() override;
};



// ******** Dynamic Params ********

template <class TDynamicParam>
TDynamicParam* createDynamicParam(Vec pos, Module *module, int paramId, int* mode) {
	TDynamicParam *dynParam = createParamCentered<TDynamicParam>(pos, module, paramId);
	dynParam->mode = mode;
	return dynParam;
}

struct DynamicSVGSwitch : SvgSwitch {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	
	void addFrameAll(std::shared_ptr<Svg> svg);
    void step() override;
};

struct DynamicSVGKnob : SvgKnob {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	
	void setOrientation(float angle);
	void addFrameAll(std::shared_ptr<Svg> svg);
	void addEffect(std::shared_ptr<Svg> svg);// do this last
    void step() override;
};


#endif