//***********************************************************************************************
//Blank-Panel Logo for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "Geodesics.hpp"


// From Fundamental LFO.cpp
struct LowFrequencyOscillator {
	float phase = 0.0f;
	float freq = 1.0f;

	LowFrequencyOscillator() {}
	void setPitch(float pitch) {
		pitch = std::fmin(pitch, 8.0f);
		freq = std::pow(2.0f, pitch);
	}
	void step(float dt) {
		float deltaPhase = std::fmin(freq * dt, 0.5f);
		phase += deltaPhase;
		if (phase >= 1.0f)
			phase -= 1.0f;
	}
	float sqr() {
		return (phase < 0.5f) ? 2.0f : 0.0f;
	}
};


//*****************************************************************************


struct BlankLogo : Module {
	enum ParamIds {
		CLK_FREQ_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	
	// Constants
	const float song[5] = {7.0f/12.0f, 9.0f/12.0f, 5.0f/12.0f, 5.0f/12.0f - 1.0f, 0.0f/12.0f};


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	float clkValue;
	int stepIndex;
	
	// No need to save, no reset
	LowFrequencyOscillator oscillatorClk;
	Trigger clkTrigger;
	
	
	BlankLogo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CLK_FREQ_PARAM, -2.0f, 4.0f, 1.0f, "CLK freq", " BPM", 2.0f, 60.0f);// 120 BMP when default value  (120 = 60*2^1) diplay params 

		clkTrigger.reset();
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}


	void onReset() override {	
		resetNonJson();
	}
	void resetNonJson() {
		clkValue = 0.0f;
		stepIndex = 0;
	}
	

	void onRandomize() override {
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (outputs[OUT_OUTPUT].isConnected()) {
			// CLK
			oscillatorClk.setPitch(params[CLK_FREQ_PARAM].getValue());
			oscillatorClk.step(args.sampleTime);
			float clkValue = oscillatorClk.sqr();	
			
			if (clkTrigger.process(clkValue)) {
				stepIndex++;
				if (stepIndex >= 5)
					stepIndex = 0;
				outputs[OUT_OUTPUT].setVoltage(song[stepIndex]);
			}
		}
	}
};


struct BlankLogoWidget : ModuleWidget {
	BlankLogoWidget(BlankLogo *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/BlankLogo-DM.svg")));
		
		// Screws
		// part of svg panel, no code required
		
		addParam(createDynamicParam<BlankCKnob>(Vec(29.5f,74.2f), module, BlankLogo::CLK_FREQ_PARAM, module ? &module->panelTheme : NULL));// 120 BMP when default value
		addOutput(createOutputCentered<BlankPort>(Vec(29.5f,187.5f), module, BlankLogo::OUT_OUTPUT));	
	}
};

Model *modelBlankLogo = createModel<BlankLogo, BlankLogoWidget>("Blank-PanelLogo");

/*CHANGE LOG

1.0.0:
slug changed from "Blank-Panel Logo" to "Blank-PanelLogo"

*/

