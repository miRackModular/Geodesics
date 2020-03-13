//***********************************************************************************************
//Event modifier module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Fate : Module {
	enum ParamIds {
		FREEWILL_PARAM,
		CHOICESDEPTH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREEWILL_INPUT,// CV_value/10  is added to FREEWILL_PARAM, which is a 0 to 1 knob
		CLOCK_INPUT,// choice trigger
		MAIN_INPUT,// established order
		EXMACHINA_INPUT,
		CHOICSDEPTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(TRIG_LIGHT, 2), // room for WhiteBlue (white is normal unaltered fate trigger, blue is for altered fate trigger)
		NUM_LIGHTS
	};
	
	
	// Constants
	// none
	
	// Need to save
	int panelTheme;
	
	// No need to save
	RefreshCounter refresh;
	Trigger clockTrigger;
	float addCV;
	bool sourceExMachina;
	float trigLights[2] = {1.0f, 1.0f};// white, blue


	inline bool isAlteredFate() {return (random::uniform() < (params[FREEWILL_PARAM].getValue() + inputs[FREEWILL_INPUT].getVoltage() / 10.0f));}// randomUniform is [0.0, 1.0)

	
	Fate() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(Fate::FREEWILL_PARAM, 0.0f, 1.0f, 0.0f, "Free will");
		configParam(Fate::CHOICESDEPTH_PARAM, 0.0f, 1.0f, 0.5f, "Choices depth");

		onReset();

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		addCV = 0.0f;
		sourceExMachina = false;
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
		
		addCV = 0.0f;
		sourceExMachina = false;
	}

	void process(const ProcessArgs &args) override {		
		// user inputs
		//if (refresh.processInputs()) {
			// none
		//}// userInputs refresh
		
		
		// clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if (isAlteredFate()) {
				float choicesDepthCVinput = 0.0f;
				if (inputs[CHOICSDEPTH_INPUT].isConnected()) {
					choicesDepthCVinput = inputs[CHOICSDEPTH_INPUT].getVoltage() / 10.0f;
				}
				addCV = (random::uniform() * 10.0f - 5.0f);
				addCV *= clamp(params[CHOICESDEPTH_PARAM].getValue() + choicesDepthCVinput, 0.0f, 1.0f);
				trigLights[1] = 1.0f;
				sourceExMachina = inputs[EXMACHINA_INPUT].isConnected();
			}
			else {
				addCV = 0.0f;
				trigLights[0] = 1.0f;
				sourceExMachina = false;
			}
		}
		
		// output
		float inputVoltage = sourceExMachina ? inputs[EXMACHINA_INPUT].getVoltage() : inputs[MAIN_INPUT].getVoltage();
		outputs[MAIN_OUTPUT].setVoltage(inputVoltage + addCV);

		// lights
		if (refresh.processLights()) {
			float deltaTime = args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2);
			
			lights[TRIG_LIGHT + 0].setSmoothBrightness(trigLights[0], deltaTime);
			lights[TRIG_LIGHT + 1].setSmoothBrightness(trigLights[1], deltaTime);
			trigLights[0] = 0.0f;
			trigLights[1] = 0.0f;
		}// lightRefreshCounter
	}// step()
};


struct FateWidget : ModuleWidget {
	FateWidget(Fate *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Fate-DM.svg")));

		// Screws 
		// part of svg panel, no code required
	
		float colRulerCenter = box.size.x / 2.0f;
		float offsetX = 16.0f;

		// free will knob and cv input
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, 380 - 326), module, Fate::FREEWILL_PARAM, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380 - 290), true, module, Fate::FREEWILL_INPUT, module ? &module->panelTheme : NULL));
		
		// choice trigger input and light (clock)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetX, 380 - 254), true, module, Fate::CLOCK_INPUT, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteBlueLight>>(Vec(colRulerCenter - offsetX, 380.0f - 200.5f), module, Fate::TRIG_LIGHT));

		// main output and input
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetX, 380 - 232), false, module, Fate::MAIN_OUTPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetX, 380.0f - 145.5f), true, module, Fate::MAIN_INPUT, module ? &module->panelTheme : NULL));
		
		// ex machina input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetX, 380.0f - 123.5f), true, module, Fate::EXMACHINA_INPUT, module ? &module->panelTheme : NULL));
		
		// choices depth cv input and knob
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380.0f - 87.5f), true, module, Fate::CHOICSDEPTH_INPUT, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, 380.0f - 51.5f), module, Fate::CHOICESDEPTH_PARAM, module ? &module->panelTheme : NULL));
	}
};

Model *modelFate = createModel<Fate, FateWidget>("Fate");

/*CHANGE LOG

*/