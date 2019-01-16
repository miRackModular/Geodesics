//***********************************************************************************************
//Dual State Multi Source Sequencer module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Uncertainties : Module {
	enum ParamIds {
		RUN_PARAM,
		STEPCLOCKS_PARAM,// magnetic clock
		RESET_PARAM,
		RESETONRUN_PARAM,
		LENGTH_PARAM,
		ENUMS(CV_PARAMS, 16),// first 8 are blue, last 8 are yellow
		ENUMS(PROB_PARAMS, 8),// prob knobs
		ENUMS(OCT_PARAMS, 2),// energy
		ENUMS(QUANTIZE_PARAM, 2),// plank
		ENUMS(STATE_PARAMS, 2),// state switch
		ENUMS(FIXEDCV_PARAMS, 2),
		ENUMS(EXTSIG_PARAMS, 2),
		ENUMS(RANDOM_PARAMS, 2),
		GPROB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CERTAIN_CLK_INPUT,
		UNCERTAIN_CLK_INPUT,
		LENGTH_INPUT,
		RUN_INPUT,
		RESET_INPUT,
		ENUMS(STATE_INPUTS, 2),
		ENUMS(OCT_INPUTS, 2),
		ENUMS(EXTSIG_INPUTS, 2),
		GPROB_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,// main output
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 16),// first 8 are blue, last 8 are yellow
		CV_LIGHT,// main output
		RUN_LIGHT,
		STEPCLOCKS_LIGHT,
		RESET_LIGHT,
		RESETONRUN_LIGHT,
		ENUMS(LENGTH_LIGHTS, 8),
		ENUMS(STATE_LIGHTS, 2),
		ADD_LIGHT,
		ENUMS(QUANTIZE_LIGHTS, 2),
		ENUMS(ENERGY_LIGHTS, 10),// first 5 are blue, last 5 are yellow
		ENUMS(FIXEDCV_LIGHTS, 2),
		ENUMS(EXTSIG_LIGHTS, 2),
		ENUMS(RANDOM_LIGHTS, 2),
		OUTPUT_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
							  
	// Need to save, with reset
	int panelTheme = 0;
	bool running;
	bool resetOnRun;
	
	
	// No need to save
	long clockIgnoreOnReset;
	float resetLight;
	unsigned int lightRefreshCounter = 0;
	SchmittTrigger runningTrigger;
	SchmittTrigger certainClockTrigger;
	SchmittTrigger uncertainClockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger resetOnRunTrigger;
	
	inline float quantizeCV(float cv) {return roundf(cv * 12.0f) / 12.0f;}
	
	
	Uncertainties() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	
	void onReset() override {
		running = false;
		resetOnRun = false;
		initRun(true);
	}

	
	void onRandomize() override {
		initRun(true);
	}
	

	void initRun(bool hard) {// run button activated or run edge in run input jack
		if (hard) {

		}
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		resetLight = 0.0f;
	}
	

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));

		return rootJ;
	}

	
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		initRun(true);
	}

	
	void step() override {	
		float sampleTime = engineGetSampleTime();
	
		//********** Buttons, knobs, switches and inputs **********
	
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running)
				initRun(resetOnRun);
		}
		
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {


			// Reset on Run button
			if (resetOnRunTrigger.process(params[RESETONRUN_PARAM].value)) {
				resetOnRun = !resetOnRun;
			}

		}// userInputs refresh
		

		//********** Clock and reset **********
		
		// Clocks
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true);
			resetLight = 1.0f;
			certainClockTrigger.reset();
			uncertainClockTrigger.reset();
		}
		
		
		//********** Outputs and lights **********

		// Output
		outputs[CV_OUTPUT].value = 0.0f;
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * sampleTime * displayRefreshStepSkips;	
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

		}// lightRefreshCounter
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
	
};


struct UncertaintiesWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Uncertainties *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Uncertainties *module = dynamic_cast<Uncertainties*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// Geodesics.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// Geodesics.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);

		return menu;
	}	
	
	UncertaintiesWidget(Uncertainties *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/WhiteLight/Uncertainties-WL.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/DarkMatter/Uncertainties-DM.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = 157.5f;//box.size.x / 2.0f;
		float rowRulerOutput = 380.0f - 155.5f;
		static constexpr float radius1 = 50.0f;
		static constexpr float offset1 = 35.5f;
		static constexpr float radius3 = 105.0f;
		static constexpr float offset3 = 74.5f;
		static constexpr float offset2b = 74.5f;// big
		static constexpr float offset2s = 27.5f;// small
		
		
		// CV out and light
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerOutput), Port::OUTPUT, module, Uncertainties::CV_OUTPUT, &module->panelTheme));		
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter, rowRulerOutput - 21.5f), module, Uncertainties::CV_LIGHT));
		
		// Blue CV knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput - radius1), module, Uncertainties::CV_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset1, rowRulerOutput - offset1), module, Uncertainties::CV_PARAMS + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius1, rowRulerOutput), module, Uncertainties::CV_PARAMS + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset1, rowRulerOutput + offset1), module, Uncertainties::CV_PARAMS + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput + radius1), module, Uncertainties::CV_PARAMS + 4, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset1, rowRulerOutput + offset1), module, Uncertainties::CV_PARAMS + 5, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius1, rowRulerOutput), module, Uncertainties::CV_PARAMS + 6, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset1, rowRulerOutput - offset1), module, Uncertainties::CV_PARAMS + 7, 0.0f, 1.0f, 0.5f, &module->panelTheme));

		// Yellow CV knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput - radius3), module, Uncertainties::CV_PARAMS + 8 + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerOutput - offset3), module, Uncertainties::CV_PARAMS + 8 + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerOutput), module, Uncertainties::CV_PARAMS + 8 + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerOutput + offset3), module, Uncertainties::CV_PARAMS + 8 + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput + radius3), module, Uncertainties::CV_PARAMS + 8 + 4, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerOutput + offset3), module, Uncertainties::CV_PARAMS + 8 + 5, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerOutput), module, Uncertainties::CV_PARAMS + 8 + 6, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerOutput - offset3), module, Uncertainties::CV_PARAMS + 8 + 7, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		
		// Prob CV knobs
		addParam(createDynamicParam<GeoKnobRight>(Vec(colRulerCenter + offset2s, rowRulerOutput - offset2b), module, Uncertainties::PROB_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBotRight>(Vec(colRulerCenter + offset2b, rowRulerOutput - offset2s), module, Uncertainties::PROB_PARAMS + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBottom>(Vec(colRulerCenter + offset2b, rowRulerOutput + offset2s), module, Uncertainties::PROB_PARAMS + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBotLeft>(Vec(colRulerCenter + offset2s, rowRulerOutput + offset2b), module, Uncertainties::PROB_PARAMS + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		// TODO fix above 4 positions, do remaining 4
		
	}
};

Model *modelUncertainties = Model::create<Uncertainties, UncertaintiesWidget>("Geodesics", "Uncertainties", "Uncertainties", SEQUENCER_TAG);

/*CHANGE LOG

0.6.5:
created

*/